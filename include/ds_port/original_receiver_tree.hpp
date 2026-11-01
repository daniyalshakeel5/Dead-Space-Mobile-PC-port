#pragma once
#include "ds_port/original_resource_tree.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ds_port::original_runtime {
// Exact bounded ARM original 0x2710A8: tree header [tree+4],
// root [tree+12], node left/right pointers at +0/+4,
// UTF-16 [begin,end) original pointers at node+0x10/+0x14.
// Compared by original 0x26E670, unsigned 16-bit lexical order.
struct OriginalReceiverWideSpan {
    const std::uint8_t* mapped_bytes=nullptr;
    std::size_t accessible_bytes=0;
};
using OriginalReceiverWideResolver=bool (*)(
    std::uint32_t original_start_va,void* context,
    OriginalReceiverWideSpan* mapped) noexcept;
struct OriginalReceiverTreeLookupInputs {
    std::uint32_t original_tree_arm_va=0;
    const void* mapped_tree=nullptr;
    std::size_t tree_accessible_bytes=0;
    OriginalTreeNodeResolver resolve_node=nullptr;
    OriginalReceiverWideResolver resolve_utf16=nullptr;
    void* context=nullptr;
};
enum class OriginalReceiverTreeStatus : std::uint8_t {
    Found,NotFound,InvalidTree,UnmappedNode,UnmappedUtf16,
    InvalidOriginalUtf16Range,TraversalLimit
};
struct OriginalReceiverTreeResult {
    OriginalReceiverTreeStatus status;
    std::uint32_t original_node_arm_va;
};
// Original 0x26E670 UTF-16 unsigned lexicographic compare.
// The original length-breaker returns -1/0/+1.
int original_compare_utf16_0x26e670(
    std::u16string_view left,std::u16string_view right) noexcept;
OriginalReceiverTreeResult original_find_utf16_tree_0x2710a8(
    const OriginalReceiverTreeLookupInputs&,
    std::u16string_view original_resource_key) noexcept;
} // namespace ds_port::original_runtime
