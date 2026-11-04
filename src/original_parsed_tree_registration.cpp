#include "ds_port/original_parsed_tree_registration.hpp"
namespace ds_port::original_runtime {
namespace {
std::uint32_t word(const void* ptr) noexcept {
    const auto* p=static_cast<const std::uint8_t*>(ptr);
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
}
OriginalParsedTreeResult original_register_parsed_trees_0x286918(
    const OriginalParsedTreeRegistrationInputs& in) noexcept {
    using S=OriginalParsedTreeStatus;
    auto fail=[](S status,unsigned count,unsigned passes) noexcept {
        return OriginalParsedTreeResult{status,count,passes};
    };
    if(!in.original_parsed_arm_va||in.original_parsed_arm_va>0xffffff77u||
       !in.parsed_host_bytes||in.parsed_accessible_bytes<0x88u||
       !in.original_registration_context_arm_va||
       !in.resolve_original_node||!in.native_registration)
        return fail(S::InvalidOriginalState,0,0);
    const auto* bytes=static_cast<const std::uint8_t*>(in.parsed_host_bytes);
    // Original 0x283408(container) returns container+4. Its +8
    // contains the first node (r7); original ADD r8,r0,#4 means
    // the sentinel is container+8, not container+4.
    // Original callsites BL 0x284178 are exactly:
    // 0x2869A8, 0x2869E4, 0x286A20.
    constexpr std::uint32_t offsets[3]={0x28u,0x50u,0x78u};
    constexpr std::uint32_t callsites[3]={
        0x002869a8u,0x002869e4u,0x00286a20u};
    std::uint32_t count=0;
    for(unsigned pass=0;pass<3;++pass) {
        const auto container=in.original_parsed_arm_va+offsets[pass];
        const auto header=original_tree_header_0x283408(container);
        if(header.status!=OriginalTreeWalkStatus::Completed)
            return fail(S::InvalidTreePointer,count,pass);
        const std::uint32_t sentinel=header.original_node_va+4u;
        std::uint32_t current=word(bytes+offsets[pass]+0x0cu);
        if(!current)return fail(S::InvalidTreePointer,count,pass);
        // Bound hostile or malformed original-asset trees. Native tests
        // exercise this with mapped original ARM node layouts.
        unsigned steps=0;
        while(current!=sentinel) {
            if(++steps>65536u)
                return fail(S::TraversalLimit,count,pass);
            OriginalTreeNodeSpan node{};
            if(!in.resolve_original_node(current,in.node_context,&node)||
               !node.mapped_bytes||node.accessible_bytes<0x28u)
                return fail(S::UnmappedOriginalNode,count,pass);
            const auto* n=static_cast<const std::uint8_t*>(node.mapped_bytes);
            const auto source=word(n+0x10);
            const auto descriptor=word(n+0x24);
            if(!source||!descriptor)
                return fail(S::OriginalRegistrationBlocked,count,pass);
            const OriginalRegistrationCall original{
                kOriginalStartupRegistrationHelperVA,
                callsites[pass],
                in.original_registration_context_arm_va,
                source,descriptor,false
            };
            if(original_execute_resource_registration_0x284178(
                original,*in.native_registration)!=
               OriginalNativeRegistrationStatus::CompletedOriginalPath)
                return fail(S::OriginalRegistrationBlocked,count,pass);
            ++count;
            const auto next=original_tree_predecessor_0x2933ec(
                current,in.resolve_original_node,in.node_context);
            if(next.status!=OriginalTreeWalkStatus::Completed||
               !next.original_node_va||next.original_node_va==current)
                return fail(S::OriginalNodeTraversalBlocked,count,pass);
            current=next.original_node_va;
        }
    }
    return {S::Completed,count,3u};
}
} // namespace ds_port::original_runtime
