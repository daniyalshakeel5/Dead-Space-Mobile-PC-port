#ifdef NDEBUG
#undef NDEBUG
#endif
#include "atc_decoder.hpp"
#include <array>
#include <cassert>
#include <vector>
using namespace xperia;
int main(){
    std::vector<std::uint8_t> rgba;
    // RGB555 red (0x7c00), RGB565 blue (0x001f).
    // Four leading indices: red, blue, mix1, mix2.
    const std::array<std::uint8_t,8> rgb{
        0x00,0x7c, 0x1f,0x00, 0x9c,0x00,0x00,0x00};
    assert(decode_atc_rgba(rgb.data(),rgb.size(),4,4,AtcFormat::Rgb,rgba));
    assert(rgba.size()==64u);
    assert(rgba[0]==255&&rgba[1]==0&&rgba[2]==0&&rgba[3]==255);
    assert(rgba[4]==0&&rgba[5]==0&&rgba[6]==255&&rgba[7]==255);
    assert(rgba[8]>rgba[10]&&rgba[12]<rgba[14]);
    // Explicit alpha is nibble-packed, low nibble first (0, 255).
    const std::array<std::uint8_t,16> alpha{
        0xf0,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
        0x00,0x7c,0x1f,0x00,0x9c,0x00,0x00,0x00};
    assert(decode_atc_rgba(alpha.data(),alpha.size(),4,4,
                           AtcFormat::RgbaExplicit,rgba));
    assert(rgba[3]==0&&rgba[7]==255);
    // Non-multiple-of-four dimensions crop correctly, with two blocks.
    std::array<std::uint8_t,16> two{};
    for(int i=0;i<8;++i){two[i]=rgb[i];two[i+8]=rgb[i];}
    assert(decode_atc_rgba(two.data(),two.size(),5,3,
                           AtcFormat::Rgb,rgba));
    assert(rgba.size()==5u*3u*4u);
    assert(rgba[4u*4u]==255); // first pixel of the second block
    assert(!decode_atc_rgba(rgb.data(),rgb.size(),5,3,AtcFormat::Rgb,rgba));
    assert(rgba.empty());
    assert(!decode_atc_rgba(nullptr,0,4,4,AtcFormat::Rgb,rgba));
    assert(!decode_atc_rgba(rgb.data(),rgb.size(),0,4,AtcFormat::Rgb,rgba));
    assert(!decode_atc_rgba(rgb.data(),rgb.size(),5000,4,AtcFormat::Rgb,rgba));
    return 0;
}
