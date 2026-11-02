#include "atc_decoder.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

namespace xperia {
namespace {
std::uint8_t scale5(unsigned n) {return std::uint8_t((n<<3)|(n>>2));}
std::uint8_t scale6(unsigned n) {return std::uint8_t((n<<2)|(n>>4));}
struct RGB {std::uint8_t r=0,g=0,b=0;};
RGB rgb555(std::uint16_t a) {
    return {scale5((a>>10)&31u),scale5((a>>5)&31u),scale5(a&31u)};
}
RGB rgb565(std::uint16_t a) {
    return {scale5((a>>11)&31u),scale6((a>>5)&63u),scale5(a&31u)};
}
RGB blend(RGB a,RGB b,unsigned weight_a,unsigned weight_b) {
    return {std::uint8_t((weight_a*a.r+weight_b*b.r)/8),
            std::uint8_t((weight_a*a.g+weight_b*b.g)/8),
            std::uint8_t((weight_a*a.b+weight_b*b.b)/8)};
}
RGB sub_quarter(RGB first,RGB second) {
    return {std::uint8_t(std::max(0,int(first.r)-int(second.r)/4)),
            std::uint8_t(std::max(0,int(first.g)-int(second.g)/4)),
            std::uint8_t(std::max(0,int(first.b)-int(second.b)/4))};
}
void block(const std::uint8_t* encoded,bool alpha,
           std::array<std::uint8_t,64>& pixels) {
    const std::uint8_t* color=encoded+(alpha?8:0);
    const std::uint16_t low=std::uint16_t(color[0])|(std::uint16_t(color[1])<<8u);
    const std::uint16_t high=std::uint16_t(color[2])|(std::uint16_t(color[3])<<8u);
    std::array<RGB,4> palette{};
    const RGB first=rgb555(low&0x7fffu),last=rgb565(high);
    if((low&0x8000u)==0){
        palette[0]=first;
        palette[3]=last;
        palette[1]=blend(first,last,5,3);
        palette[2]=blend(first,last,3,5);
    }else{
        palette[0]={0,0,0};
        palette[2]=first;
        palette[3]=last;
        palette[1]=sub_quarter(first,last);
    }
    const std::uint32_t indices=std::uint32_t(color[4])|
        std::uint32_t(color[5])<<8u|
        std::uint32_t(color[6])<<16u|
        std::uint32_t(color[7])<<24u;
    for(unsigned i=0;i<16u;++i){
        const auto rgb=palette[(indices>>(2u*i))&3u];
        const unsigned x=4u*i;
        pixels[x]=rgb.r;
        pixels[x+1]=rgb.g;
        pixels[x+2]=rgb.b;
        pixels[x+3]=alpha ?
            std::uint8_t((i&1u ? (encoded[i/2u]>>4u):
                          (encoded[i/2u]&15u))*17u) : 255;
    }
}
}
bool decode_atc_rgba(const std::uint8_t* compressed,
    std::size_t length,std::uint32_t width,std::uint32_t height,
    AtcFormat format,std::vector<std::uint8_t>& out) noexcept {
    out.clear();
    if(!compressed||!width||!height||width>4096||height>4096)
        return false;
    const std::size_t bx=(std::size_t(width)+3u)/4u;
    const std::size_t by=(std::size_t(height)+3u)/4u;
    const std::size_t bytes=format==AtcFormat::Rgb?8u:16u;
    // Reject overflows, missing blocks and unexpected huge output.
    if(bx>std::numeric_limits<std::size_t>::max()/by ||
       bx*by>length/bytes ||
       std::size_t(width)>std::numeric_limits<std::size_t>::max()/height/4u)
        return false;
    try {
        out.resize(std::size_t(width)*height*4u);
        std::array<std::uint8_t,64> rgba{};
        for(std::size_t y=0;y<by;++y){
            for(std::size_t x=0;x<bx;++x){
                block(compressed+(y*bx+x)*bytes,bytes==16u,rgba);
                for(unsigned py=0;py<4u;++py){
                    const auto row=4u*y+py;
                    if(row>=height)continue;
                    for(unsigned px=0;px<4u;++px){
                        const auto col=4u*x+px;
                        if(col>=width)continue;
                        for(unsigned channel=0;channel<4u;++channel)
                            out[(row*width+col)*4u+channel]=rgba[(py*4u+px)*4u+channel];
                    }
                }
            }
        }
        return true;
    }catch(...){
        out.clear();
        return false;
    }
}
}
