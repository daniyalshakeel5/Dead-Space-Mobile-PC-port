#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>

namespace xperia {
// Per-game sandboxed POSIX-style file handles. Only installed original
// assets are readable. No host file pointers or OS handles go to ARM32.
// Save-file writes require a separate, explicitly approved root later.
class GuestFileSystem {
public:
    explicit GuestFileSystem(const std::filesystem::path& install_root);
    ~GuestFileSystem();
    GuestFileSystem(const GuestFileSystem&)=delete;
    GuestFileSystem& operator=(const GuestFileSystem&)=delete;
    int open_readonly(const std::string& guest_path, std::uint32_t flags) noexcept;
    std::int32_t read(int fd, void* destination, std::size_t bytes) noexcept;
    std::int32_t seek(int fd, std::int32_t offset, int whence) noexcept;
    int close(int fd) noexcept;
    std::size_t handle_count() const noexcept {return handles_.size();}
    static constexpr std::size_t max_io=8u*1024u*1024u;
private:
    std::filesystem::path root_;
    std::map<int,std::FILE*> handles_;
    int next_handle_=3;
    bool resolve_asset(const std::string&,std::filesystem::path&) const;
};
}
