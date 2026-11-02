#ifdef NDEBUG
#undef NDEBUG
#endif
#include "ds_port/original_m3g_scene.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
using namespace ds_port;
using Bytes=std::vector<std::uint8_t>;
void u32(Bytes& b,std::uint32_t v) {for(int i=0;i<4;++i)b.push_back((v>>(i*8))&255);}
void f32(Bytes& b,float v) {std::uint32_t word=0;std::memcpy(&word,&v,4);u32(b,word);}
Bytes object3d(){Bytes b;u32(b,0);u32(b,0);u32(b,0);return b;}
Bytes node(){
    auto b=object3d();b.insert(b.end(),{0,0,1,1,255});
    u32(b,0xffffffff);b.push_back(0);return b;
}
void add(Bytes& file,std::uint8_t type,const Bytes& payload) {
    file.push_back(type);u32(file,static_cast<std::uint32_t>(payload.size()));
    file.insert(file.end(),payload.begin(),payload.end());
}
Bytes fixture() {
    Bytes file={0xab,'I','M','-','M','3','G',0xbb,13,10,26,10,0};
    u32(file,0);u32(file,0);
    add(file,0,{});
    auto image=object3d();image.push_back(100);image.push_back(0);
    u32(image,1);u32(image,1);u32(image,0);u32(image,4);
    image.insert(image.end(),{255,32,16,255});add(file,10,image);
    auto tex=object3d();tex.push_back(0);tex.push_back(0);u32(tex,2);
    tex.insert(tex.end(),{0,0,0,227,241,241,208,209});add(file,17,tex);
    auto mat=object3d();mat.push_back(0);
    for(int i=0;i<4;++i)u32(mat,0);
    u32(mat,1);u32(mat,3);add(file,3,mat);
    auto mesh=node();u32(mesh,0);u32(mesh,0);add(file,14,mesh);
    auto camera=node();camera.push_back(50);f32(camera,60.f);
    f32(camera,1.f);f32(camera,.1f);f32(camera,100.f);
    add(file,5,camera);
    auto group=node();u32(group,2);u32(group,5);u32(group,6);add(file,9,group);
    u32(file,0);const std::uint32_t n=static_cast<std::uint32_t>(file.size()-12);
    for(int i=0;i<4;++i)file[13+i]=(n>>(8*i))&255;
    return file;
}
int main() {
    auto bytes=fixture();
    auto r=decode_original_m3g_scene(bytes.data(),bytes.size());
    assert(r.decoded);
    assert(r.groups==1&&r.group_links==2&&r.nodes.size()==3);
    assert(r.camera.node_id==6);
    assert(std::fabs(r.camera.projection[0]-1.7320508f)<0.01f);
    assert(r.materials.size()==1&&r.materials[0].appearance_id==4);
    assert(r.materials[0].textured&&r.materials[0].image_index==0);
    assert(r.images.size()==1&&r.images[0].pixels_rgba[0]==255);
    assert(r.nodes[0].parent_id==7&&r.nodes[1].parent_id==7);
    bytes[12]=1;
    assert(!decode_original_m3g_scene(bytes.data(),bytes.size()).decoded);
    return 0;
}
