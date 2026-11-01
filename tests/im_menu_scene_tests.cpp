#include "ds_port/im_menu_scene.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
using namespace ds_port;
namespace {
void write32(std::vector<std::uint8_t>& p,std::size_t at,std::uint32_t n) {
    for(unsigned i=0;i<4;++i)p[at+i]=std::uint8_t(n>>(8*i));
}
void word(std::vector<std::uint8_t>& p,std::uint32_t n) {
    for(unsigned i=0;i<4;++i)p.push_back(std::uint8_t(n>>(8*i)));
}
void half(std::vector<std::uint8_t>& p,std::uint16_t v){
    p.push_back(std::uint8_t(v));p.push_back(std::uint8_t(v>>8));
}
void flt(std::vector<std::uint8_t>& p,std::size_t at,float f){
    std::uint32_t n=0;std::memcpy(&n,&f,4);write32(p,at,n);
}
void chunk(std::vector<std::uint8_t>& p,std::uint8_t t,
           const std::vector<std::uint8_t>& data){
    p.push_back(t);word(p,static_cast<std::uint32_t>(data.size()));
    p.insert(p.end(),data.begin(),data.end());
}
std::vector<std::uint8_t> fixture(){
    std::vector<std::uint8_t> o={
        0xab,'I','M','-','M','3','G',0xbb,13,10,26,10,0,0,0,0,0,0,0,0,0};
    chunk(o,0,std::vector<std::uint8_t>(12));
    std::vector<std::uint8_t> pos(17);
    pos[12]=2;pos[13]=3;pos[14]=0;pos[15]=3;
    for(const std::uint16_t v:{
        0,0,0,100,0,0,0,100,0})half(pos,v);
    chunk(o,20,pos); // ref 2
    std::vector<std::uint8_t> uv(17);
    uv[12]=2;uv[13]=2;uv[14]=0;uv[15]=3;
    for(const std::uint16_t v:{
        0,0,32767,0,0,32767})half(uv,v);
    chunk(o,20,uv); // ref 3
    std::vector<std::uint8_t> vb(68);
    write32(vb,16,2);flt(vb,32,1.f);
    write32(vb,44,1);write32(vb,48,3);
    flt(vb,64,1.f/32767.f);
    chunk(o,21,vb); // ref 4
    chunk(o,3,std::vector<std::uint8_t>(20)); // appearance ref 5
    std::vector<std::uint8_t> strip(17);
    strip[12]=0x81;write32(strip,13,3);
    strip.push_back(0);strip.push_back(1);strip.push_back(2);
    word(strip,1);word(strip,3);
    chunk(o,11,strip); // ref 6
    std::vector<std::uint8_t> ib(20);
    write32(ib,12,6);write32(ib,16,5);
    chunk(o,100,ib); // ref 7
    std::vector<std::uint8_t> mesh(34);
    write32(mesh,22,4);write32(mesh,26,1);write32(mesh,30,7);
    chunk(o,14,mesh); // ref 8
    word(o,0);write32(o,13,static_cast<std::uint32_t>(o.size()-12));
    return o;
}
}
int main(){
    auto bytes=fixture();
    auto original=decode_im_menu_geometry(bytes.data(),bytes.size());
    assert(original.decoded&&original.mesh_count==1);
    assert(original.vertices.size()==3 && original.indices.size()==3);
    assert(original.surfaces.size()==1);
    assert(original.mesh_bounds.size()==1);
    const auto& b=original.mesh_bounds[0];
    assert(b.mesh_object_index==8);
    assert(b.min_x==0 && b.max_x==100);
    assert(b.min_y==0 && b.max_y==100);
    assert(b.min_z==0 && b.max_z==0);
    assert(original.surfaces[0].original_appearance_index==5);
    assert(original.surfaces[0].mesh_object_index==8);
    assert(original.indices[0]==0&&original.indices[1]==1&&original.indices[2]==2);
    assert(original.vertices[0].u==0&&original.vertices[0].v==0);
    assert(original.vertices[1].u>0.999f&&original.vertices[1].u<1.001f);
    assert(original.vertices[2].v>0.999f&&original.vertices[2].v<1.001f);
    assert(original.vertices[1].x==100);
    // Missing UV link, invalid appearance and index outside vertex
    // count must be rejected before drawing a GPU triangle.
    auto broken=bytes;
    // Derived positions: first chunk after 21; position array 17+18;
    // UV array 17+12; vertex buffer starts at object 4.
    // Corrupt the fixture's original VB UV reference from 3 to 999.
    const auto vb_header=21u+17u+(5u+35u)+(5u+29u);
    assert(broken[vb_header]==21u);
    write32(broken,vb_header+5u+48u,999);
    assert(!decode_im_menu_geometry(broken.data(),broken.size()).decoded);
    broken=bytes;
    assert(broken.size()>30);
    broken.resize(broken.size()-1);
    assert(!decode_im_menu_geometry(broken.data(),broken.size()).decoded);
    broken=bytes;
    broken[0]=0;
    assert(!decode_im_menu_geometry(broken.data(),broken.size()).decoded);
    return 0;
}
