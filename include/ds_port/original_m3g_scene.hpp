#pragma once
#include "ds_port/original_menu_scene_graph.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace ds_port {
// JSR-184 Object3D/Transformable/Node/Camera/Group/Appearance/Texture2D
// decoder for the original game's IM-M3G section framing. Unlike the
// older heuristic scanner these fields follow their documented grammar.
// Material images belong to main_menu.m3g, NOT ui_frontend.m3g.
struct OriginalM3gScene {
    bool decoded=false;
    std::string error;
    std::vector<OriginalSceneNode> nodes;
    OriginalSceneCamera camera;
    std::vector<OriginalSceneBinding> materials;
    std::vector<M3gRgbaImage> images;
    std::size_t groups=0;
    std::size_t group_links=0;
    std::size_t animation_references=0;
};
OriginalM3gScene decode_original_m3g_scene(
    const std::uint8_t* data,std::size_t size);
}
