#include "ds_port/original_menu_scene_graph.hpp"
#include <cassert>
#include <cmath>
using namespace ds_port;
int main(){
    const std::array<float,16> identity={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    auto move=identity;move[12]=3.f;
    ImMenuGeometry g{};g.decoded=true;g.mesh_count=1;
    g.vertices.resize(3);g.indices={0,1,2};g.mesh_bounds.push_back({10,0,0,0,1,1,1});
    OriginalMenuDrawStream s{};s.valid=true;s.batches.push_back({10,8,0,3});
    std::vector<OriginalSceneNode> nodes={{4,0,move},{5,4,identity},{10,4,identity}};
    OriginalSceneCamera camera{5,identity};
    auto result=resolve_original_menu_scene(g,s,{},nodes,camera,{{8,0,false}});
    assert(result.valid);
    assert(result.frame.mesh_transforms[0].model_matrix[12]==3.f);
    assert(std::fabs(result.frame.view[12]+3.f)<1e-4f);
    nodes[0].parent_id=10;
    assert(!resolve_original_menu_scene(g,s,{},nodes,camera,{{8,0,false}}).valid);
    nodes[0].parent_id=0;nodes[1].parent_id=999;
    assert(!resolve_original_menu_scene(g,s,{},nodes,camera,{{8,0,false}}).valid);
    nodes[1].parent_id=4;camera.node_id=42;
    assert(!resolve_original_menu_scene(g,s,{},nodes,camera,{{8,0,false}}).valid);
    ImMenuGeometry topology{};
    topology.decoded=true;
    topology.group_object_indices={1,2,3};
    topology.validated_group_children={
        {1,{2,10}},{2,{3}}
    };
    auto graph=recover_original_menu_topology(topology);
    assert(graph.valid&&graph.unresolved_groups==1);
    assert(graph.child_parent.size()==3);
    topology.validated_group_children.push_back({3,{1}});
    assert(!recover_original_menu_topology(topology).valid);
    topology.validated_group_children.pop_back();
    topology.validated_group_children.push_back({3,{10}});
    assert(!recover_original_menu_topology(topology).valid);
    return 0;
}
