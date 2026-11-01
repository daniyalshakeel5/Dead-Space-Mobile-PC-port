#include "ds_port/original_resource_tree.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
using namespace ds_port::original_runtime;
namespace {
void put(std::uint8_t* p,unsigned v){
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(v>>(i*8));
}
struct Nodes {
    std::array<std::uint8_t,12> root{},left{},right{},sentinel{};
    bool missing_right=false;
};
bool map(std::uint32_t va,void* raw,OriginalTreeNodeSpan* out) noexcept{
    auto& n=*static_cast<Nodes*>(raw);
    if(va==0x2000)*out={n.root.data(),n.root.size()};
    else if(va==0x3000)*out={n.left.data(),n.left.size()};
    else if(va==0x4000&&!n.missing_right)*out={n.right.data(),n.right.size()};
    else if(va==0x1004)*out={n.sentinel.data(),n.sentinel.size()};
    else return false;
    return true;
}
}
int main(){
    auto h=original_tree_header_0x283408(0x1000);
    assert(h.status==OriginalTreeWalkStatus::Completed);
    assert(h.original_node_va==0x1004u);
    assert(original_tree_header_0x283408(0xfffffffeu).status==
           OriginalTreeWalkStatus::InvalidOriginalAddress);
    Nodes n{};
    // Three-node in-order tree: left <- root -> right; parents mapped.
    put(n.root.data(),0x3000);put(n.root.data()+4,0x4000);
    put(n.root.data()+8,0x1004);
    put(n.left.data()+8,0x2000);
    put(n.right.data()+8,0x2000);
    put(n.sentinel.data(),0x2000);
    auto p=original_tree_predecessor_0x2933ec(0x2000,map,&n);
    assert(p.status==OriginalTreeWalkStatus::Completed&&
           p.original_node_va==0x3000);
    p=original_tree_predecessor_0x2933ec(0x4000,map,&n);
    assert(p.status==OriginalTreeWalkStatus::Completed&&
           p.original_node_va==0x2000);
    // A right descendant of the left subtree wins over that left child.
    put(n.left.data()+4,0x4000);
    p=original_tree_predecessor_0x2933ec(0x2000,map,&n);
    assert(p.status==OriginalTreeWalkStatus::Completed&&
           p.original_node_va==0x4000);
    // Host mappings reject missing/corrupt original node references.
    n.missing_right=true;
    p=original_tree_predecessor_0x2933ec(0x2000,map,&n);
    assert(p.status==OriginalTreeWalkStatus::UnmappedNode);
    return 0;
}
