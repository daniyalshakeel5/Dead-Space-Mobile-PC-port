#ifdef NDEBUG
#undef NDEBUG
#endif
#include "guest_jni_vm.hpp"
#include <cassert>
#include <cstdint>
#include <string>
#include <cstring>
#include <vector>
using namespace xperia;
int main(){
    GuestElf32 elf;
    GuestJniVm vm;
    std::string error;
    elf.jni_exports["Java_com_ea_EAIO_EAIO_Startup"]=0x10001100u;
    assert(vm.install(elf,error));
    assert(!vm.install(elf,error));
    assert(elf.read_u32(vm.vm())==vm.table_address);
    assert(elf.read_u32(vm.env_address)==vm.env_table_address);
    std::uint8_t byte=0;
    const auto getenv_ptr=elf.read_u32(vm.table_address+6*4);
    assert(vm.code_byte(getenv_ptr,byte) && byte==6);
    assert(vm.code_byte(getenv_ptr+3,byte) && byte==0xef);
    const auto version_ptr=elf.read_u32(vm.env_table_address+4*4);
    assert(version_ptr!=0 && vm.code_byte(version_ptr,byte));
    elf.segments.push_back({0x10001000u,std::vector<std::uint8_t>(4096,0)});
    std::array<std::uint32_t,16> r{};
    r[0]=vm.vm();r[1]=0x10001100;r[2]=0x00010006u;
    assert(vm.dispatch(vm.vm_svc_base+6,r,elf,error));
    assert(r[0]==0 && elf.read_u32(0x10001100)==vm.env_address);
    r[0]=vm.vm();r[1]=0x10001100;r[2]=0x00010009u;
    assert(vm.dispatch(vm.vm_svc_base+6,r,elf,error));
    assert(static_cast<std::int32_t>(r[0])==-3 && elf.read_u32(0x10001100)==0);
    r[0]=vm.env_address;
    assert(vm.dispatch(vm.env_svc_base+4,r,elf,error) && r[0]==0x00010006u);
    r[0]=vm.env_address;
    assert(!vm.dispatch(vm.env_svc_base+6,r,elf,error));
    assert(error.find("6")!=std::string::npos);
    const auto write_string=[&](std::uint32_t at,const char* value){
        auto* dst=elf.writeable(at,std::strlen(value)+1);
        assert(dst);
        std::memcpy(dst,value,std::strlen(value)+1);
    };
    constexpr std::uint32_t base=0x10001000u;
    write_string(base+0x200,"com/eamobile/Query");
    r.fill(0);r[0]=vm.env_address;r[1]=base+0x200;
    assert(vm.dispatch(vm.env_svc_base+6,r,elf,error));
    const auto query_class=r[0];
    assert(query_class!=0);
    write_string(base+0x230,"isContentReady");
    write_string(base+0x250,"()Z");
    r.fill(0);r[0]=vm.env_address;r[1]=query_class;
    r[2]=base+0x230;r[3]=base+0x250;
    assert(vm.dispatch(vm.env_svc_base+113,r,elf,error));
    const auto ready_method=r[0];
    r.fill(0);r[0]=vm.env_address;r[1]=query_class;r[2]=ready_method;
    assert(vm.dispatch(vm.env_svc_base+117,r,elf,error));
    assert(r[0]==0); // no actual installed content yet
    vm.set_content_ready(true);
    r[0]=vm.env_address;r[1]=query_class;r[2]=ready_method;
    assert(vm.dispatch(vm.env_svc_base+117,r,elf,error)&&r[0]==1);
    write_string(base+0x270,"getVersion");
    write_string(base+0x290,"()Ljava/lang/String;");
    r.fill(0);r[0]=vm.env_address;r[1]=query_class;
    r[2]=base+0x270;r[3]=base+0x290;
    assert(vm.dispatch(vm.env_svc_base+113,r,elf,error));
    const auto version_method=r[0];
    r.fill(0);r[0]=vm.env_address;r[1]=query_class;r[2]=version_method;
    assert(vm.dispatch(vm.env_svc_base+114,r,elf,error));
    const auto str=r[0];
    r.fill(0);r[0]=vm.env_address;r[1]=str;
    assert(vm.dispatch(vm.env_svc_base+169,r,elf,error));
    assert(std::strcmp(reinterpret_cast<const char*>(elf.read(r[0],7)),
                       "1.1.33")==0);
    write_string(base+0x300,"com/ea/EAIO/EAIO");
    r.fill(0);r[0]=vm.env_address;r[1]=base+0x300;
    assert(vm.dispatch(vm.env_svc_base+6,r,elf,error));
    const auto eaio_class=r[0];
    write_string(base+0x340,"Startup");
    write_string(base+0x370,"(Landroid/content/res/AssetManager;)V");
    r.fill(0);r[0]=vm.env_address;r[1]=eaio_class;r[2]=base+0x340;r[3]=base+0x370;
    assert(vm.dispatch(vm.env_svc_base+113,r,elf,error));
    const auto startup_method=r[0];
    r.fill(0);r[0]=vm.env_address;r[1]=eaio_class;r[2]=startup_method;r[3]=0xfaceu;
    assert(vm.dispatch(vm.env_svc_base+141,r,elf,error));
    auto callback=vm.take_callback();
    assert(callback.address==0x10001100u &&
           callback.arguments[0]==vm.env_address &&
           callback.arguments[2]==0xfaceu);
    assert(vm.take_callback().address==0);
    struct AudioProbe { unsigned bytes=0,actions=0; } probe;
    vm.set_audio_sink({&probe,
        [](void* context,const std::uint8_t* data,std::size_t bytes,
           std::string& failure)->bool {
            if(!data||!bytes){failure="PCM missing";return false;}
            auto& p=*static_cast<AudioProbe*>(context);
            p.bytes+=static_cast<unsigned>(bytes);
            failure.clear();return true;
        },
        [](void* context,unsigned action,std::string& failure)->bool {
            if(action>2){failure="Audio command unknown";return false;}
            auto& p=*static_cast<AudioProbe*>(context);
            ++p.actions;failure.clear();return true;
        }});
    r.fill(0);r[0]=vm.env_address;r[1]=vm.audio_track_address;
    assert(vm.dispatch(vm.env_svc_base+31,r,elf,error));
    const auto audio_class=r[0];
    assert(audio_class!=0u);
    write_string(base+0x400,"write");
    write_string(base+0x420,"([SII)I");
    r.fill(0);r[0]=vm.env_address;r[1]=audio_class;
    r[2]=base+0x400;r[3]=base+0x420;
    assert(vm.dispatch(vm.env_svc_base+33,r,elf,error));
    const auto write_id=r[0];
    r.fill(0);r[0]=vm.env_address;r[1]=8;
    assert(vm.dispatch(vm.env_svc_base+178,r,elf,error));
    const auto arr=r[0];
    assert(arr>=0x74600000u);
    r.fill(0);r[0]=vm.env_address;r[1]=arr;
    assert(vm.dispatch(vm.env_svc_base+186,r,elf,error));
    auto* samples=elf.writeable(r[0],16u);
    assert(samples);
    for(unsigned i=0;i<16u;++i)samples[i]=static_cast<std::uint8_t>(i);
    r.fill(0);r[0]=vm.env_address;r[1]=arr;
    assert(vm.dispatch(vm.env_svc_base+171,r,elf,error)&&r[0]==8);
    // Two JNI varargs after jshortArray travel on the ARM AAPCS stack.
    const auto sp=base+0xa00;
    auto* stack=elf.writeable(sp,8);
    assert(stack);
    std::memset(stack,0,8);
    stack[4]=8; // offset 0, count 8 shorts
    r.fill(0);r[0]=vm.env_address;r[1]=vm.audio_track_address;
    r[2]=write_id;r[3]=arr;r[13]=sp;
    assert(vm.dispatch(vm.env_svc_base+49,r,elf,error)&&r[0]==8);
    assert(probe.bytes==16u);
    stack[4]=9; // invalid count must never access guest memory
    r.fill(0);r[0]=vm.env_address;r[1]=vm.audio_track_address;
    r[2]=write_id;r[3]=arr;r[13]=sp;
    assert(!vm.dispatch(vm.env_svc_base+49,r,elf,error));
    write_string(base+0x400,"play");
    write_string(base+0x420,"()V");
    r.fill(0);r[0]=vm.env_address;r[1]=audio_class;
    r[2]=base+0x400;r[3]=base+0x420;
    assert(vm.dispatch(vm.env_svc_base+33,r,elf,error));
    const auto play_id=r[0];
    r.fill(0);r[0]=vm.env_address;r[1]=vm.audio_track_address;r[2]=play_id;
    assert(vm.dispatch(vm.env_svc_base+61,r,elf,error));
    assert(probe.actions==1u);
    // Device and storage Java delegate methods are original Xperia
    // class/method signatures, not ad hoc host property guesses.
    const auto find_class=[&](const char* name)->std::uint32_t {
        write_string(base+0x500,name);
        r.fill(0);r[0]=vm.env_address;r[1]=base+0x500;
        assert(vm.dispatch(vm.env_svc_base+6,r,elf,error));
        return r[0];
    };
    const auto get_instance_method=[&](std::uint32_t clazz,
            const char* name,const char* sig)->std::uint32_t {
        write_string(base+0x540,name);
        write_string(base+0x600,sig);
        r.fill(0);r[0]=vm.env_address;r[1]=clazz;
        r[2]=base+0x540;r[3]=base+0x600;
        assert(vm.dispatch(vm.env_svc_base+33,r,elf,error));
        return r[0];
    };
    const auto new_instance=[&](std::uint32_t clazz)->std::uint32_t {
        const auto init=get_instance_method(clazz,"<init>","()V");
        r.fill(0);r[0]=vm.env_address;r[1]=clazz;r[2]=init;
        assert(vm.dispatch(vm.env_svc_base+28,r,elf,error));
        return r[0];
    };
    const auto read_java_string=[&](std::uint32_t ref)->std::string {
        r.fill(0);r[0]=vm.env_address;r[1]=ref;
        assert(vm.dispatch(vm.env_svc_base+169,r,elf,error));
        const auto* data=elf.read(r[0],255);
        assert(data);
        return reinterpret_cast<const char*>(data);
    };
    const auto system=find_class("com/ea/blast/SystemAndroidDelegate");
    const auto system_obj=new_instance(system);
    const auto manufacturer=get_instance_method(system,"GetManufacturer",
                                      "()Ljava/lang/String;");
    r.fill(0);r[0]=vm.env_address;r[1]=system_obj;r[2]=manufacturer;
    assert(vm.dispatch(vm.env_svc_base+34,r,elf,error));
    assert(read_java_string(r[0])=="sony");
    const auto device=get_instance_method(system,"GetDeviceName",
                                      "()Ljava/lang/String;");
    r.fill(0);r[0]=vm.env_address;r[1]=system_obj;r[2]=device;
    assert(vm.dispatch(vm.env_svc_base+34,r,elf,error));
    assert(read_java_string(r[0])=="R800");
    const auto display=find_class("com/ea/blast/DisplayAndroidDelegate");
    const auto display_obj=new_instance(display);
    const auto width=get_instance_method(display,"GetDefaultWidth","()I");
    r.fill(0);r[0]=vm.env_address;r[1]=display_obj;r[2]=width;
    assert(vm.dispatch(vm.env_svc_base+49,r,elf,error)&&r[0]==544);
    const auto height=get_instance_method(display,"GetDefaultHeight","()I");
    r.fill(0);r[0]=vm.env_address;r[1]=display_obj;r[2]=height;
    assert(vm.dispatch(vm.env_svc_base+49,r,elf,error)&&r[0]==960);
    const auto storage=find_class("com/ea/blast/GetAppDataDirectoryDelegate");
    const auto storage_obj=new_instance(storage);
    const auto data_dir=get_instance_method(storage,"GetAppDataDirectory",
                                          "()Ljava/lang/String;");
    r.fill(0);r[0]=vm.env_address;r[1]=storage_obj;r[2]=data_dir;
    assert(vm.dispatch(vm.env_svc_base+34,r,elf,error));
    assert(read_java_string(r[0])=="deadspace");
    write_string(base+0x300,"com/ea/UNKNOWN");
    r.fill(0);r[0]=vm.env_address;r[1]=base+0x300;
    assert(!vm.dispatch(vm.env_svc_base+6,r,elf,error));
    assert(error.find("com/ea/UNKNOWN")!=std::string::npos);
    r[0]=0x10001000;
    assert(!vm.dispatch(vm.vm_svc_base+6,r,elf,error));
    return 0;
}
