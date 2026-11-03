#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>
namespace xperia {
// Original Xperia assets are read-only. Files under published/var are redirected
// into private savegames/, never extracted into the copyrighted asset tree.
class GuestFileSystem {
public:
    explicit GuestFileSystem(const std::filesystem::path& install_root);
    ~GuestFileSystem();
    GuestFileSystem(const GuestFileSystem&)=delete;
    GuestFileSystem& operator=(const GuestFileSystem&)=delete;
    int open_readonly(const std::string& guest_path,std::uint32_t flags) noexcept;
    int open_file(const std::string& guest_path,std::uint32_t flags) noexcept;
    int open_stdio(const std::string& guest_path,const std::string& mode) noexcept;
    std::int32_t read(int fd,void* destination,std::size_t bytes) noexcept;
    std::int32_t write(int fd,const void* source,std::size_t bytes) noexcept;
    std::int32_t seek(int fd,std::int32_t offset,int whence) noexcept;
    int flush(int fd) noexcept;
    int getc(int fd) noexcept;
    int close(int fd) noexcept;
    int make_directory(const std::string& path) noexcept;
    int remove_file(const std::string& path) noexcept;
    int rename_file(const std::string& from,const std::string& to) noexcept;
    std::size_t handle_count() const noexcept {return handles_.size();}
    static constexpr std::size_t max_io=8u*1024u*1024u;
private:
    struct Handle {std::FILE* file=nullptr;bool write_allowed=false;};
    std::filesystem::path root_,assets_,saves_;
    std::map<int,Handle> handles_;
    int next_handle_=3;
    bool resolve(const std::string&,std::filesystem::path&,bool& is_save) const;
    int open_internal(const std::string&,const std::string&,bool write_allowed) noexcept;
};
}
