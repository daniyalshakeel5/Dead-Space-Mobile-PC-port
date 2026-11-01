#include "ds_port/im_menu_scene.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace ds_port {
namespace {
constexpr std::array<std::uint8_t,12> kMagic={
    0xab,'I','M','-','M','3','G',0xbb,13,10,26,10
};
struct Chunk {std::uint8_t type; const std::uint8_t* p;std::size_t size;};
std::uint32_t le32(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
          (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
std::uint16_t le16(const std::uint8_t* p) noexcept {
    return std::uint16_t(p[0])|(std::uint16_t(p[1])<<8);
}
float fp32(const std::uint8_t* p) noexcept {
    const std::uint32_t value=le32(p);
    float f=0;std::memcpy(&f,&value,4);return f;
}
int int16(const std::uint8_t* p) noexcept {
    const int x=le16(p);return x<32768?x:x-65536;
}
bool read_chunks(const std::uint8_t* bytes,std::size_t n,
                 std::vector<Chunk>& chunks) {
    if(!bytes||n<30||n>64u*1024u*1024u||
       !std::equal(kMagic.begin(),kMagic.end(),bytes)||
       bytes[12]!=0||le32(bytes+13)!=n-12)
        return false;
    const auto end=n-4;std::size_t at=21;
    while(at<end&&chunks.size()<20000){
        if(end-at<5)return false;
        const auto type=bytes[at++];
        const auto length=le32(bytes+at);at+=4;
        if(length>end-at)return false;
        chunks.push_back({type,bytes+at,length});
        at+=length;
    }
    return at==end&&!chunks.empty()&&chunks[0].type==0;
}
const Chunk* ref(const std::vector<Chunk>& chunks,
                 std::uint32_t id,std::uint8_t type) {
    if(!id||id>chunks.size())return nullptr;
    const auto& chunk=chunks[id-1];
    return chunk.type==type?&chunk:nullptr;
}
bool array_valid(const Chunk* a,unsigned dimension,
                 unsigned count) {
    if(!a||a->size<17||a->p[12]!=2 ||
       a->p[13]!=dimension||a->p[14]!=0)return false;
    const auto n=le16(a->p+15);
    return n==count && n!=0 &&
           a->size==17+std::size_t(n)*dimension*2u;
}
} // namespace
ImMenuGeometry decode_im_menu_geometry(const std::uint8_t* data,
                                       std::size_t bytes) {
    ImMenuGeometry out{};
    auto fail=[&](const char* why) {
        out.decoded=false;out.error=why;out.mesh_count=0;
        out.vertices.clear();out.indices.clear();out.surfaces.clear();
        return out;
    };
    std::vector<Chunk> chunks;
    if(!read_chunks(data,bytes,chunks))
        return fail("malformed original menu IM-M3G framing");
    try{
        for(std::size_t i=0;i<chunks.size();++i){
            const auto& mesh=chunks[i];
            if(mesh.type!=14)continue;
            if(mesh.size<30)
                return fail("truncated original menu mesh");
            const auto* vb=ref(chunks,le32(mesh.p+22),21);
            if(!vb||vb->size!=68)
                return fail("unsupported menu VertexBuffer layout");
            const auto* pos=ref(chunks,le32(vb->p+16),20);
            if(!pos||pos->size<17)
                return fail("invalid menu position array");
            const auto count=le16(pos->p+15);
            if(!array_valid(pos,3,count) || le32(vb->p+44)!=1)
                return fail("unverified menu position or UV-set layout");
            const auto* uv=ref(chunks,le32(vb->p+48),20);
            if(!array_valid(uv,2,count))
                return fail("missing or invalid original menu UV array");
            const float xyz_bias[3]={
                fp32(vb->p+20),fp32(vb->p+24),fp32(vb->p+28)};
            const float xyz_scale=fp32(vb->p+32);
            const float uv_bias[2]={fp32(vb->p+52),fp32(vb->p+56)};
            const float uv_scale=fp32(vb->p+64);
            if(!std::isfinite(xyz_bias[0]) ||
               !std::isfinite(xyz_bias[1]) ||
               !std::isfinite(xyz_bias[2]) ||
               !std::isfinite(xyz_scale) ||
               !std::isfinite(uv_bias[0]) ||
               !std::isfinite(uv_bias[1]) ||
               !std::isfinite(uv_scale))
                return fail("non-finite original menu transform");
            const auto first=out.vertices.size();
            if(first+count>1000000)
                return fail("menu vertex limit exceeded");
            for(std::size_t k=0;k<count;++k){
                const auto* xyz=pos->p+17+k*6u;
                const auto* st=uv->p+17+k*4u;
                ImMenuVertex v{};
                v.x=xyz_bias[0]+xyz_scale*float(int16(xyz));
                v.y=xyz_bias[1]+xyz_scale*float(int16(xyz+2));
                v.z=xyz_bias[2]+xyz_scale*float(int16(xyz+4));
                v.u=uv_bias[0]+uv_scale*float(int16(st));
                v.v=uv_bias[1]+uv_scale*float(int16(st+2));
                if(!std::isfinite(v.x)||!std::isfinite(v.y)||
                   !std::isfinite(v.z)||!std::isfinite(v.u)||
                   !std::isfinite(v.v))
                    return fail("non-finite decoded menu vertex");
                out.vertices.push_back(v);
            }
            const auto part_count=le32(mesh.p+26);
            if(part_count>(mesh.size-30)/4u ||
               mesh.size!=30+std::size_t(part_count)*4u)
                return fail("invalid original mesh submesh references");
            for(std::size_t p=0;p<part_count;++p){
                const auto ib_id=le32(mesh.p+30+p*4u);
                const auto* ib=ref(chunks,ib_id,100);
                if(!ib||ib->size<20)
                    return fail("invalid menu index buffer");
                std::size_t at=12;
                const auto flag=le32(ib->p+8);
                if(flag==1){
                    if(ib->size-at<8)return fail("bad named menu index buffer");
                    at+=4;const auto name_len=le32(ib->p+at);at+=4;
                    if(name_len>ib->size-at)return fail("bad index name length");
                    at+=name_len;
                }else if(flag!=0)return fail("unsupported menu index buffer");
                if(ib->size-at!=8)
                    return fail("invalid menu index buffer references");
                const auto* strip=ref(chunks,le32(ib->p+at),11);
                const auto appearance=le32(ib->p+at+4);
                if(!strip||!ref(chunks,appearance,3)||strip->size<21)
                    return fail("missing original menu appearance or strip");
                const auto width=strip->p[12]==0x81?1u:
                                 strip->p[12]==0x82?2u:0u;
                if(!width)return fail("unrecognized menu index width");
                const auto num=le32(strip->p+13);
                if(num> (strip->size-21)/width || num%3u)
                    return fail("invalid menu strip index count");
                const auto edge=17+std::size_t(num)*width;
                if(edge>strip->size-4)
                    return fail("truncated menu strip table");
                const auto strips=le32(strip->p+edge);
                if(strips!=num/3u ||
                   strips>(strip->size-edge-4)/4u ||
                   strip->size!=edge+4+strips*4u)
                    return fail("invalid menu strip lengths");
                for(std::size_t j=0;j<strips;++j)
                    if(le32(strip->p+edge+4+j*4u)!=3)
                        return fail("unsupported non-triangle strip");
                ImMenuSurface surface{};
                surface.mesh_object_index=static_cast<std::uint32_t>(i+1);
                surface.original_appearance_index=appearance;
                surface.first_index=static_cast<std::uint32_t>(out.indices.size());
                surface.triangle_count=strips;
                for(std::size_t j=0;j<num;++j){
                    const auto index_at=strip->p+17+j*width;
                    const auto idx=width==1?std::uint32_t(index_at[0]):
                                              std::uint32_t(le16(index_at));
                    if(idx>=count)return fail("out of bounds menu index");
                    out.indices.push_back(
                        static_cast<std::uint32_t>(first+idx));
                }
                out.surfaces.push_back(surface);
            }
            ++out.mesh_count;
        }
    }catch(const std::bad_alloc&){
        return fail("menu allocation failed");
    }catch(const std::length_error&){
        return fail("menu size exceeded");
    }
    if(!out.mesh_count||out.surfaces.empty()||out.vertices.empty())
        return fail("no original menu mesh geometry");
    out.decoded=true;return out;
}
} // namespace ds_port
