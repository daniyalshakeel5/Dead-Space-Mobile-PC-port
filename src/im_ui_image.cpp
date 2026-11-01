#include "ds_port/im_ui_image.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <new>

namespace ds_port {
namespace {
constexpr std::array<std::uint8_t,12> kImMagic={
    0xab,'I','M','-','M','3','G',0xbb,13,10,26,10};
constexpr std::size_t kMaxFile=64u*1024u*1024u;
std::uint32_t le32(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
        (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
struct Cursor {
    const std::uint8_t* ptr;
    std::size_t size,at=0;
    bool take(std::uint32_t& v) noexcept {
        if(at>size||size-at<4)return false;
        v=le32(ptr+at);at+=4;return true;
    }
    bool skip(std::size_t n) noexcept {
        if(at>size||n>size-at)return false;
        at+=n;return true;
    }
};
bool decode_image(const std::uint8_t* payload,std::size_t length,
                  M3gRgbaImage& image) {
    Cursor at{payload,length,0};
    std::uint32_t ignored=0,tracks=0,params=0;
    if(!at.take(ignored)||!at.take(tracks)||
       tracks>(length-at.at)/4u||
       !at.skip(std::size_t(tracks)*4u)||
       !at.take(params)||params>(length-at.at)/8u)
        return false;
    // Original splash packs have one user metadata parameter:
    // id=500 and 488/504-byte payload. Frontend first Image2D
    // has one 29728-byte parameter. None of this is pixel data.
    for(std::uint32_t i=0;i<params;++i){
        std::uint32_t paramid=0,bytes=0;
        if(!at.take(paramid)||!at.take(bytes)||!at.skip(bytes))
            return false;
    }
    if(at.at>length||length-at.at<18u)return false;
    const auto fmt=payload[at.at++];
    const auto mutable_flag=payload[at.at++];
    std::uint32_t width=0,height=0,palette=0,pixels=0;
    if(!at.take(width)||!at.take(height)||!at.take(palette))
        return false;
    if(palette!=0||!at.skip(palette)||!at.take(pixels))
        return false;
    // User's EA/Dead Space splashes and UI atlas all contain
    // RGBA8. Do not silently invent ETC1 or palette handling.
    if(fmt!=100u||mutable_flag!=0||width==0||height==0||
       width>4096||height>4096)return false;
    const std::uint64_t required=std::uint64_t(width)*height*4u;
    if(required>64u*1024u*1024u || pixels!=required ||
       pixels!=length-at.at)return false;
    image.width=width;image.height=height;
    image.pixels_rgba.assign(payload+at.at,payload+at.at+pixels);
    image.decoded=true;
    return true;
}
} // namespace
ImUiPack decode_im_ui_rgba_images(
    const std::uint8_t* data,std::size_t size) {
    ImUiPack result{};
    auto fail=[&](const char* reason) {
        result.images.clear();result.decoded=false;
        result.error=reason;return result;
    };
    if(!data||size<30||size>kMaxFile ||
       !std::equal(kImMagic.begin(),kImMagic.end(),data)||
       data[12]!=0||le32(data+13)!=size-12)
        return fail("invalid original IM-M3G framing");
    std::size_t at=21,objects=0;
    const auto end=size-4; // trailing proprietary checksum not yet proven
    while(at<end && objects<8192) {
        if(end-at<5)return fail("truncated IM-M3G object");
        const auto type=data[at++];
        const auto len=le32(data+at);at+=4;
        if(len>end-at)return fail("IM-M3G object out of bounds");
        if(objects==0 && type!=0)return fail("missing first header object");
        if(type==10){
            M3gRgbaImage image{};
            if(!decode_image(data+at,len,image))
                return fail("unsupported or malformed UI Image2D");
            result.images.push_back(std::move(image));
            if(result.images.size()>8)
                return fail("excessive UI atlas image count");
        }else if(type!=0 || objects!=0)
            return fail("unsupported UI pack object type");
        at+=len;++objects;
    }
    if(at!=end||objects<2||result.images.empty())
        return fail("incomplete original UI pack");
    result.decoded=true;return result;
}
} // namespace ds_port
