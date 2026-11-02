#pragma once
#include "ds_port/im_menu_scene.hpp"
#include "ds_port/original_menu_draw_stream.hpp"
#include "ds_port/im_ui_image.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace ds_port {
struct OriginalMenuAnimationRuntime;
struct OriginalUiRuntime;
// Renderer inputs must be recovered from the original executable/scene.
// Arrays use OpenGL column-major convention; no generated default camera.
struct OriginalMenuGpuSurface {
    std::uint32_t appearance_object_index=0;
    std::uint32_t image_index=0; // index into supplied original decoded images
    bool use_texture=false;
    std::uint8_t blending=68;
    std::uint8_t alpha_threshold=0;
    bool depth_test=true,depth_write=true,color_write=true,alpha_write=true;
    std::uint8_t culling=160,winding=168;
    float depth_offset_factor=0.f,depth_offset_units=0.f;
};
struct OriginalMenuGpuMesh {
    std::uint32_t mesh_object_index=0;
    std::array<float,16> model_matrix{};
    bool visible=true;
    float alpha=1.f;
};
struct OriginalMenuGpuFrame {
    std::array<float,16> projection{};
    std::array<float,16> view{};
    std::vector<OriginalMenuGpuMesh> mesh_transforms;
    std::vector<OriginalMenuGpuSurface> material_bindings;
};
enum class OriginalMenuGpuStatus {
    Rendered, IncompleteOriginalScene, InvalidScene,
    GraphicsUnavailable, UserAborted
};
// Works only after original camera/scene/material resolution is supplied.
// This production native OpenGL renderer never invents game-space transforms.
OriginalMenuGpuStatus run_original_menu_gl(
    const ImMenuGeometry& geometry,
    const OriginalMenuDrawStream& stream,
    const std::vector<M3gRgbaImage>& original_images,
    const OriginalMenuGpuFrame& original_frame,
    std::string& diagnostic,
    const OriginalMenuAnimationRuntime* motion_runtime=nullptr,
    const OriginalUiRuntime* ui_runtime=nullptr);
bool validate_original_menu_gpu_frame(
    const ImMenuGeometry& geometry,const OriginalMenuDrawStream& stream,
    const std::vector<M3gRgbaImage>& images,
    const OriginalMenuGpuFrame& frame,std::string& reason);
}
