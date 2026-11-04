#pragma once
#include "ds_port/original_startup_first.hpp"
#include "ds_port/original_stage_zero_allocating.hpp"
#include "ds_port/original_startup_second_tail.hpp"
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
// Actual ARM NativeOnDrawFrame stage-zero sequence:
// [global 0x0049CAA8] stage=0 -> original 0x27F870(global+8)
// -> 0x27F790(global+8) -> 0x280520(global+8) -> stage=1.
// Both known native routines run on mapped original game memory.
// The original 0x280520 AFTER 0x2833F4 has more unported calls.
// We MUST NOT store stage=1 until that remaining code is implemented.
struct OriginalEngineStartupInputs {
    void* frame_global_0x0049caa8;
    std::size_t frame_global_bytes;
    OriginalStartupFirstInputs original_first_0x27f790;
    OriginalStageZeroAllocatingInputs original_second_0x280520;
    const OriginalStartupSecondTailInputs* original_second_tail_0x280580=nullptr;
};
enum class OriginalEngineStartupStatus:std::uint8_t {
    InvalidOriginalContext,
    OriginalStageNotZero,
    FirstStartupRoutineBlocked,
    SecondStartupRoutineBlocked,
    ReachedUnreconstructedOriginalTail,
    OriginalSecondTailBlocked,
    OriginalStageOneEntered
};
struct OriginalEngineStartupResult {
    OriginalEngineStartupStatus status;
    OriginalStartupFirstStatus first_status;
    OriginalStageZeroAllocatingResult second_status;
    OriginalStartupSecondTailStatus tail_status;
};
// Runs both verified native call chains in strict original sequence,
// without advancing frame state or pretending engine startup completed.
OriginalEngineStartupResult original_execute_known_engine_startup(
    const OriginalEngineStartupInputs&) noexcept;
}
