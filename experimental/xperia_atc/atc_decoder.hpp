#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace xperia {
// AMD ATC block texture formats used by Xperia/Vita donor.
// Output: row-major RGBA8, 4-byte pixels; never uploads to GL itself.
// The caller must supply pixel dimensions and the actual compressed
// payload from an original source file, not the entire IM-M3G wrapper.
enum class AtcFormat {Rgb, RgbaExplicit};
bool decode_atc_rgba(const std::uint8_t* compressed,
    std::size_t length,std::uint32_t width,std::uint32_t height,
    AtcFormat format,std::vector<std::uint8_t>& out) noexcept;
}
