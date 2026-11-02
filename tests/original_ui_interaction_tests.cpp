#ifdef NDEBUG
#undef NDEBUG
#endif
#include "ds_port/original_ui_interaction.hpp"
#include <cassert>
using namespace ds_port;
int main() {
    OriginalUiSbin db{};db.decoded=true;
    db.sprites={
        {2,"framework_mainmenu/button.tif",0,1,8,8,{0,0,8,8},{0,0,8,8}},
        {5,"framework_mainmenu/overlay.tif",0,1,8,8,{0,0,8,8},{8,0,8,8}}
    };
    M3gRgbaImage atlas{};atlas.decoded=true;
    atlas.width=16;atlas.height=8;atlas.pixels_rgba.assign(16*8*4,255);
    std::vector<OriginalUiDraw> sprites={
        {2,10,20,100,40,true},{5,60,20,40,40,true}
    };
    std::string error;
    assert(validate_original_ui_draw_list(db,{atlas},sprites,error));
    assert(hit_test_original_ui(sprites,65,25).sprite_object_id==5);
    assert(hit_test_original_ui(sprites,15,25).sprite_object_id==2);
    assert(!hit_test_original_ui(sprites,1,1).found);
    OriginalUiInteractionState state{};
    assert(dispatch_original_ui_pointer(
        state,sprites,OriginalUiPointerAction::Move,15,25).hovered==2);
    dispatch_original_ui_pointer(state,sprites,OriginalUiPointerAction::Down,15,25);
    auto release=dispatch_original_ui_pointer(
        state,sprites,OriginalUiPointerAction::Up,15,25);
    assert(release.activated==2);
    dispatch_original_ui_pointer(state,sprites,OriginalUiPointerAction::Down,15,25);
    assert(!dispatch_original_ui_pointer(
        state,sprites,OriginalUiPointerAction::Up,65,25).activated);
    dispatch_original_ui_pointer(state,sprites,OriginalUiPointerAction::Down,65,25);
    dispatch_original_ui_pointer(state,sprites,OriginalUiPointerAction::Cancel,0,0);
    assert(!state.pressed&&!state.hovered);
    sprites[0].sprite_object_id=999;
    assert(!validate_original_ui_draw_list(db,{atlas},sprites,error));
    return 0;
}
