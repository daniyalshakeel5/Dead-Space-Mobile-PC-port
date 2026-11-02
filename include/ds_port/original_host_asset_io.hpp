#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>
namespace ds_port {
// Read an asset through the verified original provider file-mode read,
// size and seek behavior, but with a real explicitly-bound host descriptor.
// root is an installer output containing assets/, and relative_path is
// a game's actual published resource name (never a host absolute path).
// No original ARM code is executed or bundled in the native executable.
// File size is bounded and failure never exposes a partial payload.
bool read_original_asset_file(
    const std::filesystem::path& root,std::string_view relative_path,
    std::size_t max_bytes,std::vector<std::uint8_t>& bytes,
    bool* file_found=nullptr);
} // namespace ds_port
