#include "ds_port/etc1.hpp"

#include <algorithm>
#include <array>

namespace ds_port {
namespace {
constexpr std::int16_t kModifier[8][4] = {
    {2, 8, -2, -8},
    {5, 17, -5, -17},
    {9, 29, -9, -29},
    {13, 42, -13, -42},
    {18, 60, -18, -60},
    {24, 80, -24, -80},
    {33, 106, -33, -106},
    {47, 183, -47, -183},
};
constexpr std::int32_t sign3(std::uint32_t value) {
    return value >= 4 ? static_cast<std::int32_t>(value) - 8
                      : static_cast<std::int32_t>(value);
}
std::uint8_t saturate(std::int32_t c) {
    return static_cast<std::uint8_t>(std::max(0, std::min(c, 255)));
}
struct Rgb { std::int32_t r, g, b; };
std::uint64_t block64(const std::uint8_t* bytes) {
    std::uint64_t v = 0;
    for (unsigned i=0; i<8; ++i)
        v = (v << 8) | bytes[i];
    return v;
}
bool decode_block(const std::uint8_t* bytes, std::uint8_t rgba[64]) {
    const auto b = block64(bytes);
    const bool differential = ((b >> 33) & 1u) != 0;
    const bool flip = ((b >> 32) & 1u) != 0;
    Rgb first{}, second{};
    if (differential) {
        const auto r = static_cast<std::int32_t>((b >> 59) & 31);
        const auto g = static_cast<std::int32_t>((b >> 51) & 31);
        const auto bl = static_cast<std::int32_t>((b >> 43) & 31);
        const auto rr = r + sign3(static_cast<unsigned>((b >> 56) & 7));
        const auto gg = g + sign3(static_cast<unsigned>((b >> 48) & 7));
        const auto bb = bl + sign3(static_cast<unsigned>((b >> 40) & 7));
        // Out-of-range differential values are ETC2 T/H modes,
        // not a valid ETC1 image. Fail rather than silently misdecode.
        if (rr < 0 || rr > 31 || gg < 0 || gg > 31 || bb < 0 || bb > 31)
            return false;
        const auto expand = [](std::int32_t x) { return (x << 3) | (x >> 2); };
        first = {expand(r), expand(g), expand(bl)};
        second = {expand(rr), expand(gg), expand(bb)};
    } else {
        const auto expand = [](std::uint32_t x) {
            return static_cast<std::int32_t>(x * 17);
        };
        first = {expand((b >> 60) & 15), expand((b >> 52) & 15),
                 expand((b >> 44) & 15)};
        second = {expand((b >> 56) & 15), expand((b >> 48) & 15),
                  expand((b >> 40) & 15)};
    }
    const auto table0 = static_cast<unsigned>((b >> 37) & 7);
    const auto table1 = static_cast<unsigned>((b >> 34) & 7);
    for (unsigned y = 0; y < 4; ++y) {
        for (unsigned x = 0; x < 4; ++x) {
            const unsigned index = x * 4 + y; // ETC1 column-major bit order
            const unsigned code = static_cast<unsigned>(
                ((b >> index) & 1) | (((b >> (index + 16)) & 1) << 1));
            const bool second_half = flip ? y >= 2 : x >= 2;
            const auto color = second_half ? second : first;
            const auto modifier = kModifier[second_half ? table1 : table0][code];
            const auto pixel = 4 * (y * 4 + x);
            rgba[pixel + 0] = saturate(color.r + modifier);
            rgba[pixel + 1] = saturate(color.g + modifier);
            rgba[pixel + 2] = saturate(color.b + modifier);
            rgba[pixel + 3] = 255;
        }
    }
    return true;
}
}  // namespace

std::uint64_t etc1_base_level_bytes(std::uint32_t w, std::uint32_t h) {
    if (!w || !h || w > 8192 || h > 8192) return 0;
    return std::uint64_t((w+3)/4) * ((h+3)/4) * 8;
}
std::uint64_t etc1_full_mip_chain_bytes(std::uint32_t w, std::uint32_t h) {
    if (!etc1_base_level_bytes(w, h)) return 0;
    std::uint64_t result = 0;
    for (;;) {
        result += etc1_base_level_bytes(w, h);
        if (w == 1 && h == 1) break;
        w = std::max(1u, w/2);
        h = std::max(1u, h/2);
    }
    return result;
}
bool decode_etc1_rgb_level(const std::uint8_t* data, std::size_t data_size,
                           std::uint32_t w, std::uint32_t h,
                           std::vector<std::uint8_t>& rgba) {
    rgba.clear();
    const auto expected = etc1_base_level_bytes(w, h);
    const auto count = std::uint64_t(w)*h*4;
    if (!data || !expected || expected > data_size || count > 64ull*1024*1024)
        return false;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(count));
    const auto tiles_x = (w+3)/4, tiles_y = (h+3)/4;
    for (std::uint32_t by = 0; by < tiles_y; ++by) {
        for (std::uint32_t bx = 0; bx < tiles_x; ++bx) {
            std::uint8_t tile[64]{};
            const auto at = (std::size_t(by)*tiles_x + bx) * 8;
            if (!decode_block(data + at, tile)) return false;
            for (unsigned y=0; y<4; ++y) {
                const auto dest_y = by*4+y;
                if (dest_y >= h) continue;
                for (unsigned x=0; x<4; ++x) {
                    const auto dest_x = bx*4+x;
                    if (dest_x >= w) continue;
                    const auto src = 4 * (y*4+x);
                    const auto dest = 4 * (std::size_t(dest_y)*w+dest_x);
                    for (unsigned component=0; component<4; ++component)
                        pixels[dest+component] = tile[src+component];
                }
            }
        }
    }
    rgba = std::move(pixels);
    return true;
}
}  // namespace ds_port
