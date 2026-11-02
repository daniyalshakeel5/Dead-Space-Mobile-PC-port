#ifdef NDEBUG
#undef NDEBUG
#endif
#include "guest_filesystem.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
namespace fs=std::filesystem;
using xperia::GuestFileSystem;
int main(){
    auto root=fs::temp_directory_path()/"xperia_guest_asset_synthetic";
    fs::remove_all(root);
    fs::create_directories(root/"assets/published/data");
    fs::create_directories(root/"outside");
    const auto asset=root/"assets/published/data/level_index.sb";
    const auto secret=root/"outside/private.txt";
    {std::ofstream o(asset,std::ios::binary);o<<"XRPGdata";}
    {std::ofstream o(secret,std::ios::binary);o<<"SECRET";}
    GuestFileSystem fsys(root);
    assert(fsys.handle_count()==0);
    const int fd=fsys.open_readonly("assets/published/data/level_index.sb",0);
    assert(fd>=3&&fsys.handle_count()==1);
    std::array<char,16> data{};
    assert(fsys.read(fd,data.data(),4)==4);
    assert(std::memcmp(data.data(),"XRPG",4)==0);
    assert(fsys.seek(fd,0,SEEK_SET)==0);
    assert(fsys.read(fd,data.data(),8)==8);
    assert(std::memcmp(data.data(),"XRPGdata",8)==0);
    assert(fsys.read(fd,data.data(),8)==0); // actual EOF
    assert(fsys.seek(fd,-4,SEEK_END)==4);
    assert(fsys.read(fd,data.data(),4)==4);
    assert(std::memcmp(data.data(),"data",4)==0);
    assert(fsys.close(fd)==0&&fsys.handle_count()==0);
    assert(fsys.close(fd)==-1); // double close
    assert(fsys.read(fd,data.data(),4)==-1);
    const int absolute_fd=fsys.open_readonly("/assets/published/data/level_index.sb",0);
    assert(absolute_fd>=3);
    assert(fsys.close(absolute_fd)==0);
    const auto verify_alias=[&fsys](const char* alias){
        const int handle=fsys.open_readonly(alias,0);
        assert(handle>=3);
        char payload[8]={};
        assert(fsys.read(handle,payload,sizeof(payload))==8);
        assert(std::memcmp(payload,"XRPGdata",8)==0);
        assert(fsys.close(handle)==0);
    };
    verify_alias("appbundle:/published/data/level_index.sb");
    verify_alias("deadspace/published/data/level_index.sb");
    verify_alias("published/data/level_index.sb");
    assert(fsys.open_readonly("appbundle:/../../outside/private.txt",0)==-1);
    assert(fsys.open_readonly("appbundle:/published/../../../outside/private.txt",0)==-1);
    assert(fsys.open_readonly("appbundle://etc/passwd",0)==-1);
    assert(fsys.open_readonly("other:/published/data/level_index.sb",0)==-1);
    assert(fsys.open_readonly("assets/../outside/private.txt",0)==-1);
    assert(fsys.open_readonly("assets/published/data/../../outside/private.txt",0)==-1);
    assert(fsys.open_readonly("C:\\Windows\\win.ini",0)==-1);
    assert(fsys.open_readonly("/etc/passwd",0)==-1);
    assert(fsys.open_readonly("assets/published/data/level_index.sb",1)==-1);
    assert(fsys.open_readonly("assets/published/data/level_index.sb",64)==-1);
    assert(fsys.open_readonly("assets/nonexistent.sb",0)==-1);
    assert(fsys.read(fd,data.data(),GuestFileSystem::max_io+1)==-1);
    // A symlink inside the game folder must not escape its sandbox.
    std::error_code ec;
    fs::create_symlink(secret,root/"assets/link.txt",ec);
    if(!ec)assert(fsys.open_readonly("assets/link.txt",0)==-1);
    fs::remove_all(root);
    return 0;
}
