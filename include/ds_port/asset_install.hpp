#pragma once
#include <cstdint>
#include <string>

namespace ds_port {

// Install the user-provided APK's assets/ directory into a NEW destination.
// No original game resources are distributed. Rejection is fail-closed.
// ZIP32, stored and DEFLATE entries are supported; CRC32 is verified.
// Not a game renderer, decoder, or mobile runtime.
struct AssetInstallResult {
    bool success = false;
    std::string error;
    std::uint32_t assets_installed = 0;
    std::uint64_t bytes_installed = 0;
};

AssetInstallResult install_apk_assets(const std::string& apk_path,
                                      const std::string& destination);
} // namespace ds_port
