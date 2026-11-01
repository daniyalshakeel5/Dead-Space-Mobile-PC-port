#pragma once
#include "ds_port/original_clock_source.hpp"
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {

// ELF 0x00280520 instruction-verified prefix; not complete game initialization.
// ARM calls 0x28ca24(context+0x40,2), conditionally seeds time at +0x40,
// calls 0x281d54(*(uint32_t*)(context+4)), reloads that SAME field,
// then calls 0x2833f4(context+0x18,*(uint32_t*)(context+4)).
// Numeric ARM pointer tokens cannot be host-dereferenced on x86-64.
using OriginalStageZeroSetupCall = void (*)(void*,std::uint32_t) noexcept;
using OriginalStageZeroFollowupA = void (*)(std::uint32_t) noexcept;
using OriginalStageZeroFollowupB = void (*)(void*,std::uint32_t) noexcept;
enum class OriginalStageZeroSetupResult : std::uint8_t {
    InvalidContext, MissingOriginalCallee,
    OriginalClockUnavailableAfterSetup,
    VerifiedPrefixExecuted
};
OriginalStageZeroSetupResult original_execute_stage_zero_setup_prefix(
    void* original_context, std::size_t accessible_bytes,
    OriginalStageZeroSetupCall setup_0x28ca24,
    OriginalGetTimeOfDay clock_0x357c4,
    OriginalStageZeroFollowupA followup_0x281d54,
    OriginalStageZeroFollowupB followup_0x2833f4) noexcept;

// Pass nullptr as followup_0x2833f4 to invoke the fully reconstructed
// original native function, using native memory at context+0x18 and
// the SECOND load from context+4. The earlier setup and first followup
// still require separately recovered, explicitly provided implementations.
// A non-null callback is retained ONLY for instruction-order verification.
OriginalStageZeroSetupResult original_execute_stage_zero_setup_with_recovered_followup(
    void* original_context, std::size_t accessible_bytes,
    OriginalStageZeroSetupCall setup_0x28ca24,
    OriginalGetTimeOfDay clock_0x357c4,
    OriginalStageZeroFollowupA followup_0x281d54) noexcept;
} // namespace ds_port::original_runtime
