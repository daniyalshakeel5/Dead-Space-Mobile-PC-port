#include "ds_port/original_m3g_scene.hpp"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    std::ifstream input(argv[1],std::ios::binary|std::ios::ate);
    if(!input)return 3;
    const auto len=input.tellg();
    if(len<=0||len>64*1024*1024)return 4;
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(len));
    input.seekg(0);
    if(!input.read(reinterpret_cast<char*>(bytes.data()),len))return 5;
    auto original=ds_port::decode_original_m3g_scene(bytes.data(),bytes.size());
    if(!original.decoded) {
        std::cerr<<"Original M3G scene failed: "<<original.error<<"\n";
        return 6;
    }
    std::size_t textured=0;
    for(const auto& m:original.materials)if(m.textured)++textured;
    std::cout<<"Original camera node: "<<original.camera.node_id<<"\n"
             <<"Original nodes: "<<original.nodes.size()<<"\n"
             <<"Original groups: "<<original.groups<<"\n"
             <<"Original child links: "<<original.group_links<<"\n"
             <<"Original appearances: "<<original.materials.size()<<"\n"
             <<"Original textured appearances: "<<textured<<"\n"
             <<"Original scene images: "<<original.images.size()<<"\n"
             <<"Original animation track references: "<<original.animation_references<<"\n";
    return 0;
}
