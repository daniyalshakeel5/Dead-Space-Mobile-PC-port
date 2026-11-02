#include "ds_port/original_ui_layout.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include <string>
int main(int argc,char** argv){
    if(argc!=2)return 2;
    std::ifstream in(argv[1],std::ios::binary);
    if(!in)return 3;
    std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
    const auto db=ds_port::decode_original_layout_sbin(bytes.data(),bytes.size());
    if(!db.decoded){
        std::cerr<<"Original UI layout decode failed: "<<db.error<<"\n";
        return 4;
    }
    std::cout<<"Original UI layouts: "<<db.screens.size()
             <<", names "<<db.names.size()<<", references "
             <<db.references.size()<<"\n";
    for(const auto& layout:db.screens){
        if(layout.name!="UI_MAIN"&&layout.name!="UI_BUTTON_MAIN"&&
           layout.name!="UI_BUTTON_MAIN_OFF"&&layout.name!="UI_BUTTON_MAIN_ON"&&
           layout.name!="UI_PROFILE"&&layout.name!="UI_OPTIONS_MAIN"&&
           layout.name!="UI_EXTRAS"&&layout.name!="UI_BUTTON_MAIN_TOP"&&
           layout.name!="UI_BUTTON_STRETCH")continue;
        std::cout<<"Original UI layout "<<layout.name<<" entities "
                 <<layout.entities.size()<<"\n";
        for(const auto& e:layout.entities){
            std::cout<<"  entity "<<e.name<<" type "<<e.type
                     <<" sprite "<<e.image<<" nested "<<e.sublayout
                     <<" visible "<<e.visible<<" width "<<e.width
                     <<" height "<<e.height<<" vertices "<<e.vertices.size();
            for(const auto& v:e.vertices)
                std::cout<<" ["<<v.x<<","<<v.y
                         <<";"<<v.anchor_x<<","<<v.anchor_y<<"]";
            std::cout<<"\n";
        }
    }
    return 0;
}
