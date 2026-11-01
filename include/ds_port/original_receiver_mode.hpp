#pragma once
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {
// Original ARM 0x26C410:
// r0=receiver, r1=opcode, r2=value.
// opcode 1 normalizes value to 0/1 and writes byte owner+0x8B5;
// opcode 2 stores raw value to word owner+0x8B8.
// Other opcodes immediately return without touching receiver state.
// A transition to disabled mode invokes owner's vtable+0x28, whose
// concrete target is 0x273A0C. No ARM execution is required.
using OriginalReceiverDisableReset=bool (*)(
    std::uint32_t original_owner_arm_va,void* context) noexcept;
struct OriginalReceiverModeInputs0x26c410 {
    std::uint32_t owner_arm_va=0;
    void* mapped_owner=nullptr;
    std::size_t owner_bytes=0;
    OriginalReceiverDisableReset native_reset_0x273a0c=nullptr;
    void* context=nullptr;
};
enum class OriginalReceiverModeStatus0x26c410:std::uint8_t {
    Completed,InvalidOriginalOwner,
    MissingOriginalDisableReset,OriginalDisableResetRefused
};
struct OriginalReceiverModeResult0x26c410 {
    OriginalReceiverModeStatus0x26c410 status;
    std::uint32_t previous_value;
    std::uint32_t current_value;
};
OriginalReceiverModeResult0x26c410 original_set_receiver_mode_0x26c410(
    const OriginalReceiverModeInputs0x26c410&,
    std::uint32_t original_opcode,std::uint32_t original_value) noexcept;
} // namespace ds_port::original_runtime
