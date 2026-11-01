#pragma once
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {

// Proven original ARM NativeOnDrawFrame branch at ELF 0x0027ef68.
// Only stage 0 and the early stage selectors are reconstructed. An active
// game frame (stage 1, original call 0x0027fc68) is NOT yet implemented.
enum class FramePrefixResult : std::uint8_t {
    InvalidContext,
    MissingOriginalInitRoutine,
    OriginalInitCompleted,
    OriginalActiveFrameNotYetReconstructed,
    OriginalOtherStateReturned
};

// Original ARM stage 0 passes the address of the state object + 8 bytes in r0
// to internal native routine 0x0027f870, then stores 1 to its first word.
// Caller must supply a reconstructed implementation of that routine; no
// placeholder game simulation is introduced here.
using OriginalFrameInit = void (*)(void* state_plus_eight) noexcept;

FramePrefixResult execute_original_frame_prefix(
    void* original_state_object, std::size_t accessible_bytes,
    OriginalFrameInit original_initializer) noexcept;

// Original ARM 0x0027f870: call routine 0x0027f790(context), then
// tail-branch to 0x00280520(context). These callees are not reconstructed
// yet; refusing a missing callee prevents a fabricated success path.
enum class StageZeroChainResult : std::uint8_t {
    MissingNativeCallee,
    OriginalTwoCallsCompleted
};
StageZeroChainResult execute_original_stage_zero_chain(
    void* original_stage_context, OriginalFrameInit first_original_callee,
    OriginalFrameInit second_original_callee) noexcept;

// Original ARM 0x0027fc68 after its virtual call: SUBS r0,r0,#1;
// MOVNE r0,#1. Thus return 0 iff the virtual method returned 1, else 1.
// Its virtual method itself and subsequent graphics/state side effects are
// NOT implemented by this pure value-normalization function.
std::uint32_t original_stage_one_normalized_result(
    std::uint32_t virtual_method_return) noexcept;

} // namespace ds_port::original_runtime
