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
    // Report only anonymous controller counts. This is real-APK
    // evidence to distinguish genuinely scheduled ambient animation
    // from the old force-enabled debug preview bug.
    const auto active_at=[&](std::int64_t t){
        std::size_t count=0;
        for(const auto& c:motion.controllers)
            if(c.weight>0.f && t>=c.active_start && t<c.active_end)
                ++count;
        return count;
    };
    std::cout<<"Original serialized M3G controller windows: active at 0 ms="
             <<active_at(0)<<", at 1000 ms="<<active_at(1000)
             <<", total="<<motion.controllers.size()<<"\\n";
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
    // Production path leaves inactive controllers inactive.
    std::vector<ds_port::OriginalSceneNode> enabled_start,enabled_one_second;
    if(!ds_port::sample_original_menu_motion(
            motion,scene.nodes,0,enabled_start,false) ||
       !ds_port::sample_original_menu_motion(
            motion,scene.nodes,1000,enabled_one_second,false))
        return 9;
    std::size_t naturally_moved=0;
    for(std::size_t i=0;i<enabled_start.size();++i) {
        bool change=enabled_start[i].rendering_enabled!=
            enabled_one_second[i].rendering_enabled ||
            std::fabs(enabled_start[i].alpha-enabled_one_second[i].alpha)>0.00001f;
        for(std::size_t j=0;j<16;++j)
            change=change ||
                std::fabs(enabled_start[i].local[j]-
                          enabled_one_second[i].local[j])>0.00001f;
        if(change)++naturally_moved;
    }
    std::cout<<"Original runtime-gated menu scene nodes changed after 1000 ms: "
             <<naturally_moved<<"\\n";
    if(naturally_moved>moved)
        return 10;
    return motion.sequences.empty()||motion.tracks.empty()||!moved?6:0;
}
