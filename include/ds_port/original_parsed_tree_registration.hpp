#pragma once
#include "ds_port/original_resource_registration.hpp"
#include "ds_port/original_resource_tree.hpp"
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
// Verified original 0x286918 ARM register/tree body (user APK).
// This is the resource-tree *registration phase*, not 0x286918's
// still-unported parsing, allocation, and final teardown operations.
// Original 0x2869C0, 0x2869FC: three passes over containers at
// parsed+0x28, parsed+0x50, parsed+0x78.
struct OriginalParsedTreeRegistrationInputs {
    std::uint32_t original_parsed_arm_va=0;
    const void* parsed_host_bytes=nullptr;
    std::size_t parsed_accessible_bytes=0;
    std::uint32_t original_registration_context_arm_va=0;
    OriginalTreeNodeResolver resolve_original_node=nullptr;
    void* node_context=nullptr;
    const OriginalNativeRegistrationServices* native_registration=nullptr;
};
enum class OriginalParsedTreeStatus:std::uint8_t {
    InvalidOriginalState,InvalidTreePointer,UnmappedOriginalNode,
    OriginalNodeTraversalBlocked,OriginalRegistrationBlocked,
    TraversalLimit,Completed
};
struct OriginalParsedTreeResult {
    OriginalParsedTreeStatus status;
    std::uint32_t registered_nodes;
    std::uint32_t completed_passes;
};
OriginalParsedTreeResult original_register_parsed_trees_0x286918(
    const OriginalParsedTreeRegistrationInputs&) noexcept;
} // namespace ds_port::original_runtime
