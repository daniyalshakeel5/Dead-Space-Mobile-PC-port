#pragma once
#include "ds_port/original_receiver_activation.hpp"
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {
// Original ARM 0x278778 is the binding stage after 0x27A768 and
// 0x26C410 in loader 0x286918. The constructor initializes
// owner+0x8B6=0, so the original first invocation branches to
// 0x278AAC and dispatches vtable+0x64 (verified 0x278E58) with r1=1.
// This implements the original NO-PROVIDER exit ONLY. The remaining
// binding operation is NOT a dummy resource insertion.
struct OriginalOwnerBindInputs0x278778 {
    std::uint32_t owner_arm_va=0;
    void* mapped_owner=nullptr;
    std::size_t owner_bytes=0;
    OriginalActivationTreeClear release_original_tree=nullptr;
    void* context=nullptr;
};
enum class OriginalOwnerBindStatus0x278778:std::uint8_t {
    InvalidOriginalOwner,
    UnreconstructedActiveTreeBinding,
    OriginalTreeActivationBlocked,
    ReachedOriginalNullProviderExit,
    UnreconstructedProviderBinding
};
struct OriginalOwnerBindResult0x278778 {
    OriginalOwnerBindStatus0x278778 status;
    OriginalReceiverActivationStatus0x278e58 activation_status;
    std::uint32_t original_provider_va;
};
OriginalOwnerBindResult0x278778 original_bind_resource_owner_0x278778(
    const OriginalOwnerBindInputs0x278778&) noexcept;
} // namespace ds_port::original_runtime
