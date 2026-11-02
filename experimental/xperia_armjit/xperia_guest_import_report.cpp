// Original-owned ARM library import inventory; no Android emulation or
// copyrighted game code is included in this executable.
#include "guest_elf32.hpp"
#include "guest_libc_bridge.hpp"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <vector>
namespace {
void quote(const std::string& v){
    std::cout<<'"';
    const char* digits="0123456789abcdef";
    for(unsigned char c:v){
        if(c=='"'||c=='\\'){std::cout<<'\\'<<char(c);}
        else if(c=='\n')std::cout<<"\\n";
        else if(c=='\r')std::cout<<"\\r";
        else if(c=='\t')std::cout<<"\\t";
        else if(c<32){std::cout<<"\\u00"<<digits[c>>4]<<digits[c&15];}
        else std::cout<<char(c);
    }
    std::cout<<'"';
}
}
int main(int argc,char** argv){
    if(argc!=2){
        std::cerr<<"Usage: xperia_guest_import_report.exe <private installed Xperia ARM .so>\n";
        return 2;
    }
    try{
        const auto game=std::filesystem::path(argv[1]);
        if(!std::filesystem::is_regular_file(game))
            throw std::runtime_error("No original ARM game library");
        const auto len=std::filesystem::file_size(game);
        if(len!=4499468u)
            throw std::runtime_error("Wrong Xperia library version/size: expected 4499468 bytes");
        std::ifstream in(game,std::ios::binary);
        if(!in)throw std::runtime_error("Cannot open original game library");
        std::vector<std::uint8_t> input((std::istreambuf_iterator<char>(in)),
                                        std::istreambuf_iterator<char>());
        if(input.size()!=len)throw std::runtime_error("Truncated original ARM game library");
        xperia::GuestElf32 image;
        std::string error;
        if(!image.open(input,error))
            throw std::runtime_error("Original ARM ELF rejected: "+error);
        xperia::GuestLibcBridge imports;
        const auto& supported=imports.symbols();
        std::map<std::string,std::size_t> missing,matched;
        for(const auto& rel:image.pending_imports) {
            auto& target=supported.count(rel.name)?matched:missing;
            ++target[rel.name];
        }
        std::cout<<"{\n  \"schema\":1,\n";
        std::cout<<"  \"input\":\"locally-installed-Xperia-ARM32\",\n";
        std::cout<<"  \"actual_game_code_executed\":false,\n";
        std::cout<<"  \"relative_relocations\":"<<image.relative_relocations<<",\n";
        std::cout<<"  \"pending_external_relocations\":"<<image.remaining_relocations<<",\n";
        std::cout<<"  \"supported_import_sites\":";
        std::size_t total=0;
        for(const auto& it:matched)total+=it.second;
        std::cout<<total<<",\n";
        std::cout<<"  \"required_unimplemented_symbols\":"<<missing.size()<<",\n";
        std::cout<<"  \"initializers\":"<<image.initialization_order.size()<<",\n";
        std::cout<<"  \"jni_exports\":{\n";
        std::map<std::string,std::uint32_t> jni(
            image.jni_exports.begin(),image.jni_exports.end());
        std::size_t n=0;
        for(const auto& kv:jni){
            std::cout<<"    ";quote(kv.first);std::cout<<":"<<kv.second;
            std::cout<<(++n<jni.size()?",\n":"\n");
        }
        std::cout<<"  },\n  \"missing_imports\":{\n";
        n=0;
        for(const auto& kv:missing){
            std::cout<<"    ";quote(kv.first);std::cout<<":"<<kv.second;
            std::cout<<(++n<missing.size()?",\n":"\n");
        }
        std::cout<<"  }\n}\n";
        return missing.empty()?0:4; // incomplete imports must NOT appear boot-ready
    }catch(const std::exception& ex){
        std::cerr<<"XPERIA_IMPORT_REPORT_FAILED: "<<ex.what()<<"\n";
        return 2;
    }
}
