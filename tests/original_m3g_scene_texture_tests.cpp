#ifdef NDEBUG
#undef NDEBUG
#endif
#include "ds_port/m3g_container.hpp"
#include <zlib.h>
#include <cassert>
#include <cstdint>
#include <vector>
#include <algorithm>
using Bytes=std::vector<std::uint8_t>;
void u32(Bytes& b,std::uint32_t v) {
    for(int j=0;j<4;++j)b.push_back(static_cast<std::uint8_t>(v>>(j*8)));
}
void patch32(Bytes& b,std::size_t pos,std::uint32_t v) {
    for(int j=0;j<4;++j)b.at(pos+j)=static_cast<std::uint8_t>(v>>(j*8));
}
Bytes section(const Bytes& raw,bool zip) {
    Bytes packed=raw;
    if(zip) {
        packed.resize(compressBound(static_cast<uLong>(raw.size())));
        uLongf n=static_cast<uLongf>(packed.size());
        assert(compress2(packed.data(),&n,raw.data(),
            static_cast<uLong>(raw.size()),Z_BEST_SPEED)==Z_OK);
        packed.resize(n);
    }
    Bytes out{static_cast<std::uint8_t>(zip?1:0)};
    u32(out,static_cast<std::uint32_t>(13+packed.size()));
    u32(out,static_cast<std::uint32_t>(raw.size()));
    out.insert(out.end(),packed.begin(),packed.end());
    const auto digest=adler32(adler32(0L,Z_NULL,0),out.data(),
        static_cast<uInt>(out.size()));
    u32(out,static_cast<std::uint32_t>(digest));
    return out;
}
Bytes texture(bool zip,std::uint8_t format) {
    Bytes header{0};u32(header,12);
    header.insert(header.end(),{1,0,0});
    u32(header,0);u32(header,0);
    header.push_back(0);
    Bytes body{0};u32(body,0); // extended second header, validated below
    Bytes image;
    u32(image,0);u32(image,0);u32(image,0); // Object3D header
    image.push_back(format);image.push_back(0);
    u32(image,2);u32(image,2);
    u32(image,0); // no palette
    const unsigned channels=format<=97?1:format==98?2:format==99?3:4;
    u32(image,4*channels);
    for(unsigned j=0;j<4*channels;++j)
        image.push_back(static_cast<std::uint8_t>(j+10));
    body.push_back(10);u32(body,static_cast<std::uint32_t>(image.size()));
    body.insert(body.end(),image.begin(),image.end());
    // A separate legitimate M3G object makes this a larger scene
    // section than the older three-object texture-specific reader.
    body.push_back(14);u32(body,0);
    auto first=section(header,false),second=section(body,zip);
    const std::uint32_t length=static_cast<std::uint32_t>(
        12+first.size()+second.size());
    patch32(header,8,length);
    patch32(header,12,length);
    first=section(header,false);
    Bytes file{0xab,'J','S','R','1','8','4',0xbb,13,10,26,10};
    file.insert(file.end(),first.begin(),first.end());
    file.insert(file.end(),second.begin(),second.end());
    return file;
}
// The original uncompressed particle resource uses a single section,
// repeated total-size word, and a literal zero checksum.
Bytes ironmonkey_single_section(std::uint8_t format) {
    const auto standard=texture(false,format);
    const auto read32=[](const Bytes& b,std::size_t at) {
        return std::uint32_t(b[at])|(std::uint32_t(b[at+1])<<8)|
            (std::uint32_t(b[at+2])<<16)|(std::uint32_t(b[at+3])<<24);
    };
    const std::size_t first=read32(standard,13);
    const std::size_t second_start=12+first;
    const std::size_t second=read32(standard,second_start+1);
    Bytes objects;
    objects.insert(objects.end(),standard.begin()+21,
                   standard.begin()+12+first-4);
    objects.insert(objects.end(),standard.begin()+second_start+9,
                   standard.begin()+second_start+second-4);
    Bytes file(standard.begin(),standard.begin()+12);
    const auto total=static_cast<std::uint32_t>(objects.size()+13);
    file.push_back(0);u32(file,total);u32(file,total);
    file.insert(file.end(),objects.begin(),objects.end());
    u32(file,0);
    return file;
}
int main() {
    for(auto format:{96u,97u,98u,99u,100u})for(bool zip:{false,true}) {
        const auto bytes=texture(zip,static_cast<std::uint8_t>(format));
        const auto decoded=ds_port::decode_m3g_scene_texture_image(
            bytes.data(),bytes.size());
        assert(decoded.decoded);
        assert(decoded.width==2 && decoded.height==2);
        assert(decoded.pixels_rgba.size()==16);
        assert(decoded.pixels_rgba[0]==(format==96?255:10));
        const unsigned expected_alpha=format==96?10:format==98?11:
            format==100?13:255;
        assert(decoded.pixels_rgba[3]==expected_alpha);
        auto broken=bytes;
        broken.back()^=0xff;
        assert(!ds_port::decode_m3g_scene_texture_image(
            broken.data(),broken.size()).decoded);
    }
    for(auto format:{96u,97u,98u,99u,100u}) {
        auto file=ironmonkey_single_section(static_cast<std::uint8_t>(format));
        const auto decoded=ds_port::decode_m3g_ironmonkey_uncompressed_image(
            file.data(),file.size());
        assert(decoded.decoded);
        assert(decoded.width==2 && decoded.height==2);
        assert(decoded.pixels_rgba.size()==16);
        file.back()=1; // vendor zero-checksum rule must remain strict
        assert(!ds_port::decode_m3g_ironmonkey_uncompressed_image(
            file.data(),file.size()).decoded);
    }
    return 0;
}
