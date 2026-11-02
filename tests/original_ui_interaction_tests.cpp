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
    // Real frontend canvas is 480x320. The window starts at 1280x720,
    // so clicks must be converted from SDL window pixels before hit tests.
    const auto wide=original_ui_viewport(1280,720);
    assert(wide.x==100&&wide.y==0&&wide.width==1080&&wide.height==720);
    int cx=-1,cy=-1;
    assert(original_ui_window_to_canvas(1280,720,640,360,cx,cy));
    assert(cx==240&&cy==160);
    assert(!original_ui_window_to_canvas(1280,720,99,360,cx,cy));
    assert(!original_ui_window_to_canvas(1280,720,1180,360,cx,cy));
    assert(original_ui_window_to_canvas(1280,720,1179,719,cx,cy));
    assert(cx==479&&cy==319);
    assert(original_ui_window_to_canvas(1280,720,123,46,cx,cy));
    assert(hit_test_original_ui(sprites,cx,cy).sprite_object_id==2);
    const auto square=original_ui_viewport(800,800);
    assert(square.x==0&&square.y==133&&square.width==800&&square.height==533);
    assert(!original_ui_window_to_canvas(800,800,10,100,cx,cy));
    assert(original_ui_window_to_canvas(800,800,400,400,cx,cy));
    assert(cx==240&&cy==160);
    const auto tall=original_ui_viewport(720,1280);
    assert(tall.x==0&&tall.y==400&&tall.width==720&&tall.height==480);
    assert(!original_ui_window_to_canvas(720,1280,360,399,cx,cy));
    assert(original_ui_window_to_canvas(720,1280,360,640,cx,cy));
    assert(cx==240&&cy==160);
    assert(original_ui_viewport(0,720).width==0);
    assert(!original_ui_window_to_canvas(0,720,0,0,cx,cy));
    // Regression: the renderer previously pre-scaled SDL coordinates to
    // 480x320 and the UI dispatcher scaled them a second time. Test the
    // complete window-event input path at several real window sizes.
    OriginalUiInteractionState desktop{};
    const auto move=dispatch_original_ui_window_pointer(
        desktop,sprites,OriginalUiPointerAction::Move,1280,720,140,60);
    assert(move.hovered==2);
    auto pressed=dispatch_original_ui_window_pointer(
        desktop,sprites,OriginalUiPointerAction::Down,1280,720,140,60);
    assert(pressed.hovered==2 && desktop.pressed==2);
    auto clicked=dispatch_original_ui_window_pointer(
        desktop,sprites,OriginalUiPointerAction::Up,1280,720,140,60);
    assert(clicked.activated==2 && desktop.pressed==0);
    // A press within a button followed by release in the black margin
    // must not invoke that original menu action.
    dispatch_original_ui_window_pointer(
        desktop,sprites,OriginalUiPointerAction::Down,1280,720,140,60);
    clicked=dispatch_original_ui_window_pointer(
        desktop,sprites,OriginalUiPointerAction::Up,1280,720,1200,60);
    assert(clicked.activated==0 && clicked.hovered==0);
    // Horizontal and vertical letterboxing, plus a resized viewport.
    assert(dispatch_original_ui_window_pointer(
        desktop,sprites,OriginalUiPointerAction::Move,800,800,30,180)
        .hovered==2);
    assert(dispatch_original_ui_window_pointer(
        desktop,sprites,OriginalUiPointerAction::Move,720,1280,25,445)
        .hovered==2);
    assert(dispatch_original_ui_window_pointer(
        desktop,sprites,OriginalUiPointerAction::Move,720,1280,25,399)
        .hovered==0);
    dispatch_original_ui_window_pointer(
        desktop,sprites,OriginalUiPointerAction::Down,480,320,15,25);
    assert(desktop.pressed==2);
    dispatch_original_ui_window_pointer(
        desktop,sprites,OriginalUiPointerAction::Cancel,0,0,0,0);
    assert(desktop.pressed==0 && desktop.hovered==0);
    clicked=dispatch_original_ui_window_pointer(
        desktop,sprites,OriginalUiPointerAction::Up,480,320,15,25);
    assert(clicked.activated==0);
    assert(dispatch_original_ui_window_pointer(
        desktop,sprites,OriginalUiPointerAction::Move,0,0,100,100)
        .hovered==0);
    // Keyboard navigation follows recovered clickable layout actions,
    // skips decorative sprites and wraps in both directions.
    const std::vector<OriginalUiDraw> navigation={
        {0,0,0,20,20,true,true,11},
        {2,0,0,20,20,false,false,0},
        {0,20,0,20,20,true,true,22},
        {0,40,0,20,20,true,true,33},
        {0,41,0,20,20,true,true,33}
    };
    assert(cycle_original_ui_focus(navigation,0,false)==11);
    assert(cycle_original_ui_focus(navigation,0,true)==33);
    assert(cycle_original_ui_focus(navigation,11,false)==22);
    assert(cycle_original_ui_focus(navigation,33,false)==11);
    assert(cycle_original_ui_focus(navigation,11,true)==33);
    assert(cycle_original_ui_focus({},0,false)==0);
    sprites[0].sprite_object_id=999;
    assert(!validate_original_ui_draw_list(db,{atlas},sprites,error));
    return 0;
}
