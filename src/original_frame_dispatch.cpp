#include "ds_port/original_frame_dispatch.hpp"
#include <cstring>

namespace ds_port::original_runtime {

FramePrefixResult execute_original_frame_prefix(
    void* original_state_object, std::size_t accessible_bytes,
    OriginalFrameInit original_initializer) noexcept {

    // In the original ARM, [r4] contains the current stage. r4 is the native
    // state-object address; the initializer receives r4+8, not r4.
    // memcpy avoids assuming 32-bit ARM alignment on a desktop platform.
    if (!original_state_object || accessible_bytes < 9)
        return FramePrefixResult::InvalidContext;

    auto* bytes = static_cast<std::uint8_t*>(original_state_object);
    std::uint32_t stage = 0;
    std::memcpy(&stage, bytes, sizeof(stage));

    // ARM cmp r3,#0 / beq 0x0027ef98.
    if (stage == 0) {
        if (!original_initializer)
            return FramePrefixResult::MissingOriginalInitRoutine;

        // Original order is essential: call at 0x0027f870 FIRST, then store 1.
        original_initializer(bytes + 8);
        stage = 1;
        std::memcpy(bytes, &stage, sizeof(stage));
        return FramePrefixResult::OriginalInitCompleted;
    }

    // ARM cmp r3,#1 / beq 0x0027efac. The original active-frame callback
    // 0x0027fc68 is not yet reverse-engineered, so do NOT execute a stand-in.
    if (stage == 1)
        return FramePrefixResult::OriginalActiveFrameNotYetReconstructed;

    // Any other value reaches ARM stack restoration and immediate return.
    return FramePrefixResult::OriginalOtherStateReturned;
}

StageZeroChainResult execute_original_stage_zero_chain(
    void* original_stage_context, OriginalFrameInit first_original_callee,
    OriginalFrameInit second_original_callee) noexcept {
    if (!original_stage_context || !first_original_callee || !second_original_callee)
        return StageZeroChainResult::MissingNativeCallee;

    // 0x0027f870: MOV r4,r0; BL 0x0027f790; MOV r0,r4;
    // POP {r4,lr}; B 0x00280520. The same original context reaches
    // both callees, in this order. Do not assume their implementations.
    first_original_callee(original_stage_context);
    second_original_callee(original_stage_context);
    return StageZeroChainResult::OriginalTwoCallsCompleted;
}

std::uint32_t original_stage_one_normalized_result(
    std::uint32_t virtual_method_return) noexcept {
    // 0x0027fc80: SUBS r0,r0,#1; MOVNE r0,#1.
    // Exact unsigned 32-bit result: 0 only for argument 1, else 1.
    return virtual_method_return == 1u ? 0u : 1u;
}

} // namespace ds_port::original_runtime
