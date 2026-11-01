#pragma once
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {
// Original 0x278E58 is receiver vtable +0x64 (verified actual vtable
// 0x00489670), invoked by 0x2786F0 with r1=1.
// 0x273570 releases existing tree nodes; those allocator/provider
// virtuals are still unresolved. A nonempty tree REQUIRES an
// independently reconstructed release callback.
using OriginalActivationTreeClear = bool (*)(
    std::uint32_t original_tree_arm_va,
    std::uint32_t original_root_arm_va,
    void* context) noexcept;
struct OriginalReceiverActivationInputs0x278e58 {
    std::uint32_t owner_arm_va=0;
    void* mapped_owner=nullptr;
    std::size_t owner_bytes=0;
    OriginalActivationTreeClear release_existing_tree=nullptr;
    void* context=nullptr;
};
enum class OriginalReceiverActivationStatus0x278e58 : std::uint8_t {
    InvalidOwner, MissingOriginalTreeDestructor, OriginalTreeReleaseRefused,
    CompletedEmptyProviderReset, UnreconstructedProviderActivation
};
struct OriginalReceiverActivationResult0x278e58 {
    OriginalReceiverActivationStatus0x278e58 status;
    std::uint32_t previous_tree_root;
    std::uint32_t original_provider_arm_va;
};
// Native 0x278E58 prefix: release old nodes if any, initialize
// owner fields [+8B6,+8C0,+8C4,+8C8,+8CC,+8D0], then dispatch the
// original +8AC provider branch (still unreconstructed when nonzero).
OriginalReceiverActivationResult0x278e58
original_execute_receiver_activation_0x278e58(
    const OriginalReceiverActivationInputs0x278e58&) noexcept;
} // namespace ds_port::original_runtime
