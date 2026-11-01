#include "ds_port/original_menu_draw_stream.hpp"
#include <limits>
namespace ds_port {
OriginalMenuDrawStream prepare_original_menu_draw_stream(const ImMenuGeometry& scene) {
    OriginalMenuDrawStream out;
    auto fail=[&](const char* message) {
        out.error=message;
        out.valid=false;
        out.batches.clear();
        out.index_total=0;
        return out;
    };
    if(!scene.decoded || scene.vertices.empty() || scene.indices.empty() ||
       scene.surfaces.empty() || scene.mesh_bounds.size()!=scene.mesh_count)
        return fail("original menu geometry not ready");
    std::uint64_t expected=0;
    for(const auto& part:scene.surfaces) {
        const std::uint64_t n=std::uint64_t(part.triangle_count)*3;
        if(!n || part.first_index!=expected || n>scene.indices.size()-expected ||
           part.original_appearance_index==0 || part.mesh_object_index==0)
            return fail("invalid original menu surface index span");
        bool known_mesh=false;
        for(const auto& bounds:scene.mesh_bounds)
            if(bounds.mesh_object_index==part.mesh_object_index) {
                known_mesh=true;
                break;
            }
        if(!known_mesh)return fail("surface mesh ID missing");
        for(std::uint64_t j=0;j<n;++j)
            if(scene.indices[expected+j]>=scene.vertices.size())
                return fail("original menu triangle index out of bounds");
        if(n>std::numeric_limits<std::uint32_t>::max())
            return fail("draw span too long");
        out.batches.push_back({part.mesh_object_index,
            part.original_appearance_index,part.first_index,
            static_cast<std::uint32_t>(n)});
        expected+=n;
    }
    if(expected!=scene.indices.size())
        return fail("unclaimed original menu indices");
    out.index_total=expected;
    out.valid=true;
    return out;
}
}
