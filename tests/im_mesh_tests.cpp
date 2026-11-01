#include "ds_port/im_mesh.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::cerr<<"IM mesh test failure: "<<__LINE__ \
    <<" "<<#x<<"\n"; return 1; } } while (0)
using Bytes=std::vector<std::uint8_t>;
void put32(Bytes& b,std::uint32_t v) { for(int i=0;i<4;i++)b.push_back((v>>(i*8))&255); }
void put16(Bytes& b,std::uint16_t v) { b.push_back(v&255);b.push_back((v>>8)&255); }
void putfloat(Bytes& b,float v) {
    std::uint32_t bits=0;std::memcpy(&bits,&v,4);put32(b,bits);
}
void add(Bytes& b,std::uint8_t type,const Bytes& payload) {
    b.push_back(type);put32(b,static_cast<std::uint32_t>(payload.size()));
    b.insert(b.end(),payload.begin(),payload.end());
}
Bytes fixture(bool invalid_index=false) {
    Bytes objects;
    add(objects,0,Bytes(12,0)); // object 1
    Bytes va(12,0);va.insert(va.end(),{2,3,0});put16(va,3);
    for (const auto [x,y,z] : {std::array<int,3>{0,0,0},
                               std::array<int,3>{1,0,0},
                               std::array<int,3>{0,1,0}}) {
        put16(va,static_cast<std::uint16_t>(x));
        put16(va,static_cast<std::uint16_t>(y));
        put16(va,static_cast<std::uint16_t>(z));
    }
    add(objects,20,va); // object 2
    Bytes vb(12,0);vb.insert(vb.end(),{255,255,255,255});put32(vb,2);
    for (int i=0;i<3;i++)putfloat(vb,0.f);
    putfloat(vb,1.f);put32(vb,0);put32(vb,0);put32(vb,0);
    add(objects,21,vb); // object 3
    Bytes strip(12,0);strip.push_back(0x81);put32(strip,3);
    strip.insert(strip.end(),{0,1,static_cast<std::uint8_t>(invalid_index?3:2)});
    put32(strip,1);put32(strip,3);
    add(objects,11,strip); // object 4
    add(objects,3,Bytes(12,0)); // object 5 appearance
    Bytes ib(8,0);put32(ib,0);put32(ib,4);put32(ib,5);
    add(objects,100,ib); // object 6
    Bytes mesh(22,0);put32(mesh,3);put32(mesh,1);put32(mesh,6);
    add(objects,14,mesh); // object 7
    Bytes file{0xab,'I','M','-','M','3','G',0xbb,13,10,26,10,0};
    put32(file,static_cast<std::uint32_t>(13+objects.size()));
    put32(file,static_cast<std::uint32_t>(objects.size()));
    file.insert(file.end(),objects.begin(),objects.end());
    put32(file,0); // opaque original proprietary trailer
    return file;
}
int main() {
    auto input=fixture();
    std::ostringstream obj;
    auto found=ds_port::scan_im_mesh(input.data(),input.size(),&obj);
    CHECK(found.valid);
    CHECK(found.mesh_objects==1);
    CHECK(found.submeshes==1);
    CHECK(found.vertices==3);
    CHECK(found.triangles==1);
    CHECK(obj.str().find("v 1 0 0")!=std::string::npos);
    CHECK(obj.str().find("f 1 2 3")!=std::string::npos);
    CHECK(!ds_port::scan_im_mesh(nullptr,0).valid);
    CHECK(!ds_port::scan_im_mesh(input.data(),input.size()-1).valid);
    auto broken=fixture(true);
    CHECK(!ds_port::scan_im_mesh(broken.data(),broken.size()).valid);
    std::cout<<"IronMonkey independent native mesh extraction fixtures passed\n";
}
