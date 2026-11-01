#include "ds_port/original_stage_zero_setup.hpp"
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
    if(!setup_0x28ca24 || !followup_0x281d54 || !followup_0x2833f4)
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
    followup_0x2833f4(bytes+0x18,original_arm_le32(bytes+4));
    // Only 96 instruction-window bytes proven; do not claim later steps run.
    return OriginalStageZeroSetupResult::VerifiedPrefixExecuted;
}
} // namespace ds_port::original_runtime
