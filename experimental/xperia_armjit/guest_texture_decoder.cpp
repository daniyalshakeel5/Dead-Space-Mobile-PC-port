#include "guest_texture_decoder.hpp"
#include "third_party/powervr/PVRTDecompress.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
namespace xperia { namespace texture {
namespace {
constexpr std::size_t max_image=64u*1024u*1024u;
std::uint16_t read16(const std::uint8_t* d) {
    return std::uint16_t(d[0]) | (std::uint16_t(d[1])<<8u);
}
std::uint32_t read32(const std::uint8_t* d) {
    return std::uint32_t(read16(d)) | (std::uint32_t(read16(d+2))<<16u);
}
std::uint8_t expand(unsigned v,unsigned bits) {
    return std::uint8_t((v<<(8u-bits))|(v>>(2u*bits-8u)));
}
using Color=std::array<std::uint8_t,4>;
void atc_color(const std::uint8_t* d,Color* pixels) {
    const auto lo=read16(d),hi=read16(d+2);
    Color p[4]{};
    const auto unpack555=[&](unsigned v){
        return Color{expand((v>>10)&31u,5),expand((v>>5)&31u,5),
                     expand(v&31u,5),255};
    };
    const auto unpack565=[&](unsigned v){
        return Color{expand((v>>11)&31u,5),expand((v>>5)&63u,6),
                     expand(v&31u,5),255};
    };
    if(!(lo&0x8000u)){
        p[0]=unpack555(lo);
        p[3]=unpack565(hi);
        for(unsigned c=0;c<3;++c){
            p[1][c]=std::uint8_t((5u*p[0][c]+3u*p[3][c])/8u);
            p[2][c]=std::uint8_t((3u*p[0][c]+5u*p[3][c])/8u);
        }
    }else{
        p[2]=unpack555(lo&0x7fffu);
        p[3]=unpack565(hi);
        for(unsigned c=0;c<3;++c)
            p[1][c]=std::uint8_t(std::max(0,int(p[2][c])-int(p[3][c])/4));
    }
    const auto indices=read32(d+4);
    for(unsigned i=0;i<16u;++i)pixels[i]=p[(indices>>(i*2u))&3u];
}
void atc_explicit_alpha(const std::uint8_t* d,Color* pixels){
    for(unsigned i=0;i<16u;++i){
        const auto packed=d[i/2u];
        pixels[i][3]=std::uint8_t(((i&1u)?packed>>4u:packed&15u)*17u);
    }
}
void atc_interpolated_alpha(const std::uint8_t* d,Color* pixels){
    std::uint8_t alpha[8]{d[0],d[1]};
    if(alpha[0]>alpha[1]){
        for(unsigned k=1;k<=6u;++k)
            alpha[k+1]=std::uint8_t(((7u-k)*alpha[0]+k*alpha[1])/7u);
    }else{
        for(unsigned k=1;k<=4u;++k)
            alpha[k+1]=std::uint8_t(((5u-k)*alpha[0]+k*alpha[1])/5u);
        alpha[6]=0;alpha[7]=255;
    }
    std::uint64_t pattern=0;
    for(unsigned i=0;i<6;++i)pattern|=std::uint64_t(d[i+2])<<(8u*i);
    for(unsigned i=0;i<16u;++i)
        pixels[i][3]=alpha[(pattern>>(i*3u))&7u];
}
} // namespace
bool supported(std::uint32_t format) noexcept {
    return format==ATC_RGB||format==ATC_RGBA_EXPLICIT||
           format==ATC_RGBA_INTERPOLATED||
           (format>=PVRTC_RGB_4BPP && format<=PVRTC_RGBA_2BPP);
}
bool expected_size(std::uint32_t format,int width,int height,
                   std::size_t& result,std::string& error) noexcept {
    result=0;
    if(!supported(format)){error="Unsupported Xperia compressed texture enum";return false;}
    if(width<=0||height<=0||width>4096||height>4096){
        error="Compressed texture dimension outside guest limits";return false;
    }
    const auto w=std::uint64_t(width),h=std::uint64_t(height);
    if(w*h*4u>max_image){error="RGBA decoded texture exceeds 64MiB";return false;}
    std::uint64_t bytes=0;
    if(format==ATC_RGB||format==ATC_RGBA_EXPLICIT||
       format==ATC_RGBA_INTERPOLATED) {
        bytes=((w+3u)/4u)*((h+3u)/4u)*
            (format==ATC_RGB?8u:16u);
    }else{
        const bool two=(format==PVRTC_RGB_2BPP || format==PVRTC_RGBA_2BPP);
        // PVRTC1 minimum valid footprint: 16x8 at 2bpp, 8x8 at 4bpp.
        bytes=std::max<std::uint64_t>(w,two?16u:8u)*
              std::max<std::uint64_t>(h,8u)*(two?2u:4u)/8u;
    }
    if(bytes>max_image){error="Compressed texture payload exceeds 64MiB";return false;}
    result=static_cast<std::size_t>(bytes);
    error.clear();return true;
}
bool decode(std::uint32_t fmt,int width,int height,
            const std::uint8_t* data,std::size_t length,
            std::vector<std::uint8_t>& rgba,std::string& error) {
    rgba.clear();
    std::size_t required=0;
    if(!expected_size(fmt,width,height,required,error))return false;
    if(!data || length!=required) {
        error="Original compressed texture byte count does not match format";
        return false;
    }
    try {
        rgba.resize(std::size_t(width)*std::size_t(height)*4u);
        if(fmt==ATC_RGB||fmt==ATC_RGBA_EXPLICIT||fmt==ATC_RGBA_INTERPOLATED){
            const auto blocks_x=(unsigned(width)+3u)/4u;
            const auto blocks_y=(unsigned(height)+3u)/4u;
            const auto pitch=fmt==ATC_RGB?8u:16u;
            for(unsigned by=0;by<blocks_y;++by)
                for(unsigned bx=0;bx<blocks_x;++bx){
                    const auto* block=data+(std::size_t(by)*blocks_x+bx)*pitch;
                    Color px[16]{};
                    atc_color(block+(pitch==16u?8u:0u),px);
                    if(fmt==ATC_RGBA_EXPLICIT)atc_explicit_alpha(block,px);
                    if(fmt==ATC_RGBA_INTERPOLATED)atc_interpolated_alpha(block,px);
                    for(unsigned y=0;y<4;++y)
                        for(unsigned x=0;x<4;++x){
                            const auto tx=bx*4u+x,ty=by*4u+y;
                            if(tx>=unsigned(width)||ty>=unsigned(height))continue;
                            std::memcpy(rgba.data()+
                                (std::size_t(ty)*std::size_t(width)+tx)*4u,
                                px[y*4u+x].data(),4);
                        }
                }
        }else{
            const bool two=fmt==PVRTC_RGB_2BPP || fmt==PVRTC_RGBA_2BPP;
            pvr::PVRTDecompressPVRTC(data,two?1u:0u,
                                    std::uint32_t(width),std::uint32_t(height),
                                    rgba.data());
            if(fmt==PVRTC_RGB_4BPP||fmt==PVRTC_RGB_2BPP)
                for(std::size_t i=3;i<rgba.size();i+=4)rgba[i]=255;
        }
        error.clear();return true;
    }catch(const std::exception& e){
        rgba.clear();error=e.what();return false;
    }
}
}} // namespace xperia::texture
