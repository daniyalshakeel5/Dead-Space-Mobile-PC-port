#pragma once
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {

// Original ELF instruction evidence: vtable[3] at owner->vtable +0x0C.
// ELF 0x281D54 r0=owner, r1=0x20, r2=0x0042FC70, r3=1, stack=[4,0].
// 0x0042FC70 is the ORIGINAL runtime type-descriptor ARM virtual address,
// independently recovered from the original executable's literal pool.
// Never reinterpret original ARM pointers as host pointers on x86-64.
inline constexpr std::uint32_t kOriginalAllocatorDescriptorVA=0x0042fc70u;
inline constexpr std::uint32_t kOriginalAllocatorRequestSize=0x20u;
inline constexpr std::uint32_t kOriginalAllocatorFlag=1u;
inline constexpr std::uint32_t kOriginalAllocatorStackAlignment=4u;
inline constexpr std::uint32_t kOriginalAllocatorStackZero=0u;

struct OriginalAllocatorCall {
    std::uint32_t original_method_arm_address;
    std::uint32_t original_owner_arm_address;
    std::uint32_t requested_size;
    std::uint32_t original_type_descriptor_arm_address;
    std::uint32_t flag;
    std::uint32_t stack_arg0;
    std::uint32_t stack_arg1;
};

struct OriginalAllocatorReturn {
    std::uint32_t original_allocated_record_arm_address;
    void* record_host_bytes;
    std::size_t accessible_record_bytes;
};

// This callback represents the ORIGINAL vtable method until it is actually
// reconstructed. A native substitute allocation is NOT implied or supplied.
// Caller's implementation must resolve original ARM method address correctly.
using OriginalAllocatorDispatch = bool (*)(
    const OriginalAllocatorCall& original_arguments,
    void* caller_context,
    OriginalAllocatorReturn* original_result) noexcept;

enum class OriginalAllocatorCallStatus : std::uint8_t {
    InvalidOriginalMemory,
    InvalidOriginalVtable,
    MissingOriginalAllocator,
    OriginalAllocatorRefused,
    OriginalAllocatorReturnedNull,
    OriginalRecordInitializedAndPublished
};

// Reconstructs the original 0x281D54 call ABI and ALL its verified
// post-return writes (implemented in original_record_postalloc.cpp).
// Owner host bytes contain original 32-bit vtable pointer at +0;
// provided vtable host bytes must match that address, including +0x0C.
// Caller provides the host allocation contents returned by original code.
// Original global slot is the user-provided host mapping of 0x0049CB94.
// The function does NOT guess allocator implementation or ownership.
OriginalAllocatorCallStatus original_execute_allocator_call_0x281d54(
    std::uint32_t original_owner_arm_address,
    const void* original_owner_host_bytes,
    std::size_t original_owner_accessible_bytes,
    std::uint32_t original_vtable_arm_address,
    const void* original_vtable_host_bytes,
    std::size_t original_vtable_accessible_bytes,
    void* original_record_global_slot_0x0049cb94,
    std::size_t original_global_slot_accessible_bytes,
    OriginalAllocatorDispatch invoke_original_allocator,
    void* caller_context) noexcept;

} // namespace ds_port::original_runtime
