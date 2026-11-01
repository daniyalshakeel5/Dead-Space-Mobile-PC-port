#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ds_port {

// Khronos ETC1 RGB8 4x4 blocks, one complete base mip level, no alpha.
// Independent portable implementation; pixels are row-major RGBA8.
bool decode_etc1_rgb_level(const std::uint8_t* data, std::size_t data_size,
                           std::uint32_t width, std::uint32_t height,
                           std::vector<std::uint8_t>& rgba);
std::uint64_t etc1_base_level_bytes(std::uint32_t width, std::uint32_t height);
std::uint64_t etc1_full_mip_chain_bytes(std::uint32_t width, std::uint32_t height);
}  // namespace ds_port
