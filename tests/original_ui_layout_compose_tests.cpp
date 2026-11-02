#ifdef NDEBUG
#undef NDEBUG
#endif
#include "ds_port/original_ui_layout_compose.hpp"
#include <cassert>
#include <algorithm>
using namespace ds_port;
OriginalLayoutVertex v(float x,float y,float ax=0,float ay=0){
    return {x,y,ax,ay};
}
int main(){
    OriginalLayoutDatabase db{};db.decoded=true;
    OriginalLayoutScreen root{};root.name="UI_MAIN";
    OriginalLayoutEntity play{};play.name="BTN_PLAY";play.type="SUBLAYOUT";
    play.sublayout="UI_BUTTON_MAIN";
    play.vertices={v(10,-52,0,1),v(106,-12,0,1)};
    root.entities.push_back(play);db.screens.push_back(root);
    OriginalLayoutScreen btn{};btn.name="UI_BUTTON_MAIN";
    OriginalLayoutEntity off{};off.name="STORY_OFF";off.type="SUBLAYOUT";
    off.sublayout="UI_BUTTON_MAIN_OFF";
    off.vertices={v(4,4),v(103,44)};
    btn.entities.push_back(off);
    auto on=off;on.name="STORY_ON";on.sublayout="UI_BUTTON_MAIN_ON";
    btn.entities.push_back(on);
    OriginalLayoutEntity hit{};hit.name="BTN_REGION";hit.type="HELPERRECT";
    hit.vertices={v(0,0),v(100,35)};
    btn.entities.push_back(hit);db.screens.push_back(btn);
    for(const auto state:{"OFF","ON"}) {
        OriginalLayoutScreen layout{};layout.name=std::string("UI_BUTTON_MAIN_")+state;
        OriginalLayoutEntity background{};
        background.name="BTN";background.type="IMAGE";
        background.image=std::string("buttons/btn_main_btm_")+
            (state[1]=='F'?"off":"on")+".tif";
        background.width=109;background.height=31;
        background.vertices={v(0,0)};
        layout.entities.push_back(background);
        OriginalLayoutEntity icon{};
        icon.name="STORY";icon.type="IMAGE";
        icon.image=std::string("icons/icon_story_")+
            (state[1]=='F'?"off":"on")+".tif";
        icon.width=19;icon.height=21;icon.vertices={v(5,6)};
        layout.entities.push_back(icon);
        db.screens.push_back(layout);
    }
    OriginalUiSbin ui{};ui.decoded=true;
    for(int i=0;i<2;++i){
        for(const std::string key:{std::string("buttons/btn_main_btm_"),
                                   std::string("icons/icon_story_")}){
            ui.sprites.push_back({
                std::uint32_t(ui.sprites.size()+2),
                key+(i?"on":"off")+".tif",0,1,120,60,
                {0,0,120,60},{0,0,120,60}});
        }
    }
    const auto composed=compose_original_ui_screen(db,ui,"UI_MAIN");
    assert(composed.valid);
    assert(composed.actions.size()==1);
    assert(composed.draws.size()==3);
    M3gRgbaImage atlas{};atlas.decoded=true;
    atlas.width=128;atlas.height=64;atlas.pixels_rgba.resize(128*64*4);
    std::string error;
    assert(validate_original_ui_draw_list(db.screens.empty()?OriginalUiSbin{}:ui,
        {atlas},composed.draws,error));
    OriginalUiInteractionState pointer{};
    const auto action_id=composed.actions.begin()->first;
    assert(composed.actions.at(action_id)=="BTN_PLAY");
    auto down=dispatch_original_ui_pointer(pointer,composed.draws,
        OriginalUiPointerAction::Down,20,280);
    assert(down.hovered==action_id);
    auto up=dispatch_original_ui_pointer(pointer,composed.draws,
        OriginalUiPointerAction::Up,20,280);
    assert(up.activated==action_id);
    const auto hovering=compose_original_ui_screen(db,ui,"UI_MAIN",action_id);
    assert(hovering.valid&&hovering.draws.size()==3);
    assert(hovering.draws[0].sprite_object_id!=composed.draws[0].sprite_object_id);
    return 0;
}
