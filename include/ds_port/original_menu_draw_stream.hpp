#pragma once
#include "ds_port/im_menu_scene.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace ds_port {
struct OriginalMenuDrawBatch {
    std::uint32_t mesh_object_index=0;
    std::uint32_t appearance_object_index=0;
    std::uint32_t first_index=0;
    std::uint32_t index_count=0;
};
struct OriginalMenuDrawStream {
    bool valid=false;
    std::string error;
    std::vector<OriginalMenuDrawBatch> batches;
    std::uint64_t index_total=0;
};
// Native renderer-facing source geometry. No guessed transforms/camera/materials.
OriginalMenuDrawStream prepare_original_menu_draw_stream(const ImMenuGeometry& scene);
}
