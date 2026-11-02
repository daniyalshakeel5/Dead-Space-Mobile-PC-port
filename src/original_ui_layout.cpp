#include "ds_port/original_ui_layout.hpp"
#include <algorithm>
#include <cstring>
#include <cmath>
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
    const auto name_of=[&](std::uint16_t id)->std::string {
        return id==0||id>out.names.size()?"":out.names[id-1];
    };
    const auto read_float=[&](const std::uint8_t* p)->float {
        const auto bits=u32(p);
        float result=0.f;
        std::memcpy(&result,&bits,4);
        return result;
    };
    const auto array=[&](std::uint32_t object_id,std::uint16_t type,
                         std::size_t stride,const std::uint8_t*& contents,
                         std::uint32_t& count)->bool {
        if(!object_id||object_id>=out.references.size())return false;
        const auto& ref=out.references[object_id];
        if(ref.tag!=2||ref.byte_offset>out.records.size()||
           out.records.size()-ref.byte_offset<8)return false;
        const auto* ptr=out.records.data()+ref.byte_offset;
        count=u32(ptr+4);
        if(u16(ptr)!=16||u16(ptr+2)!=type||count>20000||
           count>(out.records.size()-ref.byte_offset-8)/stride)
            return false;
        contents=ptr+8;return true;
    };
    for(const auto& ref:out.references) {
        if(ref.tag!=0||ref.byte_offset>out.records.size()||
           out.records.size()-ref.byte_offset<10)
            continue;
        const auto* layout=out.records.data()+ref.byte_offset;
        if(u16(layout)!=3)continue; // standard SBIN Layout structure
        const auto screen_name=name_of(u16(layout+2));
        const auto entity_id=u32(layout+6);
        if(screen_name.empty())
            return fail("Original UI layout has invalid name");
        const std::uint8_t* entities=nullptr;
        std::uint32_t count=0;
        if(!array(entity_id,0,40,entities,count))
            return fail("Original UI layout Entity array invalid");
        OriginalLayoutScreen screen{};
        screen.name=screen_name;
        screen.entities.reserve(count);
        for(std::uint32_t k=0;k<count;++k) {
            const auto* e=entities+std::size_t(k)*40;
            OriginalLayoutEntity entity{};
            entity.name=name_of(u16(e+0));
            entity.type=name_of(u16(e+2));
            entity.image=name_of(u16(e+4));
            entity.font=name_of(u16(e+6));
            entity.text_id=name_of(u16(e+8));
            entity.sublayout=name_of(u16(e+10));
            entity.align=name_of(u16(e+12));
            entity.scrolling=e[14]!=0;
            entity.blend=name_of(u16(e+16));
            entity.color=name_of(u16(e+18));
            entity.width=read_float(e+20);
            entity.height=read_float(e+24);
            entity.flags=u32(e+28);
            entity.visible=e[32]!=0;
            const std::uint32_t vertex_id=u32(e+36);
            if(!std::isfinite(entity.width)||!std::isfinite(entity.height)||
               std::fabs(entity.width)>65536.f||
               std::fabs(entity.height)>65536.f)
                return fail("Original UI entity dimensions invalid");
            if(vertex_id) {
                const std::uint8_t* vertices=nullptr;
                std::uint32_t verts=0;
                if(!array(vertex_id,5,16,vertices,verts)||verts>32)
                    return fail("Original UI Vertex array invalid");
                for(std::uint32_t v=0;v<verts;++v) {
                    const auto* p=vertices+std::size_t(v)*16;
                    OriginalLayoutVertex vertex{
                        read_float(p),read_float(p+4),
                        read_float(p+8),read_float(p+12)};
                    if(!std::isfinite(vertex.x)||!std::isfinite(vertex.y)||
                       !std::isfinite(vertex.anchor_x)||
                       !std::isfinite(vertex.anchor_y)||
                       std::fabs(vertex.x)>65536.f||
                       std::fabs(vertex.y)>65536.f)
                        return fail("Original UI vertex coordinate invalid");
                    entity.vertices.push_back(vertex);
                }
            }
            screen.entities.push_back(std::move(entity));
        }
        out.screens.push_back(std::move(screen));
    }
    if(out.screens.empty())
        return fail("Original UI Layout records missing");
    if(!out.contains_name("UI_MAIN")||!out.contains_name("BTN_PLAY"))
        return fail("Original main-menu layout identifiers missing");
    out.decoded=true;return out;
}
}
