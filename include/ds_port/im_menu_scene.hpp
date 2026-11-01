#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ds_port {
// Authentically decoded IM-M3G menu vertices and texture coordinates.
// These carry original per-mesh geometry and Appearance references.
// NO camera, animation, transform graph or material/texture binding
// is claimed until recovered from the user's original game.
struct ImMenuVertex {
    float x=0,y=0,z=0,u=0,v=0;
};
struct ImMenuSurface {
    std::uint32_t mesh_object_index=0;
    std::uint32_t original_appearance_index=0;
    std::uint32_t first_index=0;
    std::uint32_t triangle_count=0;
};
// Local-space bounds only; world transforms and camera are not implied.
struct ImMenuMeshBounds {
    std::uint32_t mesh_object_index=0;
    float min_x=0,min_y=0,min_z=0;
    float max_x=0,max_y=0,max_z=0;
};
struct ImMenuGeometry {
    bool decoded=false;
    std::string error;
    std::uint32_t mesh_count=0;
    std::vector<ImMenuVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<ImMenuSurface> surfaces;
    std::vector<ImMenuMeshBounds> mesh_bounds;
    // Raw original IM-M3G object-type histogram; type semantics unverified.
    std::array<std::uint32_t,256> original_object_type_counts{};
};
ImMenuGeometry decode_im_menu_geometry(
    const std::uint8_t* original_im_m3g,std::size_t bytes);
} // namespace ds_port
