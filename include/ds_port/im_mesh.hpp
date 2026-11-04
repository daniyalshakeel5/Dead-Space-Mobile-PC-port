#pragma once
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>

namespace ds_port {
// IronMonkey IM-M3G basic mesh/position/triangle-list subset only.
// Original input assets MUST be locally provided by the user.
// Does not parse proprietary materials, normals, UVs, animations or engine.
struct ImMeshReport {
    bool valid = false;
    std::string error;
    std::uint32_t mesh_objects = 0;
    std::uint32_t submeshes = 0;
    std::uint64_t vertices = 0;
    std::uint64_t triangles = 0;
};

// Passing nullptr only inspects decoded mesh geometry in memory.
// Passing stream emits independently authored Wavefront OBJ syntax.
// No original bytes/names are ever copied to returned reports.
ImMeshReport scan_im_mesh(const std::uint8_t* data, std::size_t size,
                          std::ostream* obj_out = nullptr);
}  // namespace ds_port
