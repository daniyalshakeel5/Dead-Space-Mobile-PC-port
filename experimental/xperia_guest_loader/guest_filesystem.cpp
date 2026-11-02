#include "guest_filesystem.hpp"
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
namespace xperia {
namespace fs=std::filesystem;
namespace {
bool inside(const fs::path& root,const fs::path& child) {
    auto r=root.begin(),p=child.begin();
    for(;r!=root.end();++r,++p) {
        if(p==child.end()||*r!=*p)return false;
    }
    return p!=child.end(); // strict descendant, not root itself
}
}
GuestFileSystem::GuestFileSystem(const fs::path& install_root)
    :root_(fs::canonical(install_root)) {
    if(!fs::is_directory(root_/"assets"))
        throw fs::filesystem_error("Xperia install has no assets directory",
                                   root_,std::make_error_code(std::errc::no_such_file_or_directory));
}
GuestFileSystem::~GuestFileSystem(){
    for(auto& h:handles_)std::fclose(h.second);
}
bool GuestFileSystem::resolve_asset(const std::string& name,fs::path& out) const {
    try {
        if(name.empty()||name.size()>4096||name.find('\\')!=std::string::npos||
           name.find(':')!=std::string::npos||name.find('\0')!=std::string::npos)
            return false;
        std::string path=name;
        if(path.rfind("/assets/",0)==0)path.erase(0,1);
        if(path.rfind("assets/",0)!=0)return false;
        // Only the exact original assets subtree is allowed. No device,
        // network, symlink escape, arbitrary absolute or traversal paths.
        fs::path relative;
        for(const auto& part:fs::path(path)){
            if(part==".."||part=="."||part.empty())return false;
            relative/=part;
        }
        out=fs::canonical(root_/relative);
        const auto assets=fs::canonical(root_/"assets");
        return inside(assets,out)&&fs::is_regular_file(out);
    }catch(const fs::filesystem_error&){return false;}
}
int GuestFileSystem::open_readonly(const std::string& name,std::uint32_t flags) noexcept {
    // Linux ARM O_RDONLY is 0. Reject O_CREAT, append, writable handles,
    // directories and unsupported modes. No save-files yet.
    if(flags!=0||handles_.size()>=128||next_handle_>=INT_MAX)return -1;
    fs::path candidate;
    if(!resolve_asset(name,candidate))return -1;
#ifdef _WIN32
    std::FILE* file=nullptr;
    if(_wfopen_s(&file,candidate.c_str(),L"rb")!=0)return -1;
#else
    std::FILE* file=std::fopen(candidate.c_str(),"rb");
    if(!file)return -1;
#endif
    try {
        const int fd=next_handle_++;
        handles_.emplace(fd,file);
        return fd;
    }catch(...){std::fclose(file);return -1;}
}
std::int32_t GuestFileSystem::read(int fd,void* dst,std::size_t n) noexcept {
    const auto it=handles_.find(fd);
    if(it==handles_.end()||!dst||n>max_io)return -1;
    const auto transferred=std::fread(dst,1,n,it->second);
    if(transferred<n && std::ferror(it->second))return -1;
    return static_cast<std::int32_t>(transferred);
}
std::int32_t GuestFileSystem::seek(int fd,std::int32_t offset,int whence) noexcept {
    const auto it=handles_.find(fd);
    if(it==handles_.end()||(whence!=SEEK_SET&&whence!=SEEK_CUR&&whence!=SEEK_END))
        return -1;
    if(std::fseek(it->second,static_cast<long>(offset),whence)!=0)return -1;
    const auto position=std::ftell(it->second);
    return position<0||position>INT32_MAX?-1:static_cast<std::int32_t>(position);
}
int GuestFileSystem::close(int fd) noexcept {
    const auto it=handles_.find(fd);
    if(it==handles_.end())return -1;
    std::FILE* file=it->second;
    handles_.erase(it);
    return std::fclose(file)==0?0:-1;
}
} // namespace xperia
