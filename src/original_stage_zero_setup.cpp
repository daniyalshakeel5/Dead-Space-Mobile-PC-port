#include "ds_port/original_stage_zero_setup.hpp"
#include "ds_port/original_stage_zero_followup.hpp"
#include "ds_port/original_stage_zero_type_two.hpp"
#include <cstdint>

namespace ds_port::original_runtime {
static std::uint32_t original_arm_le32(const std::uint8_t* data) noexcept {
    return std::uint32_t(data[0]) |
        (std::uint32_t(data[1])<<8) |
        (std::uint32_t(data[2])<<16) |
        (std::uint32_t(data[3])<<24);
}
OriginalStageZeroSetupResult original_execute_stage_zero_setup_prefix(
    void* original_context, std::size_t accessible_bytes,
    OriginalStageZeroSetupCall setup_0x28ca24,
    OriginalGetTimeOfDay clock_0x357c4,
    OriginalStageZeroFollowupA followup_0x281d54,
    OriginalStageZeroFollowupB followup_0x2833f4) noexcept {
    // Original setup accesses context+0x40..+0x47, and last call gets
    // context+0x18. Reject invalid memory before any irreversible callback.
    if(!original_context || accessible_bytes < 0x48)
        return OriginalStageZeroSetupResult::InvalidContext;
    if(!setup_0x28ca24 || !followup_0x281d54)
        return OriginalStageZeroSetupResult::MissingOriginalCallee;
    auto* bytes=static_cast<std::uint8_t*>(original_context);
    // Original ARM 0x00280530, setup always precedes clock-slot load.
    setup_0x28ca24(bytes+0x40,2u);
    // Exact zero-word guard and 64-bit SMLAL/STRD already reconstructed.
    const auto clock=original_seed_time_using_gettimeofday(
        bytes,accessible_bytes,clock_0x357c4);
    if(clock!=OriginalGetTimeOfDaySeedResult::OriginalSlotSeeded &&
       clock!=OriginalGetTimeOfDaySeedResult::OriginalSlotAlreadyInitialized)
        return OriginalStageZeroSetupResult::OriginalClockUnavailableAfterSetup;
    // ARM LDR r0,[r4,#4] before first BL; another LDR r1,[r4,#4]
    // after first BL. Reload is essential if first call mutates the field.
    followup_0x281d54(original_arm_le32(bytes+4));
    const std::uint32_t reloaded = original_arm_le32(bytes+4);
    if (followup_0x2833f4) {
        // Only the synthetic order tests inject a target here.
        followup_0x2833f4(bytes+0x18,reloaded);
    } else {
        // Full original ARM 0x002833f4 recovered, so call the real
        // independently authored x86-64 implementation by default.
        if(!original_followup_store_0x2833f4(
             bytes+0x18,accessible_bytes-0x18,reloaded))
            return OriginalStageZeroSetupResult::InvalidContext;
    }
    // Only 96 instruction-window bytes proven; do not claim later steps run.
    return OriginalStageZeroSetupResult::VerifiedPrefixExecuted;
}
OriginalStageZeroSetupResult original_execute_stage_zero_setup_with_recovered_followup(
    void* original_context, std::size_t accessible_bytes,
    OriginalStageZeroSetupCall setup_0x28ca24,
    OriginalGetTimeOfDay clock_0x357c4,
    OriginalStageZeroFollowupA followup_0x281d54) noexcept {
    return original_execute_stage_zero_setup_prefix(
        original_context,accessible_bytes,
        setup_0x28ca24,clock_0x357c4,followup_0x281d54,nullptr);
}

OriginalStageZeroSetupResult original_execute_stage_zero_setup_with_recovered_type_two(
    void* original_context, std::size_t accessible_bytes,
    const void* original_float_field_0x0049cd1c, std::size_t source_bytes,
    OriginalGetTimeOfDay clock_0x357c4,
    OriginalStageZeroFollowupA followup_0x281d54) noexcept {
    // Full original type-2 callee writes through context+0x40+0x14;
    // the final known field is therefore context+0x54 (4 bytes).
    if(!original_context || accessible_bytes < 0x58)
        return OriginalStageZeroSetupResult::InvalidContext;
    if(!original_float_field_0x0049cd1c || source_bytes < 4 ||
       !followup_0x281d54)
        return OriginalStageZeroSetupResult::MissingOriginalCallee;
    auto* bytes=static_cast<std::uint8_t*>(original_context);
    // Stage zero ARM 0x00280520: MOV r1,#2, r0=context+0x40,
    // BL 0x28CA24, with the native type-2 branch proven in full.
    if(!original_initialize_type_two_0x28ca24(
        bytes+0x40,accessible_bytes-0x40,
        original_float_field_0x0049cd1c,source_bytes))
        return OriginalStageZeroSetupResult::InvalidContext;

    const auto clock=original_seed_time_using_gettimeofday(
        bytes,accessible_bytes,clock_0x357c4);
    if(clock!=OriginalGetTimeOfDaySeedResult::OriginalSlotSeeded &&
       clock!=OriginalGetTimeOfDaySeedResult::OriginalSlotAlreadyInitialized)
        return OriginalStageZeroSetupResult::OriginalClockUnavailableAfterSetup;

    // ARM LDR r0,[r4,#4], BL 0x281D54; we cannot invent that
    // function's indirect vtable call and allocation. Must be supplied.
    followup_0x281d54(original_arm_le32(bytes+4));
    // ARM reload LDR r1,[r4,#4] AFTER the first follow-up.
    return original_followup_store_0x2833f4(
        bytes+0x18,accessible_bytes-0x18,original_arm_le32(bytes+4))
        ? OriginalStageZeroSetupResult::VerifiedPrefixExecuted
        : OriginalStageZeroSetupResult::InvalidContext;
}

} // namespace ds_port::original_runtime
