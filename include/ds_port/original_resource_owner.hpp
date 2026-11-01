#pragma once
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {

// Addresses obtained from the user's identical original APK on GitHub
// Actions run 41335804052: original 0x27A768, embedded 0x27A4F0.
inline constexpr std::uint32_t kOriginalOwnerVtable0x27a768=0x00489670u;
inline constexpr std::uint32_t kOriginalEmbeddedVtable0x27a4f0=0x004896f8u;
inline constexpr std::uint32_t kOriginalEmbeddedAllocatorSlot=0x0049bca8u;
inline constexpr std::size_t kOriginalOwnerObjectBytes=0x8fcu;

// Original 0x271914 default pool is required ONLY when original r2==0.
using OriginalOwnerDefaultPool=bool (*)(
    std::uint32_t original_callee_va,void* context,
    std::uint32_t* original_pool_va) noexcept;
// Original provider vtable +0x14 is required only when r1!=0.
// Its returned r0 bit 1 initializes owner byte +0x8B4.
using OriginalOwnerProviderFlags=bool (*)(
    std::uint32_t original_provider_va,std::uint32_t original_slot,
    void* context,std::uint32_t* original_r0) noexcept;
struct OriginalOwnerConstructorInputs0x27a768 {
    std::uint32_t owner_arm_va=0;
    void* mapped_owner=nullptr;
    std::size_t owner_bytes=0;
    const void* mapped_allocator_global_word=nullptr; // ELF 0x49BCA8
    std::size_t mapped_allocator_global_bytes=0;
    std::uint32_t original_pool_arm_va=0; // original r2
    std::uint32_t original_provider_arm_va=0; // original r1
    OriginalOwnerDefaultPool default_pool_0x271914=nullptr;
    OriginalOwnerProviderFlags provider_flags_0x14=nullptr;
    void* context=nullptr;
};
enum class OriginalOwnerConstructorStatus0x27a768:std::uint8_t {
    Completed,InvalidMemory,MissingOriginalAllocator,
    MissingDefaultPool,OriginalDefaultPoolRefused,
    MissingOriginalProviderMethod,OriginalProviderRefused
};
struct OriginalOwnerConstructorResult0x27a768 {
    OriginalOwnerConstructorStatus0x27a768 status;
    std::uint32_t original_pool_arm_va;
    std::uint32_t original_provider_arm_va;
};
// Reconstruct the complete field-initialization path when original
// 0x27A4F0 receives r1=0 (as actually called by 0x27A768), and the
// 0x27A768 constructor itself. This establishes a REAL native owner
// memory layout; it does NOT claim the provider or engine is running.
// Original allocator/global object and pool must already be native.
OriginalOwnerConstructorResult0x27a768
original_construct_resource_owner_0x27a768(
    const OriginalOwnerConstructorInputs0x27a768&) noexcept;
} // namespace ds_port::original_runtime
