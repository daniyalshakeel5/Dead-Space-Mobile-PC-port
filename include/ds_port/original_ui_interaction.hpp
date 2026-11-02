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
};
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
};
bool validate_original_ui_draw_list(
    const OriginalUiSbin& database,
    const std::vector<M3gRgbaImage>& atlases,
    const std::vector<OriginalUiDraw>& commands,std::string& reason);
OriginalUiHit hit_test_original_ui(
    const std::vector<OriginalUiDraw>& commands,int x,int y);
OriginalUiPointerResult dispatch_original_ui_pointer(
    OriginalUiInteractionState& state,
    const std::vector<OriginalUiDraw>& commands,
    OriginalUiPointerAction action,int x,int y);
}
