#include "ds_port/im_menu_scene.hpp"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>
int main(int argc,char** argv){
    if(argc!=2){
        std::cerr<<"usage: ds_im_menu_probe <original-owned-main_menu.m3g>\n";
        return 2;
    }
    std::ifstream in(argv[1],std::ios::binary|std::ios::ate);
    if(!in)return 2;
    const auto size=in.tellg();
    if(size<=0 || static_cast<std::uint64_t>(size)>64ull*1024*1024)
        return 2;
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    in.seekg(0);
    if(!in.read(reinterpret_cast<char*>(bytes.data()),size))return 2;
    const auto scene=ds_port::decode_im_menu_geometry(bytes.data(),bytes.size());
    if(!scene.decoded){
        std::cerr<<"Original menu geometry/UV unsupported: "<<scene.error<<"\n";
        return 1;
    }
    // This is metadata only: no original mesh coordinates or image bytes
    // appear in CI logs or distributable build artifacts.
    std::cout<<"{\"decoded\":true,\"meshes\":"<<scene.mesh_count
             <<",\"vertices\":"<<scene.vertices.size()
             <<",\"triangles\":"<<scene.indices.size()/3
             <<",\"surfaces\":"<<scene.surfaces.size()
             <<",\"vertices_with_uv\":"<<scene.vertices.size()
             <<",\"object_types\":{";
    bool first=true;
    for(unsigned i=0;i<scene.original_object_type_counts.size();++i) {
        const auto count=scene.original_object_type_counts[i];
        if(!count)continue;
        if(!first)std::cout<<",";
        first=false;
        std::cout<<"\""<<i<<"\":"<<count;
    }
    std::cout<<"}}\n";
    return 0;
}
