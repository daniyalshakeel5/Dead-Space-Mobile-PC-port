#include "ds_port/original_ui_layout.hpp"
#include <algorithm>
#include <cstring>
#include <unordered_map>
namespace ds_port {
namespace {
std::uint32_t u32(const std::uint8_t* p){
    return std::uint32_t(p[0])|std::uint32_t(p[1])<<8u|
           std::uint32_t(p[2])<<16u|std::uint32_t(p[3])<<24u;
}
std::uint16_t u16(const std::uint8_t* p){
    return std::uint16_t(p[0])|std::uint16_t(p[1])<<8u;
}
struct Span{const std::uint8_t* data=nullptr;std::size_t size=0;};
}
bool OriginalLayoutDatabase::contains_name(const std::string& n)const{
    return std::find(names.begin(),names.end(),n)!=names.end();
}
OriginalLayoutDatabase decode_original_layout_sbin(
    const std::uint8_t* bytes,std::size_t size){
    OriginalLayoutDatabase out{};
    const auto fail=[&](const char* error) {
        out.decoded=false;out.error=error;out.names.clear();
        out.types.clear();out.references.clear();out.records.clear();
        return out;
    };
    if(!bytes||size<64||size>8u*1024u*1024u||
       std::memcmp(bytes,"SBIN",4)!=0||u32(bytes+4)!=2)
        return fail("Invalid original layout SBIN v2");
    std::unordered_map<std::string,Span> blocks;
    std::size_t cursor=8;
    for(int n=0;n<24&&cursor+12<=size;++n) {
        const auto* head=bytes+cursor;
        if(!std::all_of(head,head+4,[](std::uint8_t c){
            return c>='A'&&c<='Z';}))break;
        const std::string key(reinterpret_cast<const char*>(head),4);
        const std::uint32_t count=u32(head+4);
        if(count>size-cursor-12||
           !blocks.emplace(key,Span{head+12,count}).second)
            return fail("Malformed original layout SBIN section");
        cursor+=12+count;
    }
    for(const char* key:{"STRU","FIEL","OHDR","DATA","STRS"})
        if(!blocks.count(key))return fail("Missing original layout SBIN section");
    const auto& strings=blocks.at("STRS");
    if(strings.size<4)return fail("Missing original layout names");
    std::size_t pos=4;
    const std::uint32_t count=u32(strings.data);
    if(!count||count>50000)return fail("Original layout name count unsafe");
    out.names.reserve(count);
    for(std::uint32_t i=0;i<count;++i) {
        if(strings.size-pos<4)return fail("Truncated original layout string");
        const auto n=u32(strings.data+pos);pos+=4;
        if(n>strings.size-pos||n>1024)
            return fail("Invalid original layout string length");
        for(std::size_t k=0;k<n;++k)
            if(strings.data[pos+k]<32||strings.data[pos+k]>126)
                return fail("Invalid original layout string byte");
        out.names.emplace_back(
            reinterpret_cast<const char*>(strings.data+pos),n);
        pos+=n;
    }
    if(pos!=strings.size)
        return fail("Original layout names have trailing bytes");
    const auto& defs=blocks.at("STRU");
    const auto& fields=blocks.at("FIEL");
    if(defs.size%6||fields.size%8)
        return fail("Original layout schema misaligned");
    const std::size_t field_count=fields.size/8;
    for(std::size_t i=0;i<defs.size;i+=6) {
        const unsigned name=u16(defs.data+i),
                       first=u16(defs.data+i+2),
                       number=u16(defs.data+i+4);
        if(!name||name>out.names.size()||first>=field_count||
           number>field_count-first)
            return fail("Invalid original layout structure");
        OriginalLayoutType type{};
        type.name=out.names[name-1];
        for(unsigned j=first;j<first+number;++j) {
            const auto* p=fields.data+std::size_t(j)*8u;
            const unsigned field_name=u16(p);
            if(!field_name||field_name>out.names.size())
                return fail("Invalid original layout field");
            type.fields.push_back({out.names[field_name-1],
                u16(p+2),u16(p+4),u16(p+6)});
        }
        out.types.push_back(std::move(type));
    }
    const auto& refs=blocks.at("OHDR");
    const auto& data=blocks.at("DATA");
    if(refs.size%4||refs.size>2u*1024u*1024u||
       data.size>4u*1024u*1024u)
        return fail("Original layout record bounds exceeded");
    out.references.reserve(refs.size/4u);
    for(std::size_t i=0;i<refs.size;i+=4) {
        const auto encoded=u32(refs.data+i);
        const auto offset=encoded/8u;
        if(offset>data.size)
            return fail("Original layout object pointer out of range");
        out.references.push_back({offset,
            static_cast<std::uint8_t>(encoded&7)});
    }
    out.records.assign(data.data,data.data+data.size);
    if(!out.contains_name("UI_MAIN")||!out.contains_name("BTN_PLAY"))
        return fail("Original main-menu layout identifiers missing");
    out.decoded=true;return out;
}
}
