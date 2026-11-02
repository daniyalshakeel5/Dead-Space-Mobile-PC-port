#include "ds_port/original_menu_animation.hpp"
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
    return motion.sequences.empty()||motion.tracks.empty()?6:0;
}
