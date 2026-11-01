#include "ds_port/original_menu_draw_stream.hpp"
#include <cassert>
using namespace ds_port;
int main() {
    ImMenuGeometry s{};
    s.decoded=true;s.mesh_count=1;
    s.vertices.resize(3);s.indices={0,1,2};
    s.mesh_bounds.push_back({17,0,0,0,1,1,1});
    s.surfaces.push_back({17,21,0,1});
    auto d=prepare_original_menu_draw_stream(s);
    assert(d.valid && d.batches.size()==1 && d.index_total==3);
    assert(d.batches[0].appearance_object_index==21);
    s.indices[2]=3;
    assert(!prepare_original_menu_draw_stream(s).valid);
    s.indices[2]=2;s.surfaces[0].first_index=1;
    assert(!prepare_original_menu_draw_stream(s).valid);
    s.surfaces[0].first_index=0;s.surfaces[0].mesh_object_index=99;
    assert(!prepare_original_menu_draw_stream(s).valid);
    return 0;
}
