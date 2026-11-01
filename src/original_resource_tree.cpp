#include "ds_port/original_resource_tree.hpp"
namespace ds_port::original_runtime {
namespace {
std::uint32_t word(const unsigned char* p) noexcept {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
bool read_node(std::uint32_t va,OriginalTreeNodeResolver resolver,
               void* ctx,std::uint32_t* left,std::uint32_t* right,
               std::uint32_t* parent) noexcept {
    OriginalTreeNodeSpan node{};
    if(!va||!resolver||!resolver(va,ctx,&node)||
       !node.mapped_bytes||node.accessible_bytes<12)
        return false;
    const auto* p=static_cast<const unsigned char*>(node.mapped_bytes);
    *left=word(p);*right=word(p+4);*parent=word(p+8);
    return true;
}
}
OriginalTreeWalkResult original_tree_header_0x283408(
    std::uint32_t tree_arm_va) noexcept {
    if(!tree_arm_va||tree_arm_va>0xfffffffbu)
        return {OriginalTreeWalkStatus::InvalidOriginalAddress,0};
    return {OriginalTreeWalkStatus::Completed,tree_arm_va+4u};
}
OriginalTreeWalkResult original_tree_predecessor_0x2933ec(
    std::uint32_t current,OriginalTreeNodeResolver resolver,
    void* context) noexcept {
    using S=OriginalTreeWalkStatus;
    if(!current||!resolver)
        return {S::InvalidOriginalAddress,0};
    std::uint32_t left=0,right=0,parent=0;
    if(!read_node(current,resolver,context,&left,&right,&parent))
        return {S::UnmappedNode,0};
    // Exact original 0x2933EC: if left exists, descend to its
    // furthest-right descendant. Native bound rejects corrupt cycles.
    if(left) {
        current=left;
        for(unsigned count=0;count<65536u;++count) {
            if(!read_node(current,resolver,context,&left,&right,&parent))
                return {S::UnmappedNode,0};
            if(!right)return {S::Completed,current};
            current=right;
        }
        return {S::TraversalLimit,0};
    }
    // Original 0x29341C..0x29345C climbs parent links until the
    // current node is no longer its parent's left child.
    for(unsigned count=0;count<65536u;++count) {
        if(!parent)
            return {S::UnmappedNode,0};
        std::uint32_t pleft=0,pright=0,pparent=0;
        if(!read_node(parent,resolver,context,&pleft,&pright,&pparent))
            return {S::UnmappedNode,0};
        if(current!=pleft)return {S::Completed,parent};
        current=parent;
        parent=pparent;
    }
    return {S::TraversalLimit,0};
}
} // namespace ds_port::original_runtime
