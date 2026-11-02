#pragma once
#include "ds_port/original_embedded_provider.hpp"
#include "ds_port/original_owner_file_line.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string_view>
#include <vector>

namespace ds_port {

// This host-owned stream delegates to recovered original file-provider
// virtuals: read 0x2716F8, seek 0x273034, position 0x271794,
// size 0x2763AC. No ARM runtime, JNI, or game-owned fd is assumed.
class OriginalHostAssetStream {
public:
    OriginalHostAssetStream() noexcept=default;
    ~OriginalHostAssetStream() noexcept;
    OriginalHostAssetStream(const OriginalHostAssetStream&)=delete;
    OriginalHostAssetStream& operator=(const OriginalHostAssetStream&)=delete;
    bool open(const std::filesystem::path& installed_root,
              std::string_view asset_path,std::size_t maximum_size,
              bool* file_found=nullptr);
    void close() noexcept;
    bool is_open() const noexcept {return file_!=nullptr;}
    std::uint32_t size_bytes() const noexcept {return size_;}
    original_runtime::OriginalEmbeddedProviderResult read(
        void* destination,std::size_t count) noexcept;
    original_runtime::OriginalEmbeddedProviderResult seek(
        std::int32_t offset,std::uint32_t original_origin) noexcept;
    original_runtime::OriginalEmbeddedProviderResult position(
        std::uint32_t original_selector=0u) noexcept;
    // Original mode-8 file provider +0x34/+0x28/+0x2C and
    // original UTF-8 to UTF-16 routine 0x26D5FC, without
    // pretending that the owner +0x70 parser is complete.
    original_runtime::OriginalOwnerWideLineResult read_utf16_line(
        original_runtime::OriginalNativeUtf16String& output) noexcept;
private:
    std::FILE* file_=nullptr;
    int fd_=-1;
    std::uint32_t size_=0;
    std::array<std::uint8_t,0xa8u> provider_{};
};

// Read the whole installed user-owned asset via the same verified
// provider operations. Never return partial data on an error.
bool read_original_asset_file(
    const std::filesystem::path& root,std::string_view relative_path,
    std::size_t max_bytes,std::vector<std::uint8_t>& bytes,
    bool* file_found=nullptr);
} // namespace ds_port
