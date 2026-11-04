#include "ds_port/original_menu_scene_graph.hpp"
#include <cmath>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>
namespace ds_port {
namespace {
using Matrix=std::array<float,16>;
bool finite(const Matrix& a){for(float x:a)if(!std::isfinite(x))return false;return true;}
Matrix mul(const Matrix& a,const Matrix& b) {
    Matrix out{};
    for(int col=0;col<4;++col)
        for(int row=0;row<4;++row)
            for(int k=0;k<4;++k)
                out[col*4+row]+=a[k*4+row]*b[col*4+k];
    return out;
}
bool inverse(const Matrix& m,Matrix& result) {
    // Gauss-Jordan with pivoting (row-major scratch, OpenGL column-major I/O).
    double a[4][8]{};
    for(int r=0;r<4;++r) {
        for(int c=0;c<4;++c)a[r][c]=m[c*4+r];
        a[r][r+4]=1;
    }
    for(int col=0;col<4;++col) {
        int pivot=col;
        for(int r=col+1;r<4;++r)
            if(std::fabs(a[r][col])>std::fabs(a[pivot][col]))pivot=r;
        if(std::fabs(a[pivot][col])<1e-12)return false;
        for(int j=0;j<8;++j){const double v=a[col][j];a[col][j]=a[pivot][j];a[pivot][j]=v;}
        const double scale=a[col][col];
        for(int j=0;j<8;++j)a[col][j]/=scale;
        for(int r=0;r<4;++r)if(r!=col){
            const double factor=a[r][col];
            for(int j=0;j<8;++j)a[r][j]-=factor*a[col][j];
        }
    }
    for(int r=0;r<4;++r)for(int c=0;c<4;++c)
        result[c*4+r]=static_cast<float>(a[r][c+4]);
    return finite(result);
}
}
OriginalSceneTopology recover_original_menu_topology(const ImMenuGeometry& geometry) {
    OriginalSceneTopology out{};
    if(!geometry.decoded || geometry.group_object_indices.empty()) {
        out.error="Original menu group records unavailable";return out;
    }
    out.unresolved_groups=geometry.group_object_indices.size()-
        geometry.validated_group_children.size();
    std::unordered_map<std::uint32_t,std::uint32_t> parents;
    for(const auto& group:geometry.validated_group_children) {
        for(const auto child:group.children) {
            if(child==group.group_object_index ||
               !parents.emplace(child,group.group_object_index).second) {
                out.error="Original menu has duplicate parent or self-link";return out;
            }
        }
    }
    for(const auto& item:parents) {
        std::unordered_set<std::uint32_t> seen;
        auto node=item.first;
        while(parents.count(node)) {
            if(!seen.insert(node).second) {
                out.error="Original menu child hierarchy contains a cycle";
                return out;
            }
            node=parents.at(node);
        }
    }
    for(const auto& item:parents)out.child_parent.push_back(item);
    std::sort(out.child_parent.begin(),out.child_parent.end());
    out.valid=true;
    return out;
}
OriginalSceneResolveResult resolve_original_menu_scene(
    const ImMenuGeometry& geometry,const OriginalMenuDrawStream& stream,
    const std::vector<M3gRgbaImage>& images,
    const std::vector<OriginalSceneNode>& nodes,
    const OriginalSceneCamera& camera,
    const std::vector<OriginalSceneBinding>& material) {
    OriginalSceneResolveResult out{};
    auto fail=[&](const char* reason) {out.error=reason;out.frame={};return out;};
    if(!geometry.decoded||!stream.valid||!camera.node_id||
       nodes.empty()||material.empty()||!finite(camera.projection))
        return fail("Original hierarchy/camera/material input incomplete");
    std::unordered_map<std::uint32_t,const OriginalSceneNode*> by_id;
    for(const auto& node:nodes)
        if(!node.object_id||!finite(node.local)||
           !by_id.emplace(node.object_id,&node).second)
            return fail("Duplicate or invalid recovered scene node");
    if(!by_id.count(camera.node_id))
        return fail("Original camera node absent from recovered hierarchy");
    std::unordered_map<std::uint32_t,Matrix> world;
    std::unordered_map<std::uint32_t,bool> visible;
    std::unordered_map<std::uint32_t,float> alpha;
    std::unordered_map<std::uint32_t,int> color;
    std::function<bool(std::uint32_t)> visit=[&](std::uint32_t id)->bool {
        if(color[id]==2)return true;
        if(color[id]==1)return false;
        color[id]=1;
        const auto& node=*by_id.at(id);
        if(node.parent_id) {
            if(!by_id.count(node.parent_id)||!visit(node.parent_id))return false;
            world[id]=mul(world.at(node.parent_id),node.local);
            visible[id]=visible.at(node.parent_id)&&node.rendering_enabled;
            alpha[id]=alpha.at(node.parent_id)*node.alpha;
        }else {
            world[id]=node.local;
            visible[id]=node.rendering_enabled;
            alpha[id]=node.alpha;
        }
        color[id]=2;return finite(world.at(id)) &&
            std::isfinite(alpha.at(id)) && alpha.at(id)>=0.f && alpha.at(id)<=1.f;
    };
    for(const auto& node:nodes)
        if(!visit(node.object_id))return fail("Original scene has invalid parent link or cycle");
    out.frame.projection=camera.projection;
    if(!inverse(world.at(camera.node_id),out.frame.view))
        return fail("Original camera world matrix is singular");
    for(const auto& bounds:geometry.mesh_bounds) {
        if(!world.count(bounds.mesh_object_index))
            return fail("Original mesh missing recovered world hierarchy node");
        out.frame.mesh_transforms.push_back(
            {bounds.mesh_object_index,world.at(bounds.mesh_object_index),
             visible.at(bounds.mesh_object_index),alpha.at(bounds.mesh_object_index)});
    }
    std::unordered_set<std::uint32_t> seen;
    for(const auto& m:material) {
        if(!m.appearance_id||!seen.insert(m.appearance_id).second)
            return fail("Original material binding duplicated or invalid");
        OriginalMenuGpuSurface binding{};
        binding.appearance_object_index=m.appearance_id;
        binding.image_index=m.image_index;
        binding.use_texture=m.textured;
        binding.blending=m.blending;
        binding.alpha_threshold=m.alpha_threshold;
        binding.depth_test=m.depth_test;
        binding.depth_write=m.depth_write;
        binding.color_write=m.color_write;
        binding.alpha_write=m.alpha_write;
        binding.culling=m.culling;
        binding.winding=m.winding;
        binding.depth_offset_factor=m.depth_offset_factor;
        binding.depth_offset_units=m.depth_offset_units;
        out.frame.material_bindings.push_back(binding);
    }
    std::string validation;
    if(!validate_original_menu_gpu_frame(geometry,stream,images,out.frame,validation)){
        out.error=validation;out.frame={};return out;
    }
    out.valid=true;return out;
}
}
