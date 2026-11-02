#ifdef NDEBUG
#undef NDEBUG
#endif
#include "ds_port/original_ui_layout.hpp"
#include "ds_port/original_ui_layout_compose.hpp"
#include "ds_port/original_ui_sbin.hpp"
#include "ds_port/im_ui_image.hpp"
#include "ds_port/original_ui_interaction.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include <cstdint>
#include <cassert>
#include <algorithm>
#include <unordered_set>
static std::vector<std::uint8_t> read_file(const char* path,std::size_t limit) {
    std::ifstream f(path,std::ios::binary|std::ios::ate);
    if(!f)return {};
    const auto n=f.tellg();
    if(n<=0||std::uint64_t(n)>limit)return {};
    f.seekg(0);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(n));
    if(!f.read(reinterpret_cast<char*>(bytes.data()),n))return {};
    return bytes;
}
int main(int argc,char** argv){
    if(argc!=3)return 2;
    const auto layouts=read_file(argv[1],8u*1024u*1024u);
    const auto atlas_data=read_file(argv[2],32u*1024u*1024u);
    if(layouts.empty()||atlas_data.empty())return 3;
    const auto ui_layout=ds_port::decode_original_layout_sbin(
        layouts.data(),layouts.size());
    const auto pack=ds_port::decode_im_ui_rgba_images(
        atlas_data.data(),atlas_data.size());
    if(!ui_layout.decoded||!pack.decoded){
        std::cerr<<"Original menu layout or image decode failed\\n";
        return 4;
    }
    const auto db=ds_port::decode_original_ui_sbin(
        pack.frontend_sbin.data(),pack.frontend_sbin.size());
    if(!db.decoded){
        std::cerr<<"Original sprite lookup failed: "<<db.error<<"\\n";
        return 5;
    }
    const auto main=ds_port::compose_original_ui_screen(
        ui_layout,db,"UI_MAIN");
    if(!main.valid){
        std::cerr<<"Original UI_MAIN composition failed: "<<main.error<<"\\n";
        return 6;
    }
    if(main.actions.size()<4 || main.draws.size()<12)return 7;
    std::string reason;
    if(!ds_port::validate_original_ui_draw_list(
            db,pack.images,main.draws,reason)){
        std::cerr<<"Original main-menu draw list invalid: "<<reason<<"\\n";
        return 8;
    }
    const auto viewport=ds_port::original_ui_viewport(1280,720);
    std::size_t clicked=0;
    for(const auto& action:main.actions){
        const auto hit=std::find_if(main.draws.begin(),main.draws.end(),
            [&](const ds_port::OriginalUiDraw& d){
                return d.interactive&&d.action_id==action.first;
            });
        if(hit==main.draws.end())return 9;
        const int x=hit->x+hit->width/2;
        const int y=hit->y+hit->height/2;
        if(x<0||x>=480||y<0||y>=320)continue;
        const int window_x=viewport.x+
            int((std::int64_t(x)*viewport.width+viewport.width/2)/480);
        const int window_y=viewport.y+
            int((std::int64_t(y)*viewport.height+viewport.height/2)/320);
        int cx=-1,cy=-1;
        if(!ds_port::original_ui_window_to_canvas(
                1280,720,window_x,window_y,cx,cy))return 10;
        ds_port::OriginalUiInteractionState state{};
        const auto down=ds_port::dispatch_original_ui_pointer(state,main.draws,
            ds_port::OriginalUiPointerAction::Down,cx,cy);
        const auto up=ds_port::dispatch_original_ui_pointer(state,main.draws,
            ds_port::OriginalUiPointerAction::Up,cx,cy);
        // If buttons overlap, the foremost original hit region wins.
        if(down.hovered==action.first && up.activated==action.first) {
            ++clicked;
            const auto hovered=ds_port::compose_original_ui_screen(
                ui_layout,db,"UI_MAIN",action.first);
            if(!hovered.valid)return 11;
        }
        std::cout<<"Original menu button "<<action.second<<" action_id "
                 <<action.first<<" clickable "<<(up.activated==action.first)
                 <<"\\n";
    }
    if(clicked<3)return 12;
    // Navigate every genuine UI_MAIN button in source layout order and
    // prove the keyboard traversal wraps exactly once, without fake IDs.
    std::uint32_t focused=0;
    std::unordered_set<std::uint32_t> visited;
    for(std::size_t i=0;i<main.actions.size();++i){
        focused=ds_port::cycle_original_ui_focus(main.draws,focused,false);
        if(!main.actions.count(focused) || !visited.insert(focused).second)
            return 14;
    }
    if(visited.size()!=main.actions.size() ||
       ds_port::cycle_original_ui_focus(main.draws,focused,false)==0)
        return 15;
    std::cout<<"Original APK keyboard menu traversal verified: "
             <<visited.size()<<" named UI_MAIN controls\\n";
    for(const char* screen:{"UI_PROFILE","UI_OPTIONS_MAIN","UI_EXTRAS"}){
        const auto sub=ds_port::compose_original_ui_screen(ui_layout,db,screen);
        std::cout<<"Original menu screen "<<screen<<" valid "<<sub.valid
                 <<" sprites "<<sub.draws.size()<<" controls "
                 <<sub.actions.size()<<"\\n";
        if(!sub.valid)return 13;
        const auto back=std::find_if(sub.actions.begin(),sub.actions.end(),
            [](const auto& action){return action.second=="BTN_BACK";});
        std::cout<<"Original submenu "<<screen
                 <<" Back control "<<(back!=sub.actions.end())<<"\\n";
        if(back==sub.actions.end())return 16;
    }
    std::cout<<"Original APK native menu click mapping verified: "<<clicked
             <<" buttons, 480x320 original UI canvas, "
             <<main.draws.size()<<" sprite/hit commands\\n";
    return 0;
}
