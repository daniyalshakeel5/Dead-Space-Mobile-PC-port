#include "ds_port/etc1.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::cerr << "ETC1 fixture failure " \
    << __LINE__ << ": " << #x << '\n'; return 1; } } while (0)

std::array<std::uint8_t,8> block(std::uint64_t v) {
    std::array<std::uint8_t,8> b{};
    for (int i=7; i>=0; --i) { b[i] = static_cast<std::uint8_t>(v & 255); v >>= 8; }
    return b;
}
int main() {
    std::vector<std::uint8_t> rgba;
    const auto black = block(0);
    CHECK(ds_port::decode_etc1_rgb_level(black.data(),8,4,4,rgba));
    CHECK(rgba.size()==64);
    for (std::size_t i=0;i<rgba.size();i+=4) {
        CHECK(rgba[i]==2 && rgba[i+1]==2 && rgba[i+2]==2 && rgba[i+3]==255);
    }

    // Individual color mode, vertical partition: left red / right green.
    const auto two_color = block((std::uint64_t{15}<<60)|(std::uint64_t{15}<<48));
    CHECK(ds_port::decode_etc1_rgb_level(two_color.data(),8,4,4,rgba));
    CHECK(rgba[0]==255 && rgba[1]==2 && rgba[2]==2);
    CHECK(rgba[8]==2 && rgba[9]==255 && rgba[10]==2);
    CHECK(rgba[63]==255);

    // Differential, both halves red, valid offset 0.
    const auto differential = block((std::uint64_t{31}<<59)|(std::uint64_t{1}<<33));
    CHECK(ds_port::decode_etc1_rgb_level(differential.data(),8,4,4,rgba));
    CHECK(rgba[0]==255 && rgba[1]==2 && rgba[2]==2);

    // ETC1 forbids a differential base color outside the 5-bit range.
    const auto illegal = block((std::uint64_t{31}<<59)|(std::uint64_t{3}<<56)|
                               (std::uint64_t{1}<<33));
    CHECK(!ds_port::decode_etc1_rgb_level(illegal.data(),8,4,4,rgba));
    CHECK(rgba.empty());

    // Clipped 2x2 mip level still uses one complete 4x4 block.
    CHECK(ds_port::decode_etc1_rgb_level(black.data(),8,2,2,rgba));
    CHECK(rgba.size()==16);
    CHECK(ds_port::etc1_full_mip_chain_bytes(4,4)==24);
    CHECK(ds_port::etc1_full_mip_chain_bytes(8,8)==56);
    CHECK(ds_port::etc1_base_level_bytes(8,8)==32);
    CHECK(!ds_port::decode_etc1_rgb_level(black.data(),7,4,4,rgba));
    CHECK(!ds_port::decode_etc1_rgb_level(nullptr,8,4,4,rgba));
    CHECK(ds_port::etc1_base_level_bytes(0,8)==0);
    CHECK(ds_port::etc1_full_mip_chain_bytes(0,8)==0);
    std::cout << "Independent ETC1 software block decoder synthetic tests passed\n";
    return 0;
}
