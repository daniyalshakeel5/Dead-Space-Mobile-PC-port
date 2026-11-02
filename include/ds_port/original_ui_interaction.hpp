#pragma once
#include "ds_port/original_ui_sbin.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace ds_port {
// The application/translated UI logic supplies actual on-screen positions.
// The original SBIN supplies source sprite rectangles; these are not
// screen positions and must never be mistaken for clickable menu bounds.
struct OriginalUiDraw {
    std::uint32_t sprite_object_id=0;
    int x=0,y=0,width=0,height=0;
    bool interactive=false;
    // A helper hit rectangle has no texture; action_id identifies its owner.
    bool hit_only=false;
    std::uint32_t action_id=0;
};
// Layouts.xml.sb stores the game's actual 480x320 logical UI canvas.
// Center the overlay without stretching it in widescreen/resized windows.
struct OriginalUiViewport {
    int x=0,y=0,width=0,height=0; // top-left screen coordinates
};
OriginalUiViewport original_ui_viewport(int screen_width,int screen_height) noexcept;
// Screen events are in SDL window coordinates (not drawable pixels).
// Returns false for letterbox/pillarbox margins and out-of-window events.
bool original_ui_window_to_canvas(int window_width,int window_height,
    int pointer_x,int pointer_y,int& canvas_x,int& canvas_y) noexcept;
struct OriginalUiHit {
    bool found=false;
    std::uint32_t sprite_object_id=0;
};
struct OriginalUiInteractionState {
    std::uint32_t hovered=0,pressed=0;
};
enum class OriginalUiPointerAction {Move,Down,Up,Cancel};
struct OriginalUiPointerResult {
    std::uint32_t hovered=0,activated=0;
};
// The recovered engine UI layout/state machine supplies screen-space
// commands. Empty command sets leave the original 3D scene intact.
struct OriginalUiRuntime {
    const OriginalUiSbin* database=nullptr;
    const std::vector<M3gRgbaImage>* atlases=nullptr;
    const std::vector<OriginalUiDraw>* draw_commands=nullptr;
    void* context=nullptr;
    void (*on_activate)(void*,std::uint32_t)=nullptr;
    void (*on_hover)(void*,std::uint32_t)=nullptr;
    // Desktop fallback uses an actual recovered BTN_BACK entity when the
    // original native input queue is not yet operational.
    void (*on_back)(void*)=nullptr;
};
bool validate_original_ui_draw_list(
    const OriginalUiSbin& database,
    const std::vector<M3gRgbaImage>& atlases,
    const std::vector<OriginalUiDraw>& commands,std::string& reason);
// Desktop keyboard focus uses only original clickable layout objects,
 // in their recovered on-screen layout order. No fake menu entries.
std::uint32_t cycle_original_ui_focus(
    const std::vector<OriginalUiDraw>& commands,
    std::uint32_t current,bool backwards) noexcept;
OriginalUiHit hit_test_original_ui(
    const std::vector<OriginalUiDraw>& commands,int x,int y);
OriginalUiPointerResult dispatch_original_ui_pointer(
    OriginalUiInteractionState& state,
    const std::vector<OriginalUiDraw>& commands,
    OriginalUiPointerAction action,int x,int y);
// The only window-pixel -> 480x320 canvas conversion entry point for
// native frontend pointer events. This keeps hit testing consistent with
// the letterboxed renderer on widescreen and resized desktop windows.
// Outside-viewport clicks are treated as misses; Cancel clears any press.
OriginalUiPointerResult dispatch_original_ui_window_pointer(
    OriginalUiInteractionState& state,
    const std::vector<OriginalUiDraw>& commands,
    OriginalUiPointerAction action,int window_width,int window_height,
    int pointer_x,int pointer_y);
}
