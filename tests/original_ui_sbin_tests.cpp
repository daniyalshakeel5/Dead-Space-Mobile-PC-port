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
    block(file,"OHDR",{0,0,0,0});
    block(file,"DATA",{1,2,3,4});
    block(file,"STRS",strings);
    auto out=ds_port::decode_original_ui_sbin(file.data(),file.size());
    assert(out.decoded);
    assert(out.names.size()==3);
    assert(out.types.size()==1);
    assert(out.types.front().name=="Image");
    assert(out.types.front().fields.size()==2);
    assert(out.types.front().fields.front().name=="name");
    assert(out.object_data.size()==4);
    file[3]=0;
    assert(!ds_port::decode_original_ui_sbin(file.data(),file.size()).decoded);
    return 0;
}
