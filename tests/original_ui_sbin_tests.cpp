#ifdef NDEBUG
#undef NDEBUG
#endif
#include "ds_port/original_ui_sbin.hpp"
#include <cassert>
#include <cstdint>
#include <vector>
using Bytes=std::vector<std::uint8_t>;
static void u32(Bytes& v,std::uint32_t n){
    for(int j=0;j<4;++j)v.push_back(std::uint8_t(n>>(j*8)));
}
static void u16(Bytes& v,std::uint16_t n){
    v.push_back(std::uint8_t(n));v.push_back(std::uint8_t(n>>8));
}
static void block(Bytes& v,const char* tag,const Bytes& body){
    for(int i=0;i<4;++i)v.push_back(std::uint8_t(tag[i]));
    u32(v,static_cast<std::uint32_t>(body.size()));u32(v,0);
    v.insert(v.end(),body.begin(),body.end());
}
int main(){
    Bytes file{'S','B','I','N'};u32(file,2);
    Bytes strings;u32(strings,3);
    for(const char* s:{"Image","name","texture"}) {
        std::size_t len=0;while(s[len])++len;
        u32(strings,static_cast<std::uint32_t>(len));
        for(std::size_t i=0;i<len;++i)strings.push_back(s[i]);
    }
    Bytes structs;u16(structs,1);u16(structs,1);u16(structs,2);
    Bytes fields(8,0);
    u16(fields,2);u16(fields,13);u16(fields,0);u16(fields,0);
    u16(fields,3);u16(fields,1);u16(fields,2);u16(fields,0);
    block(file,"ENUM",{});
    block(file,"STRU",structs);block(file,"FIEL",fields);
    Bytes headers;
    for(auto id:{17u,162u,320u,480u,576u})u32(headers,id);
    Bytes data(20,0);
    u32(data,15);u32(data,1);u32(data,2); // root: one Image, ID 2
    data.resize(40,0);
    u16(data,0);u16(data,2); // Image, name="name"
    data.push_back(0);data.push_back(1); // atlas, content scale
    u16(data,2);u16(data,2);u32(data,3);u32(data,4);u16(data,0);
    u16(data,1);u16(data,0);u16(data,0);u16(data,2);u16(data,2);u16(data,0);
    u16(data,1);u16(data,0);u16(data,0);u16(data,2);u16(data,2);u16(data,0);
    block(file,"OHDR",headers);
    block(file,"DATA",data);
    block(file,"STRS",strings);
    auto out=ds_port::decode_original_ui_sbin(file.data(),file.size());
    assert(out.decoded);
    assert(out.names.size()==3);
    assert(out.types.size()==1);
    assert(out.types.front().name=="Image");
    assert(out.types.front().fields.size()==2);
    assert(out.types.front().fields.front().name=="name");
    assert(out.object_data.size()==84);
    assert(out.sprites.size()==1);
    assert(out.sprites[0].name=="name");
    assert(out.sprites[0].atlas_rect.width==2);
    ds_port::M3gRgbaImage atlas{};
    atlas.decoded=true;atlas.width=4;atlas.height=4;
    atlas.pixels_rgba.assign(4u*4u*4u,0);
    for(int y=0;y<2;++y)for(int x=0;x<2;++x){
        auto* pixel=atlas.pixels_rgba.data()+(y*4+x)*4;
        pixel[0]=123;pixel[1]=45;pixel[2]=67;pixel[3]=255;
    }
    const auto rebuilt=ds_port::reconstruct_original_ui_sprite(
        out.sprites.front(),{atlas});
    assert(rebuilt.decoded && rebuilt.width==2 && rebuilt.height==2);
    assert(rebuilt.pixels_rgba[0]==123 && rebuilt.pixels_rgba[3]==255);
    assert(rebuilt.pixels_rgba.size()==16);
    file[3]=0;
    assert(!ds_port::decode_original_ui_sbin(file.data(),file.size()).decoded);
    return 0;
}
