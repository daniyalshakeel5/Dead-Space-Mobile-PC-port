#pragma once
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
// Original 0x283408: r0 = r0+4; returns a tree header/sentinel pointer.
// Original 0x2933EC: tree predecessor over original ARM32 nodes.
// The memory resolver provides mapped original nodes, NOT emulated ARM code.
struct OriginalTreeNodeSpan {
    const void* mapped_bytes=nullptr;
    std::size_t accessible_bytes=0;
};
using OriginalTreeNodeResolver=bool (*)(
    std::uint32_t original_node_va,void* context,
    OriginalTreeNodeSpan* mapped) noexcept;
enum class OriginalTreeWalkStatus:std::uint8_t {
    Completed,InvalidOriginalAddress,UnmappedNode,TraversalLimit
};
struct OriginalTreeWalkResult {
    OriginalTreeWalkStatus status;
    std::uint32_t original_node_va;
};
OriginalTreeWalkResult original_tree_header_0x283408(
    std::uint32_t tree_arm_va) noexcept;
OriginalTreeWalkResult original_tree_predecessor_0x2933ec(
    std::uint32_t node_arm_va,
    OriginalTreeNodeResolver resolver,void* context) noexcept;
} // namespace ds_port::original_runtime
