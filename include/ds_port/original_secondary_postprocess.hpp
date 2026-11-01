#pragma once
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
// Verified from original ELF 0x288688, instruction window +0..+80.
// Callback inputs are ARM32 address tokens, never host pointers.
inline constexpr std::uint32_t kSecondaryAllocatorOriginalDescriptorVA=0x0042ffb0u;
using OriginalPostprocessStep=bool (*)(std::uint32_t original_target_va,
    std::uint32_t original_argument_arm_address,
    std::uint32_t original_arg1_arm_address,
    std::uint32_t original_arg2_arm_address,void* context,
    std::uint32_t* original_result) noexcept;
struct OriginalPostprocessInputs {
    std::uint32_t original_object_arm_address;
    void* mapped_object_bytes;
    std::size_t accessible_object_bytes;
    // Resolved PC-relative literal values. Their real ELF VAs are not
    // guessed by this function.
    std::uint32_t initial_vtable_arm_address;
    std::uint32_t first_setup_arg_arm_address;
    std::uint32_t second_setup_arg_arm_address;
    std::uint32_t third_setup_arg_arm_address;
    OriginalPostprocessStep call_original;
    void* callback_context;
};
enum class OriginalPostprocessStatus:std::uint8_t {
    InvalidMemory,MissingOriginalCallees,CallRefused,Completed
};
// Contiguous ordering: 0x2828ac(object), store vtable at object,
// 0x27fb28(object+0x14), 0x27fa74(object+0x14),
// store 0x27fa74 return at object+0x20.
// The subroutines themselves must be proven and supplied by caller.
// Both vtables verified against original ARM ELF on hosted runner.
inline constexpr std::uint32_t kOriginal2828acConstructorVtableVA=0x004898e8u;
inline constexpr std::uint32_t kOriginal288688OverrideVtableVA=0x00489a98u;

// Runs original 0x2828AC directly before remaining two external methods.
OriginalPostprocessStatus original_postprocess_0x288688_native_constructor(
    const OriginalPostprocessInputs&) noexcept;
OriginalPostprocessStatus original_postprocess_0x288688_prefix(
    const OriginalPostprocessInputs&) noexcept;
}
