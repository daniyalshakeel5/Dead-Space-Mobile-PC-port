#pragma once

#include <cstdint>
#include <string>

namespace ds_port {

// ZIP32 central-directory metadata only. No original assets are unpacked,
// executed, or shipped with this program. ZIP64/multi-disk intentionally fail
// closed until independently tested. No claim that game assets can be decoded.
struct ApkProbeResult {
    bool valid_archive = false;
    bool looks_like_android_apk = false;
    std::string error;
    std::uint32_t entries = 0;
    std::uint32_t assets = 0;
    std::uint32_t native_libraries = 0;
    std::uint32_t dex_files = 0;
    bool has_android_manifest = false;
    bool has_deadspace_library_name = false;  // Name hint, not behavioral proof.
    std::uint64_t declared_uncompressed_bytes = 0;
};

ApkProbeResult probe_apk(const std::string& path);

} // namespace ds_port
