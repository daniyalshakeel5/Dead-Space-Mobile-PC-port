#include "ds_port/im_menu_scene.hpp"
#include <array>
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
    std::cout<<"},\"camera_object_ids\":[";
    for(std::size_t i=0;i<scene.camera_object_indices.size();++i) {
        if(i)std::cout<<",";
        std::cout<<scene.camera_object_indices[i];
    }
    std::cout<<"],\"group_count\":"<<scene.group_object_indices.size()
             <<",\"scene_node_payload_sizes\":[";
    for(std::size_t i=0;i<scene.scene_node_records.size();++i) {
        const auto& node=scene.scene_node_records[i];
        if(i)std::cout<<",";
        std::cout<<"["<<node.object_index<<","<<unsigned(node.object_type)
                 <<","<<node.payload_bytes<<"]";
    }
    std::cout<<"],\"candidate_reference_count\":"
             <<scene.scene_reference_candidates.size()
             <<",\"candidate_offsets\":{";
    std::array<unsigned,64> counts{};
    for(const auto& candidate:scene.scene_reference_candidates)
        if(candidate.payload_offset/4<counts.size())
            ++counts[candidate.payload_offset/4];
    bool first_offset=true;
    for(unsigned i=0;i<counts.size();++i) {
        if(!counts[i])continue;
        if(!first_offset)std::cout<<",";
        first_offset=false;
        std::cout<<"\""<<i*4<<"\":"<<counts[i];
    }
    std::cout<<"}}\n";
    return 0;
}
