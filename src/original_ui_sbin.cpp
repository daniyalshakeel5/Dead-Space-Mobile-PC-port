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
    // The original frontend's DATA root at OHDR[1] is a counted list of
    // Image objects. Object pointers are encoded as byte-offset * 8.
    // Each Image is a two-byte class tag + 16-byte record + padding;
    // Rectangle records use the matching class tag and four UInt16s.
    const auto pointer=[&](std::uint32_t id,std::size_t record_bytes,
                           std::uint16_t type,std::size_t& offset)->bool {
        if(id>=hdr.n/4)return false;
        const std::uint32_t encoded=u32(hdr.p+std::size_t(id)*4);
        if(encoded%8)return false;
        offset=encoded/8u;
        return offset<=records.n && record_bytes<=records.n-offset &&
            u16(records.p+offset)==type;
    };
    std::size_t root=0;
    if(hdr.n/4<2 || u32(hdr.p+4)%8 ||
       (root=u32(hdr.p+4)/8u)>records.n ||
       records.n-root<8)
        return fail("Original UI image root missing");
    const std::uint32_t count=u32(records.p+root+4);
    if(count==0 || count>2000 || count>(records.n-root-8)/4)
        return fail("Invalid original sprite reference count");
    out.sprites.reserve(count);
    for(std::uint32_t n=0;n<count;++n) {
        const std::uint32_t id=u32(records.p+root+8+4u*n);
        std::size_t image_at=0;
        if(!pointer(id,20,0,image_at))
            return fail("Invalid original frontend Image object pointer");
        const auto* image=records.p+image_at+2;
        const unsigned name=u16(image);
        const unsigned atlas=image[2],scale=image[3];
        const unsigned source_width=u16(image+4),source_height=u16(image+6);
        const std::uint32_t source_id=u32(image+8),dest_id=u32(image+12);
        if(!name||name>out.names.size()||atlas>1 || !scale ||
           !source_width||!source_height || source_width>4096 ||
           source_height>4096)
            return fail("Original frontend sprite descriptor invalid");
        std::size_t source_at=0,dest_at=0;
        if(!pointer(source_id,12,1,source_at) ||
           !pointer(dest_id,12,1,dest_at))
            return fail("Original frontend source/destination rectangle missing");
        const auto read_rect=[&](std::size_t at) {
            const auto* b=records.p+at+2;
            return OriginalUiRectangle{u16(b),u16(b+2),u16(b+4),u16(b+6)};
        };
        const auto src=read_rect(source_at),dst=read_rect(dest_at);
        if(!src.width||!src.height||!dst.width||!dst.height ||
           std::uint32_t(src.x)+src.width>source_width ||
           std::uint32_t(src.y)+src.height>source_height ||
           std::uint32_t(dst.x)+dst.width>1024u ||
           std::uint32_t(dst.y)+dst.height>1024u)
            return fail("Original sprite rectangle exceeds original asset bounds");
        out.sprites.push_back({id,out.names[name-1],
            std::uint8_t(atlas),std::uint8_t(scale),
            std::uint16_t(source_width),std::uint16_t(source_height),src,dst});
    }
    out.object_headers.assign(hdr.p,hdr.p+hdr.n);
    out.object_data.assign(records.p,records.p+records.n);
    out.decoded=true;return out;
}
}
