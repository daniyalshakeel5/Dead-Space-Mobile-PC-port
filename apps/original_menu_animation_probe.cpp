#include "ds_port/original_menu_animation.hpp"
#include "ds_port/original_m3g_scene.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
int main(int argc,char** argv){
    if(argc!=2)return 2;
    std::ifstream in(argv[1],std::ios::binary);
    if(!in)return 3;
    std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
    if(bytes.size()>64u*1024u*1024u)return 4;
    const auto motion=ds_port::decode_original_menu_motion(
        bytes.data(),bytes.size());
    if(!motion.decoded){
        std::cerr<<"Original menu animation decode error: "<<motion.error<<"\n";
        return 5;
    }
    std::cout<<"Original menu motion: "<<motion.sequences.size()
             <<" sequences, "<<motion.controllers.size()
             <<" controllers, "<<motion.tracks.size()
             <<" scene node tracks\n";
    for(const auto& track:motion.tracks){
        std::cout<<"Original menu animated target "<<track.target_id
                 <<" property "<<track.property_id<<"\n";
    }
    const auto scene=ds_port::decode_original_m3g_scene(bytes.data(),bytes.size());
    if(!scene.decoded) {
        std::cerr<<"Original animation scene unavailable: "<<scene.error<<"\\n";
        return 7;
    }
    std::vector<ds_port::OriginalSceneNode> at_start,at_one_second;
    if(!ds_port::sample_original_menu_motion(
            motion,scene.nodes,0,at_start,true) ||
       !ds_port::sample_original_menu_motion(
            motion,scene.nodes,1000,at_one_second,true))
        return 8;
    std::size_t moved=0;
    for(std::size_t i=0;i<at_start.size();++i) {
        bool changed=at_start[i].rendering_enabled!=
            at_one_second[i].rendering_enabled ||
            std::fabs(at_start[i].alpha-at_one_second[i].alpha)>0.00001f;
        for(std::size_t j=0;j<16;++j)
            changed=changed ||
                std::fabs(at_start[i].local[j]-
                          at_one_second[i].local[j])>0.00001f;
        if(changed)++moved;
    }
    std::cout<<"Original animated scene nodes changed after 1000 ms: "
             <<moved<<"\\n";
    return motion.sequences.empty()||motion.tracks.empty()||!moved?6:0;
}
