#ifdef NDEBUG
#undef NDEBUG
#endif
#include "ds_port/original_ui_layout.hpp"
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
using Bytes=std::vector<std::uint8_t>;
void u32(Bytes& b,std::uint32_t n){for(int i=0;i<4;++i)b.push_back(std::uint8_t(n>>(8*i)));}
void u16(Bytes& b,std::uint16_t n){b.push_back(std::uint8_t(n));b.push_back(std::uint8_t(n>>8));}
void block(Bytes& b,const char* type,const Bytes& data){
    for(int i=0;i<4;++i)b.push_back(type[i]);
    u32(b,static_cast<std::uint32_t>(data.size()));u32(b,0);
    b.insert(b.end(),data.begin(),data.end());
}
int main(){
    Bytes b{'S','B','I','N'};u32(b,2);
    block(b,"ENUM",{});
    Bytes types;u16(types,1);u16(types,1);u16(types,1);
    block(b,"STRU",types);
    Bytes fields(8,0);
    u16(fields,2);u16(fields,13);u16(fields,0);u16(fields,0);
    block(b,"FIEL",fields);
    Bytes pointers;for(auto p:{17u,450u,576u,658u,1042u})u32(pointers,p);
    block(b,"OHDR",pointers);
    Bytes record(154,0);
    auto put16=[&](std::size_t offset,std::uint16_t val){
        record[offset]=std::uint8_t(val);
        record[offset+1]=std::uint8_t(val>>8);
    };
    auto put32=[&](std::size_t offset,std::uint32_t val){
        for(int i=0;i<4;++i)record[offset+i]=std::uint8_t(val>>(8*i));
    };
    // Genuine binary layout: Layout type 3, string ID, Entity[] ref 3.
    put16(72,3);put16(74,3);put32(78,3);
    // Typed Entity[] array: tag 16, type 0, count 1.
    put16(82,16);put16(84,0);put32(86,1);
    put16(90,4);put32(90+36,4);
    // Typed Vertex[] array: tag 16, type 5, count 1.
    put16(130,16);put16(132,5);put32(134,1);
    block(b,"DATA",record);
    Bytes strings;u32(strings,4);
    for(const std::string s:{"Entity","name","UI_MAIN","BTN_PLAY"}){
        u32(strings,static_cast<std::uint32_t>(s.size()));
        strings.insert(strings.end(),s.begin(),s.end());
    }
    block(b,"STRS",strings);
    const auto layout=ds_port::decode_original_layout_sbin(b.data(),b.size());
    assert(layout.decoded);
    assert(layout.types.size()==1 && layout.types[0].name=="Entity");
    assert(layout.contains_name("UI_MAIN")&&layout.contains_name("BTN_PLAY"));
    assert(layout.references.size()==5);
    assert(layout.references[1].byte_offset==56);
    assert(layout.references[1].tag==2);
    assert(layout.screens.size()==1);
    assert(layout.screens[0].name=="UI_MAIN");
    assert(layout.screens[0].entities.size()==1);
    assert(layout.screens[0].entities[0].name=="BTN_PLAY");
    assert(layout.screens[0].entities[0].vertices.size()==1);
    auto invalid=b;invalid[7]=7;
    assert(!ds_port::decode_original_layout_sbin(invalid.data(),invalid.size()).decoded);
    return 0;
}
