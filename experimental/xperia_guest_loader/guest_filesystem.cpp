#include "guest_filesystem.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <filesystem>
#include <string>
namespace xperia {
namespace fs=std::filesystem;
namespace {
bool below(const fs::path& root,const fs::path& path,bool allow_root=false){
    auto prefix=root.begin(),given=path.begin();
    for(;prefix!=root.end();++prefix,++given){
        if(given==path.end()||*prefix!=*given)return false;
    }
    return allow_root||given!=path.end();
}
std::FILE* open_native(const fs::path& path,const char* mode){
#ifdef _WIN32
    std::wstring wide;
    for(const char* p=mode;*p;++p)wide.push_back(wchar_t(*p));
    std::FILE* file=nullptr;
    return _wfopen_s(&file,path.c_str(),wide.c_str())==0?file:nullptr;
#else
    return std::fopen(path.c_str(),mode);
#endif
}
}
GuestFileSystem::GuestFileSystem(const fs::path& installation)
    :root_(fs::canonical(installation)),assets_(fs::canonical(root_/"assets")) {
    if(!fs::is_directory(assets_))
        throw fs::filesystem_error("Xperia original assets/ missing",assets_,
                                   std::make_error_code(std::errc::no_such_file_or_directory));
    // Original ZIP has no assets/published/var. The engine still probes it
    // for save data; this sibling directory is isolated from the game files.
    const auto save_path=root_.parent_path()/"savegames";
    fs::create_directories(save_path);
    saves_=fs::canonical(save_path);
}
GuestFileSystem::~GuestFileSystem(){
    for(const auto& kv:handles_)std::fclose(kv.second.file);
}
bool GuestFileSystem::resolve(const std::string& raw,fs::path& out,
                              bool& is_save) const {
    try{
        is_save=false;
        if(raw.empty()||raw.size()>4096||
           raw.find('\\')!=std::string::npos||raw.find('\0')!=std::string::npos)
            return false;
        std::string name=raw;
        if(name.rfind("appbundle:/",0)==0)
            name=name.substr(sizeof("appbundle:/")-1u);
        else if(name.rfind("deadspace/assets/",0)==0)
            name=name.substr(sizeof("deadspace/assets/")-1u);
        else if(name.rfind("deadspace/",0)==0)
            name=name.substr(sizeof("deadspace/")-1u);
        else if(name.rfind("/assets/",0)==0)
            name=name.substr(sizeof("/assets/")-1u);
        else if(name.rfind("assets/",0)==0)
            name=name.substr(sizeof("assets/")-1u);
        if(name.empty() || name.front()=='/' || name.find(':')!=std::string::npos)
            return false;
        fs::path relative;
        for(const auto& part:fs::path(name)){
            if(part=="."||part==".."||part.empty())return false;
            relative/=part;
        }
        if(name=="published/var"||name=="published/var/"){
            out=saves_;is_save=true;return true;
        }
        const std::string prefix="published/var/";
        if(name.rfind(prefix,0)==0){
            relative=fs::path(name.substr(prefix.size()));
            is_save=true;
        }
        const auto& root=is_save?saves_:assets_;
        out=fs::weakly_canonical(root/relative);
        return below(root,out);
    }catch(const fs::filesystem_error&){return false;}
}
int GuestFileSystem::open_internal(
        const std::string& name,const std::string& mode,
        bool write_allowed) noexcept {
    try {
        if(handles_.size()>=128||next_handle_==INT_MAX)return -1;
        fs::path path;bool save=false;
        if(!resolve(name,path,save))return -1;
        if(write_allowed&&!save)return -1;
        if(fs::is_directory(path))return -1;
        if(!save && !fs::is_regular_file(path))return -1;
        if(save && write_allowed){
            // Only the explicit private save tree can be created or changed.
            const auto parent=fs::weakly_canonical(path.parent_path());
            if(!below(saves_,parent,true))return -1;
            fs::create_directories(parent);
        }
        const auto flags=mode.c_str();
        std::FILE* file=open_native(path,flags);
        if(!file)return -1;
        try{
            const auto id=next_handle_++;
            handles_.emplace(id,Handle{file,write_allowed,path});
            return id;
        }catch(...){std::fclose(file);return -1;}
    }catch(...){return -1;}
}
int GuestFileSystem::open_file(const std::string& path,
                              std::uint32_t flags) noexcept {
    // Android Bionic O_ACCMODE=3; O_CREAT=0x40; O_EXCL=0x80;
    // O_TRUNC=0x200; O_APPEND=0x400. Fail closed on other modes.
    constexpr std::uint32_t allowed=3u|0x40u|0x80u|0x200u|0x400u;
    if(flags&~allowed)return -1;
    const auto access=flags&3u;
    if(access==3u)return -1;
    const bool writable=access!=0;
    if(!writable && flags&~3u)return -1;
    if((flags&0x80u) && !(flags&0x40u))return -1;
    fs::path resolved;bool save=false;
    if(!resolve(path,resolved,save))return -1;
    if(writable&&!save)return -1;
    if((flags&0x80u)&&fs::exists(resolved))return -1;
    if(!writable)return open_internal(path,"rb",false);
    if(flags&0x400u)return open_internal(path,access==2?"ab+":"ab",true);
    if(flags&0x200u)return open_internal(path,access==2?"wb+":"wb",true);
    if((flags&0x40u)&&!fs::exists(resolved))
        return open_internal(path,access==2?"wb+":"wb",true);
    return open_internal(path,"rb+",true);
}
int GuestFileSystem::open_readonly(const std::string& path,
                                  std::uint32_t flags) noexcept{
    return open_file(path,flags);
}
int GuestFileSystem::open_stdio(const std::string& name,
                               const std::string& mode) noexcept {
    if(mode=="r"||mode=="rb")return open_internal(name,"rb",false);
    if(mode=="r+"||mode=="rb+"||mode=="r+b")
        return open_internal(name,"rb+",true);
    if(mode=="w"||mode=="wb")return open_internal(name,"wb",true);
    if(mode=="w+"||mode=="wb+"||mode=="w+b")
        return open_internal(name,"wb+",true);
    if(mode=="a"||mode=="ab")return open_internal(name,"ab",true);
    if(mode=="a+"||mode=="ab+"||mode=="a+b")
        return open_internal(name,"ab+",true);
    return -1;
}
std::int32_t GuestFileSystem::read(int fd,void* dst,std::size_t n) noexcept{
    const auto it=handles_.find(fd);
    if(it==handles_.end()||(!dst&&n)||n>max_io)return -1;
    if(n==0)return 0;
    const auto count=std::fread(dst,1,n,it->second.file);
    if(count<n&&std::ferror(it->second.file))return -1;
    return static_cast<std::int32_t>(count);
}
std::int32_t GuestFileSystem::write(int fd,const void* src,std::size_t n) noexcept{
    const auto it=handles_.find(fd);
    if(it==handles_.end()||!it->second.write_allowed||
       (!src&&n)||n>max_io)return -1;
    if(!n)return 0;
    const auto wrote=std::fwrite(src,1,n,it->second.file);
    return wrote<n&&std::ferror(it->second.file)?-1:
           static_cast<std::int32_t>(wrote);
}
std::int32_t GuestFileSystem::seek(int fd,std::int32_t offset,int whence) noexcept{
    const auto it=handles_.find(fd);
    if(it==handles_.end()||(whence!=SEEK_SET&&whence!=SEEK_CUR&&whence!=SEEK_END))
        return -1;
    if(std::fseek(it->second.file,long(offset),whence)!=0)return -1;
    const auto value=std::ftell(it->second.file);
    return value<0||value>INT32_MAX?-1:static_cast<std::int32_t>(value);
}
int GuestFileSystem::flush(int fd) noexcept{
    const auto it=handles_.find(fd);
    if(it==handles_.end())return -1;
    return std::fflush(it->second.file)==0?0:-1;
}
int GuestFileSystem::getc(int fd) noexcept{
    const auto it=handles_.find(fd);
    return it==handles_.end()?-1:std::fgetc(it->second.file);
}
int GuestFileSystem::close(int fd) noexcept{
    const auto it=handles_.find(fd);
    if(it==handles_.end())return -1;
    auto* file=it->second.file;
    handles_.erase(it);
    return std::fclose(file)==0?0:-1;
}
int GuestFileSystem::make_directory(const std::string& name) noexcept{
    try{
        fs::path path;bool save=false;
        if(!resolve(name,path,save)||!save)return -1;
        if(path==saves_)return 0;
        if(fs::exists(path))return -1;
        return fs::create_directories(path)?0:-1;
    }catch(...){return -1;}
}
namespace {
void stat_u16(std::array<std::uint8_t,64>& b,unsigned off,std::uint16_t v){
    b[off]=std::uint8_t(v);b[off+1]=std::uint8_t(v>>8u);
}
void stat_u32(std::array<std::uint8_t,64>& b,unsigned off,std::uint32_t v){
    for(unsigned i=0;i<4u;++i)b[off+i]=std::uint8_t(v>>(i*8u));
}
bool build_arm32_stat(const fs::path& path,
                      std::array<std::uint8_t,64>& output){
    try{
        const bool is_dir=fs::is_directory(path);
        if(!is_dir&&!fs::is_regular_file(path))return false;
        const auto size=is_dir?std::uint64_t(0):fs::file_size(path);
        if(size>UINT32_MAX)return false;
        output.fill(0);
        // Layout from Android ARM asm/stat.h, not the x86-64 C library.
        // Offset 0 dev32, 4 ino32, 8 mode16, 10 nlink16,
        // 12 uid16, 14 gid16, 16 rdev32, 20 size32, 24 blksize32,
        // 28 blocks32; timestamps/nanoseconds occupy 32..55.
        stat_u32(output,0,1u);
        stat_u32(output,4,std::uint32_t(std::hash<std::string>{}(path.generic_string())));
        stat_u16(output,8,is_dir?040755u:0100644u);
        stat_u16(output,10,1u);
        stat_u16(output,12,1000u);stat_u16(output,14,1000u);
        stat_u32(output,20,static_cast<std::uint32_t>(size));
        stat_u32(output,24,4096u);
        stat_u32(output,28,static_cast<std::uint32_t>((size+511u)/512u));
        return true;
    }catch(...){return false;}
}
} // namespace
int GuestFileSystem::stat_path(
    const std::string& name,std::array<std::uint8_t,64>& out) noexcept{
    fs::path path;bool save=false;
    return resolve(name,path,save)&&build_arm32_stat(path,out)?0:-1;
}
int GuestFileSystem::stat_fd(
    int fd,std::array<std::uint8_t,64>& out) noexcept{
    const auto it=handles_.find(fd);
    return it!=handles_.end()&&build_arm32_stat(it->second.path,out)?0:-1;
}
int GuestFileSystem::truncate(int fd,std::uint32_t n) noexcept{
    try{
        const auto it=handles_.find(fd);
        if(it==handles_.end()||!it->second.write_allowed ||
           n>256u*1024u*1024u || !below(saves_,it->second.path))
            return -1;
        if(std::fflush(it->second.file)!=0)return -1;
        fs::resize_file(it->second.path,n);
        return 0;
    }catch(...){return -1;}
}
int GuestFileSystem::flush_all() noexcept{
    for(auto& entry:handles_)
        if(entry.second.write_allowed&&std::fflush(entry.second.file)!=0)
            return -1;
    return 0;
}
int GuestFileSystem::remove_directory(const std::string& name) noexcept{
    try{
        fs::path path;bool save=false;
        if(!resolve(name,path,save)||!save || path==saves_ ||
           !fs::is_directory(path))return -1;
        return fs::remove(path)?0:-1; // only an empty save subdirectory
    }catch(...){return -1;}
}
int GuestFileSystem::remove_file(const std::string& name) noexcept{
    try{
        fs::path path;bool save=false;
        if(!resolve(name,path,save)||!save||!fs::is_regular_file(path))return -1;
        return fs::remove(path)?0:-1;
    }catch(...){return -1;}
}
int GuestFileSystem::rename_file(const std::string& before,
                                 const std::string& after) noexcept{
    try{
        fs::path oldpath,newpath;bool fromsave=false,tosave=false;
        if(!resolve(before,oldpath,fromsave)||
           !resolve(after,newpath,tosave)||!fromsave||!tosave||
           !fs::is_regular_file(oldpath)||!below(saves_,newpath))return -1;
        fs::rename(oldpath,newpath);return 0;
    }catch(...){return -1;}
}
} // namespace xperia
