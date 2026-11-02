#include "ds_port/original_ui_interaction.hpp"
#include <algorithm>
#include <limits>
#include <unordered_set>
namespace ds_port {
bool validate_original_ui_draw_list(
    const OriginalUiSbin& db,
    const std::vector<M3gRgbaImage>& atlases,
    const std::vector<OriginalUiDraw>& list,std::string& error){
    if(!db.decoded) {error="Original sprite database missing";return false;}
    std::unordered_set<std::uint32_t> known;
    for(const auto& sprite:db.sprites)
        known.insert(sprite.object_id);
    if(list.size()>4096) {error="Too many original UI draw items";return false;}
    for(const auto& draw:list) {
        if(draw.hit_only) {
            if(!draw.interactive || !draw.action_id) {
                error="Original UI hit region missing action owner";return false;
            }
        }else if(!known.count(draw.sprite_object_id)) {
            error="Original UI sprite ID absent from recovered atlas";return false;
        }
        if(draw.width<=0||draw.height<=0||draw.width>16384||
           draw.height>16384 || draw.x< -16384 || draw.y< -16384 ||
           draw.x>16384 || draw.y>16384){
            error="Original UI screen rectangle out of bounds";return false;
        }
        if(!draw.hit_only) {
            const auto sprite=std::find_if(db.sprites.begin(),db.sprites.end(),
                [&](const OriginalUiSprite& s){return s.object_id==draw.sprite_object_id;});
            if(sprite->atlas>=atlases.size()||!atlases[sprite->atlas].decoded){
                error="Original UI texture atlas unavailable";return false;
            }
        }
    }
    error.clear();return true;
}
OriginalUiHit hit_test_original_ui(
    const std::vector<OriginalUiDraw>& list,int x,int y){
    // Reverse draw order makes the foremost visible widget receive input.
    for(auto it=list.rbegin();it!=list.rend();++it){
        const auto& d=*it;
        if(!d.interactive||d.width<=0||d.height<=0)continue;
        if(std::int64_t(x)>=d.x && std::int64_t(x)<std::int64_t(d.x)+d.width &&
           std::int64_t(y)>=d.y && std::int64_t(y)<std::int64_t(d.y)+d.height)
            return {true,d.action_id?d.action_id:d.sprite_object_id};
    }
    return {};
}
OriginalUiPointerResult dispatch_original_ui_pointer(
    OriginalUiInteractionState& state,
    const std::vector<OriginalUiDraw>& list,
    OriginalUiPointerAction action,int x,int y){
    const auto hit=hit_test_original_ui(list,x,y);
    const auto id=hit.found?hit.sprite_object_id:0u;
    if(action==OriginalUiPointerAction::Cancel){
        state.hovered=0;state.pressed=0;return {};
    }
    state.hovered=id;
    if(action==OriginalUiPointerAction::Down)
        state.pressed=id;
    if(action==OriginalUiPointerAction::Up) {
        const auto activated=(id && state.pressed==id)?id:0u;
        state.pressed=0;
        return {state.hovered,activated};
    }
    return {state.hovered,0};
}
}
