#include "ds_port/original_menu_gl.hpp"
#include <cassert>
using namespace ds_port;
int main(){
    ImMenuGeometry g{};
    g.decoded=true;g.mesh_count=1;
    g.vertices.resize(3);g.indices={0,1,2};
    g.mesh_bounds.push_back({12,0,0,0,1,1,1});
    OriginalMenuDrawStream d{};
    d.valid=true;d.batches.push_back({12,9,0,3});d.index_total=3;
    OriginalMenuGpuFrame f{};
    std::string reason;
    assert(!validate_original_menu_gpu_frame(g,d,{},f,reason));
    const std::array<float,16> identity={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    f.projection=identity;f.view=identity;
    f.mesh_transforms.push_back({12,identity});
    assert(!validate_original_menu_gpu_frame(g,d,{},f,reason));
    f.material_bindings.push_back({9,0,false});
    assert(validate_original_menu_gpu_frame(g,d,{},f,reason));
    f.material_bindings[0].use_texture=true;
    assert(!validate_original_menu_gpu_frame(g,d,{},f,reason));
    M3gRgbaImage img{};img.decoded=true;img.width=1;img.height=1;
    img.pixels_rgba={1,2,3,255};
    assert(validate_original_menu_gpu_frame(g,d,{img},f,reason));
    f.mesh_transforms.push_back({12,identity});
    assert(!validate_original_menu_gpu_frame(g,d,{img},f,reason));
    return 0;
}
