#include "ds_port/original_ui_sbin.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <unordered_map>
namespace ds_port {
namespace {
std::uint32_t u32(const std::uint8_t* p){
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
std::uint16_t u16(const std::uint8_t* p){
    return std::uint16_t(p[0])|std::uint16_t(p[1])<<8;
}
struct Span {const std::uint8_t* p=nullptr;std::size_t n=0;};
}
OriginalUiSbin decode_original_ui_sbin(const std::uint8_t* data,
                                      std::size_t size){
    OriginalUiSbin out{};
    const auto fail=[&](const char* msg){
        out.decoded=false;out.error=msg;out.names.clear();
        out.types.clear();out.object_headers.clear();out.object_data.clear();
        return out;
    };
    if(!data||size<64||size>1024u*1024u||
       std::memcmp(data,"SBIN",4)!=0||u32(data+4)!=2)
        return fail("Invalid IronMonkey SBIN v2 frontend metadata");
    std::unordered_map<std::string,Span> chunks;
    std::size_t at=8;
    for(int count=0;count<16 && at+12<=size;++count){
        const auto* start=data+at;
        if(!std::all_of(start,start+4,[](unsigned char c){
            return c>='A'&&c<='Z';}))break;
        const std::string name(reinterpret_cast<const char*>(start),4);
        const std::uint32_t length=u32(start+4);
        // Third word is the engine's block hash, not an Adler32 field.
        if(length>size-at-12 || !chunks.emplace(name,
           Span{start+12,length}).second)
            return fail("Malformed or duplicate SBIN section");
        at+=12+length;
    }
    for(const char* required:{"ENUM","STRU","FIEL","OHDR","DATA","STRS"})
        if(!chunks.count(required))return fail("Original SBIN section missing");
    if(chunks["STRU"].n%6!=0||chunks["FIEL"].n%8!=0||
       chunks["OHDR"].n%4!=0)return fail("Original SBIN record width invalid");
    const auto& ss=chunks.at("STRS");
    if(ss.n<4)return fail("SBIN string count missing");
    std::size_t pos=4;
    const auto nstrings=u32(ss.p);
    if(nstrings==0||nstrings>10000)return fail("SBIN string count unsafe");
    out.names.reserve(nstrings);
    for(std::uint32_t k=0;k<nstrings;++k){
        if(pos>ss.n || ss.n-pos<4)return fail("SBIN name length truncated");
        const auto length=u32(ss.p+pos);pos+=4;
        if(length>ss.n-pos||length>512)return fail("SBIN name exceeds section");
        for(std::size_t i=0;i<length;++i)
            if(ss.p[pos+i]<32||ss.p[pos+i]>126)
                return fail("SBIN name nonprintable");
        out.names.emplace_back(
            reinterpret_cast<const char*>(ss.p+pos),length);
        pos+=length;
    }
    if(pos!=ss.n)return fail("SBIN string table trailing bytes");
    const auto& fiel=chunks.at("FIEL");
    const auto& stru=chunks.at("STRU");
    const std::size_t field_count=fiel.n/8;
    for(std::size_t off=0;off<stru.n;off+=6){
        const unsigned name_index=u16(stru.p+off);
        const unsigned first=u16(stru.p+off+2);
        const unsigned count=u16(stru.p+off+4);
        if(!name_index||name_index>out.names.size()||
           first>=field_count||count>field_count-first)
            return fail("Invalid SBIN structure field index");
        OriginalUiStruct type{};
        type.name=out.names[name_index-1];
        for(unsigned i=first;i<first+count;++i){
            const auto* f=fiel.p+std::size_t(i)*8;
            const unsigned name=u16(f);
            if(!name||name>out.names.size())
                return fail("Invalid SBIN field name reference");
            type.fields.push_back({out.names[name-1],u16(f+2),u16(f+4),u16(f+6)});
        }
        out.types.push_back(std::move(type));
    }
    const auto& hdr=chunks.at("OHDR");
    const auto& records=chunks.at("DATA");
    if(hdr.n>128u*1024u || records.n>4u*1024u*1024u)
        return fail("SBIN record byte budget exceeded");
    out.object_headers.assign(hdr.p,hdr.p+hdr.n);
    out.object_data.assign(records.p,records.p+records.n);
    out.decoded=true;return out;
}
}
