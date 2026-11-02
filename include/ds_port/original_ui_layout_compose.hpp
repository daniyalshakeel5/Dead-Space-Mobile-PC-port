#pragma once
#include "ds_port/original_ui_layout.hpp"
#include "ds_port/original_ui_interaction.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
namespace ds_port {
struct OriginalUiComposedScreen {
    bool valid=false;
    std::string error;
    std::vector<OriginalUiDraw> draws;
    // Stable action IDs identify original named button entities, not
    // atlas sprite IDs shared by several buttons.
    std::unordered_map<std::uint32_t,std::string> actions;
    std::size_t missing_original_images=0,unsupported_primitives=0;
};
// Entity coordinates come exclusively from original layouts.xml.sb.
// The source data defines a 480x320 UI canvas and nested sublayout bounds.
OriginalUiComposedScreen compose_original_ui_screen(
    const OriginalLayoutDatabase& layout,
    const OriginalUiSbin& atlas,
    const std::string& root_screen,
    std::uint32_t hovered_action=0);
}
