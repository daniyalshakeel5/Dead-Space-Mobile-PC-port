#include "guest_elf32.hpp"
#include "guest_libc_bridge.hpp"
#include "guest_heap.hpp"
#include "guest_filesystem.hpp"
#include "guest_arm_call_runner.hpp"
#include "guest_jni_vm.hpp"
#include "guest_xperia_input.hpp"
#ifdef _WIN32
#include "guest_sdl_host.hpp"
#endif
#include "guest_bionic_data.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <vector>

// Execute original user-supplied ARM ELF constructors with no-success
// traps for unimplemented imported functions. Never fake Android services.
int main(int argc,char** argv){
    if(argc!=3){
        std::cerr<<"Usage: xperia_guest_startup <installed libEAMGameDeadSpace.so> <installed root>\n";
        return 2;
    }
    try {
        namespace fs=std::filesystem;
        const fs::path so(argv[1]),root(argv[2]);
        if(!fs::is_regular_file(so)||fs::file_size(so)!=4499468u ||
           !fs::is_directory(root))
            throw std::runtime_error("Missing or wrong-version original Xperia game installation");
        std::ifstream input(so,std::ios::binary);
        if(!input)throw std::runtime_error("Cannot open original ARM library");
        std::vector<std::uint8_t> data(
            (std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        if(data.size()!=4499468u)
            throw std::runtime_error("Original ELF read was incomplete");
        xperia::GuestElf32 elf;
        std::string error;
        if(!elf.open(data,error))
            throw std::runtime_error("ARM ELF load failed: "+error);
        xperia::GuestLibcBridge services;
        xperia::GuestBionicData bionic;
        if(!bionic.install(elf,error))
            throw std::runtime_error("Bionic guest data allocation failed: "+error);
        // Function imports not needed by early constructors become distinct
        // error traps. Imported data and ambiguous symbols stay blocked.
#ifdef _WIN32
        xperia::GuestSdlHost platform;
        if(!platform.start(error,960,544))
            throw std::runtime_error("Windows SDL OpenGL host failed: "+error);
        auto planned=services.plan_guest_imports(elf,error,bionic.symbols(),
                                                  platform.symbols());
#else
        auto planned=services.plan_guest_imports(elf,error,bionic.symbols());
#endif
        if(planned.empty()) {
            std::cerr<<"XPERIA_STARTUP_BLOCKED: "<<error<<"\n";
            return 4;
        }
        std::cerr<<"Original ARM imports: "<<services.symbols().size()
                 <<" implemented host services, "
                 <<services.unresolved_import_count()
                 <<" unimplemented functions bound to named failure traps.\n";
        if(!elf.bind_imports(planned,error))
            throw std::runtime_error("ARM relocation binding failed: "+error);
        xperia::GuestHeap heap(elf);
        xperia::GuestFileSystem files(root);
        xperia::GuestJniVm java;
#ifdef _WIN32
        java.set_audio_sink({&platform,
            [](void* ctx,const std::uint8_t* data,std::size_t count,
               std::string& message)->bool {
                return static_cast<xperia::GuestSdlHost*>(ctx)
                    ->queue_pcm16(data,count,message);
            },
            [](void* ctx,unsigned action,std::string& message)->bool {
                return static_cast<xperia::GuestSdlHost*>(ctx)
                    ->audio_control(action,message);
            }});
#endif
        java.set_content_ready(true); // verified installed assets/ mount
        if(!java.install(elf,error))
            throw std::runtime_error("ARM32 JNI guest environment failed: "+error);
#ifdef _WIN32
        xperia::GuestArmCallRunner guest(elf,services,heap,&files,&java,&platform);
#else
        xperia::GuestArmCallRunner guest(elf,services,heap,&files,&java);
#endif
        std::cerr<<"Executing "<<elf.initialization_order.size()
                 <<" ORIGINAL Xperia ARM ELF constructors via Dynarmic.\n";
        if(!guest.run_original_initializers(error))
            throw std::runtime_error("Original ARM constructor failed: "+error);
        std::cerr<<"ORIGINAL_ARM_CONSTRUCTORS_COMPLETE\n";
        const auto entry=elf.jni_exports.find("JNI_OnLoad");
        if(entry==elf.jni_exports.end())
            throw std::runtime_error("Original Xperia ELF missing JNI_OnLoad export");
        std::uint32_t version=0;
        std::cerr<<"Invoking ORIGINAL Xperia JNI_OnLoad through ARM32 JavaVM.\\n";
        if(!guest.invoke(entry->second,{java.vm(),0,0,0},version,error))
            throw std::runtime_error("Original JNI_OnLoad stopped: "+error);
        if(version!=0x00010002u && version!=0x00010004u &&
           version!=0x00010006u)
            throw std::runtime_error("Original JNI_OnLoad returned unsupported version");
        std::cerr<<"ORIGINAL_XPERIA_JNI_ONLOAD_COMPLETE: 0x"
                 <<std::hex<<version<<std::dec<<"\\n";
#ifdef _WIN32
        const auto required=[&](const char* name)->std::uint32_t {
            const auto it=elf.jni_exports.find(name);
            if(it==elf.jni_exports.end()||!it->second)
                throw std::runtime_error(
                    std::string("Original Xperia ARM JNI export missing: ")+name);
            return it->second;
        };
        // Actual initialization order from the Xperia-compatible Vita
        // loader: audio before NativeOnCreate, then surface then draw.
        const auto audio=required("Java_com_ea_EAAudioCore_AndroidEAAudioCore_Init");
        std::uint32_t ignored=0;
        std::cerr<<"ORIGINAL_XPERIA_AUDIO_INIT_BEGIN\\n";
        if(!guest.invoke_aapcs(audio,
             {xperia::GuestJniVm::env_address,0,
              xperia::GuestJniVm::audio_track_address,
              1024u*1024u,2,44100},ignored,error))
            throw std::runtime_error("Original Xperia audio initialization: "+error);
        std::cerr<<"ORIGINAL_XPERIA_AUDIO_INIT_COMPLETE\\n";
        const auto create=required("Java_com_ea_blast_MainActivity_NativeOnCreate");
        const auto surface=required("Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceCreated");
        const auto frame=required("Java_com_ea_blast_AndroidRenderer_NativeOnDrawFrame");
        if(!guest.invoke(create,{0,0,0,0},ignored,error))
            throw std::runtime_error("Original Xperia NativeOnCreate: "+error);
        std::cerr<<"ORIGINAL_XPERIA_NATIVE_CREATE_COMPLETE\\n";
        if(!guest.run_pending_guest_threads(error))
            throw std::runtime_error("Original Xperia pthread scheduler after create: "+error);
        if(!guest.invoke(surface,{0,0,0,0},ignored,error))
            throw std::runtime_error("Original Xperia NativeOnSurfaceCreated: "+error);
        std::cerr<<"ORIGINAL_XPERIA_SURFACE_CREATED\\n";
        if(!guest.run_pending_guest_threads(error))
            throw std::runtime_error("Original Xperia pthread scheduler after surface: "+error);
        const auto visibility=elf.jni_exports.find(
            "Java_com_ea_blast_KeyboardAndroid_NativeOnVisibilityChanged");
        if(visibility!=elf.jni_exports.end()&&visibility->second){
            if(!guest.invoke(visibility->second,
                {xperia::GuestJniVm::env_address,0,600,1},ignored,error))
                throw std::runtime_error("Original Xperia keyboard visibility: "+error);
        }
        xperia::GuestXperiaInput controls;
        if(!controls.bind(elf,error))
            throw std::runtime_error("Original Xperia controller JNI binding: "+error);
        std::cerr<<"ORIGINAL_XPERIA_GAME_FRAME_LOOP_BEGIN\\n";
        std::uint64_t frames=0;
        bool quit=false;
        while(!quit){
            platform.pump(controls,quit);
            if(quit)break;
            if(!guest.run_pending_guest_threads(error))
                throw std::runtime_error("Original Xperia ARM guest worker: "+error);
            if(!controls.flush(guest,error))
                throw std::runtime_error("Original Xperia controller dispatch: "+error);
            if(!guest.invoke(frame,{0,0,0,0},ignored,error))
                throw std::runtime_error("Original Xperia NativeOnDrawFrame #"+
                                         std::to_string(frames+1)+": "+error);
            platform.swap();
            if(++frames<=5||frames%120==0)
                std::cerr<<"ORIGINAL_XPERIA_FRAMES="<<frames<<"\\n";
        }
        std::cerr<<"ORIGINAL_XPERIA_EXIT frames="<<frames<<"\\n";
        return 0;
#else
        std::cerr<<"XPERIA_STARTUP_BLOCKED: Windows SDL graphics "
                    "and frame host are not available on this platform.\\n";
        return 5;
#endif
    }catch(const std::exception& e){
        std::cerr<<"XPERIA_STARTUP_FAILED: "<<e.what()<<"\n";
        return 2;
    }
}
