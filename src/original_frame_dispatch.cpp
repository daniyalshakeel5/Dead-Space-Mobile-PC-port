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

OriginalVirtualDispatchResult original_invoke_stage_one_first_virtual(
    void* original_virtual_object,
    OriginalVirtualFirstMethod original_first_method) noexcept {
    if (!original_virtual_object || !original_first_method)
        return OriginalVirtualDispatchResult::MissingOriginalObjectOrMethod;

    // Original ELF 0x0027fc68:
    // r0 = *[context+0xc]; r3 = [r0]; BLX first virtual target;
    // SUBS r0,r0,#1; MOVNE r0,#1.
    // These are the verified call/return semantics; the 32-bit
    // virtual pointer layout is deliberately provided by the caller.
    const std::uint32_t returned = original_first_method(original_virtual_object);
    return original_stage_one_normalized_result(returned) == 0
        ? OriginalVirtualDispatchResult::OriginalReturnWasOne
        : OriginalVirtualDispatchResult::OriginalReturnWasOther;
}

OriginalStageOneBranchResult original_dispatch_stage_one_result(
    std::uint32_t result_from_0x0027fc68,
    OriginalFrameTransition original_transition_0x0027eecc) noexcept {
    // At original frame branch ELF 0x0027efb4:
    // CMP r0,#0 ; BNE 0x0027efc8 ;
    // for zero: stack restoration and direct B 0x0027eecc.
    if (result_from_0x0027fc68 != 0)
        return OriginalStageOneBranchResult::OriginalNonzeroBranchNotYetReconstructed;
    if (!original_transition_0x0027eecc)
        return OriginalStageOneBranchResult::MissingOriginalTransition;
    original_transition_0x0027eecc();
    return OriginalStageOneBranchResult::OriginalZeroResultTransitionCalled;
}

OriginalStateTwoPrefixResult original_write_state_two_transition_prefix(
    void* original_global_state, std::size_t accessible_bytes,
    void** original_context_after_postincrement) noexcept {
    if (!original_global_state || accessible_bytes < 8 ||
        !original_context_after_postincrement)
        return OriginalStateTwoPrefixResult::InvalidOriginalStateObject;

    // Original ELF routine 0x0027eecc:
    // MOV r3,#2 ; ... ; STR r3,[r4],#8.
    // The stored word is little-endian (original ARM ELF32).
    auto* bytes=static_cast<std::uint8_t*>(original_global_state);
    bytes[0]=2; bytes[1]=0; bytes[2]=0; bytes[3]=0;
    // ARM post-indexed STR advances r4 by 8. Later native routines
    // use that address; they are deliberately left unresolved.
    *original_context_after_postincrement=bytes+8;
    return OriginalStateTwoPrefixResult::OriginalStateTwoStored;
}

OriginalActiveFramePrefixResult original_execute_active_frame_prefix(
    void* original_shared_state, std::size_t accessible_bytes,
    void* original_virtual_object,
    OriginalVirtualFirstMethod original_virtual_method,
    void** context_after_state_two_postincrement) noexcept {
    if (!original_shared_state || accessible_bytes < 8 ||
        !context_after_state_two_postincrement)
        return OriginalActiveFramePrefixResult::InvalidOriginalContext;

    const auto* state=static_cast<const std::uint8_t*>(original_shared_state);
    // ARM at ELF 0x0027ef84: compare first 32-bit word of shared global
    // to stage 1, branch into 0x0027efac only if equal.
    if (state[0]!=1 || state[1]!=0 || state[2]!=0 || state[3]!=0)
        return OriginalActiveFramePrefixResult::OriginalStateWasNotOne;

    if (!original_virtual_object || !original_virtual_method)
        return OriginalActiveFramePrefixResult::MissingOriginalVirtualTarget;

    // Original 0x0027fc68 calls [object->vtable+0] and applies
    // SUBS r0,r0,#1 / MOVNE r0,#1. The original object and function
    // MUST be supplied by a separate verified runtime implementation.
    const auto result=original_invoke_stage_one_first_virtual(
        original_virtual_object, original_virtual_method);
    if (result != OriginalVirtualDispatchResult::OriginalReturnWasOne)
        return OriginalActiveFramePrefixResult::OriginalNonzeroBranchUnimplemented;

    // The dispatcher sees normalized r0=0 and tail-branches to
    // 0x0027eecc. Independently verified PC-relative literals show
    // both routines refer to THE SAME original global at 0x0049caa8.
    // 0x0027eecc immediately stores state=2 and advances r4 by 8.
    const auto written=original_write_state_two_transition_prefix(
        original_shared_state,accessible_bytes,
        context_after_state_two_postincrement);
    return written == OriginalStateTwoPrefixResult::OriginalStateTwoStored
        ? OriginalActiveFramePrefixResult::OriginalStateTwoTransitionPrefixExecuted
        : OriginalActiveFramePrefixResult::InvalidOriginalContext;
}

} // namespace ds_port::original_runtime
