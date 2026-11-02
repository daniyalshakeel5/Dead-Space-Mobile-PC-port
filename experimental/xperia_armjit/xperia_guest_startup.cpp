#include "guest_elf32.hpp"
#include "guest_libc_bridge.hpp"
#include "guest_heap.hpp"
#include "guest_filesystem.hpp"
#include "guest_arm_call_runner.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <vector>

// Execute original user-supplied ARM constructor code only when every
// relocation can be bound to an implemented guest service. No stubs.
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
        std::map<std::string,unsigned> missing;
        for(const auto& relocation:elf.pending_imports)
            if(!services.symbols().count(relocation.name))
                ++missing[relocation.name];
        if(!missing.empty()){
            std::cerr<<"XPERIA_STARTUP_BLOCKED: "<<missing.size()
                     <<" Android/JNI/GLES import symbols not implemented:\n";
            for(const auto& item:missing)
                std::cerr<<"  "<<item.first<<" ("<<item.second<<" relocations)\n";
            return 4;
        }
        if(!elf.bind_imports(services.symbols(),error))
            throw std::runtime_error("ARM relocation binding failed: "+error);
        xperia::GuestHeap heap(elf);
        xperia::GuestFileSystem files(root);
        xperia::GuestArmCallRunner guest(elf,services,heap,&files);
        std::cerr<<"Executing "<<elf.initialization_order.size()
                 <<" ORIGINAL Xperia ARM ELF constructors via Dynarmic.\n";
        if(!guest.run_original_initializers(error))
            throw std::runtime_error("Original ARM constructor failed: "+error);
        std::cerr<<"ORIGINAL_ARM_CONSTRUCTORS_COMPLETE\n";
        // An actual Android JavaVM/JNIEnv and JNI_OnLoad are still required.
        // Do not invoke JNI_OnLoad with forged pointers or mark the game booted.
        std::cerr<<"XPERIA_STARTUP_BLOCKED: JNI JavaVM and Android services"
                    " not implemented; game not launched.\n";
        return 5;
    }catch(const std::exception& e){
        std::cerr<<"XPERIA_STARTUP_FAILED: "<<e.what()<<"\n";
        return 2;
    }
}
