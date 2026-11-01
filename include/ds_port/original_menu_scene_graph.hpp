#pragma once
#include "ds_port/original_menu_gl.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace ds_port {
// Hierarchy links, local matrices and camera projection must come from
// verified original scene decoding or actual native render traces.
struct OriginalSceneNode {
    std::uint32_t object_id=0;
    std::uint32_t parent_id=0; // zero = root
    std::array<float,16> local{};
};
struct OriginalSceneCamera {
    std::uint32_t node_id=0;
    std::array<float,16> projection{};
};
struct OriginalSceneBinding {
    std::uint32_t appearance_id=0;
    std::uint32_t image_index=0;
    bool textured=false;
};
struct OriginalSceneResolveResult {
    bool valid=false;
    std::string error;
    OriginalMenuGpuFrame frame;
};
OriginalSceneResolveResult resolve_original_menu_scene(
    const ImMenuGeometry& geometry,
    const OriginalMenuDrawStream& stream,
    const std::vector<M3gRgbaImage>& images,
    const std::vector<OriginalSceneNode>& recovered_nodes,
    const OriginalSceneCamera& recovered_camera,
    const std::vector<OriginalSceneBinding>& recovered_materials);
}
