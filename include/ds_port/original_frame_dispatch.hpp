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

// Original ARM 0x0027fc68 reads a pointer from [context+0xc], reads
// its first virtual-method pointer, calls that method with its owning object
// as r0, then applies SUBS/MOVNE to normalize the result.
// The original 32-bit pointer layout cannot safely be assumed on x86-64:
// the verified virtual target and object address must be supplied explicitly.
// There is NO stand-in virtual method and no engine state mutation.
using OriginalVirtualFirstMethod = std::uint32_t (*)(void*) noexcept;
enum class OriginalVirtualDispatchResult : std::uint8_t {
    MissingOriginalObjectOrMethod,
    OriginalReturnWasOne,
    OriginalReturnWasOther
};
OriginalVirtualDispatchResult original_invoke_stage_one_first_virtual(
    void* original_virtual_object,
    OriginalVirtualFirstMethod original_first_method) noexcept;

// Original state-one frame ARM branch: after native 0x0027fc68 returns,
// zero leads to the original transition routine 0x0027eecc. Nonzero
// proceeds to other indirect dispatches that are not yet reconstructed.
// We never report those unknown side effects as executed.
using OriginalFrameTransition = void (*)() noexcept;
enum class OriginalStageOneBranchResult : std::uint8_t {
    MissingOriginalTransition,
    OriginalZeroResultTransitionCalled,
    OriginalNonzeroBranchNotYetReconstructed
};
OriginalStageOneBranchResult original_dispatch_stage_one_result(
    std::uint32_t result_from_0x0027fc68,
    OriginalFrameTransition original_transition_0x0027eecc) noexcept;

// The ORIGINAL zero-result transition routine 0x0027eecc begins by
// obtaining a global state object, storing the integer 2 to [object],
// post-incrementing the pointer by 8 bytes, and passing that context
// into further native calls. Those subsequent calls are NOT ported.
// This executes only its proven prefix. Never describe it as a complete
// game shutdown/transition or a playable frame.
enum class OriginalStateTwoPrefixResult : std::uint8_t {
    InvalidOriginalStateObject,
    OriginalStateTwoStored
};
OriginalStateTwoPrefixResult original_write_state_two_transition_prefix(
    void* original_global_state, std::size_t accessible_bytes,
    void** original_context_after_postincrement) noexcept;

// Verified contiguous ARM state-1 prefix on the SHARED original global
// (original ELF VA 0x0049caa8, confirmed by independent PC-relative
// literal-pool resolution for BOTH 0x0027ef68 and 0x0027eecc).
//
// Stage==1 -> 0x0027fc68 first virtual target -> normalize result.
// A return of 1 from that virtual target normalizes to 0, so the
// dispatcher branches to 0x0027eecc and stores state=2. Only this
// prefix is reconstructed. Original transition's subsequent calls
// and active nonzero branch remain UNIMPLEMENTED and never faked.
// Caller supplies the original object's host-native pointer and method;
// ARM32 vtable pointers cannot be directly reinterpreted on x86-64.
enum class OriginalActiveFramePrefixResult : std::uint8_t {
    InvalidOriginalContext,
    OriginalStateWasNotOne,
    MissingOriginalVirtualTarget,
    OriginalNonzeroBranchUnimplemented,
    OriginalStateTwoTransitionPrefixExecuted,
    OriginalNonzeroTwoVirtualCallsExecuted,
    OriginalNonzeroFirstCallOnly
};
OriginalActiveFramePrefixResult original_execute_active_frame_prefix(
    void* original_shared_state, std::size_t accessible_bytes,
    void* original_virtual_object,
    OriginalVirtualFirstMethod original_virtual_method,
    void** context_after_state_two_postincrement) noexcept;

// Original active frame's NONZERO branch, ELF 0x0027efc8..0x0027f000:
//   backend = *(shared_global + 0x68);
//   backend->vtable[0x20/4](backend,14,0,0,stack_zero);
//   backend = *(shared_global + 0x68);     // critically RELOADED
//   backend->vtable[0x38/4](backend);
//   B 0x0027ef90 (frame return).
// Every instruction and call ordering is matched to the real APK.
// Backend pointer and both virtual target implementations remain
// dependency-injected until the original object layout is reconstructed.
// The resolver must re-read the host equivalent of [shared_global+0x68]
// EVERY time; the original can change the backend in the first call.
using OriginalFrameBackendResolver = void* (*)(void* shared_global) noexcept;
using OriginalFrameVtableMethod0x20 = void (*)(
    void* backend, std::uint32_t argument_14,
    std::uint32_t zero_a, std::uint32_t zero_b,
    std::uint32_t stack_zero) noexcept;
using OriginalFrameVtableMethod0x38 = void (*)(void* backend) noexcept;
enum class OriginalNonzeroVirtualSequenceResult : std::uint8_t {
    MissingOriginalTargets,
    FirstBackendUnavailable,
    SecondBackendUnavailableAfterFirstCall,
    TwoOriginalVirtualCallsExecuted
};
OriginalNonzeroVirtualSequenceResult original_execute_nonzero_virtual_sequence(
    void* original_shared_global,
    OriginalFrameBackendResolver original_backend_resolver,
    OriginalFrameVtableMethod0x20 original_slot_0x20_method,
    OriginalFrameVtableMethod0x38 original_slot_0x38_method) noexcept;

// Same verified stage-one dispatch as original_execute_active_frame_prefix,
// with the additional two ARM-verified nonzero-path virtual calls when
// their original targets have been supplied. Neither method is faked,
// and the original transition's remaining actions are not implemented.
OriginalActiveFramePrefixResult original_execute_active_frame_verified_branches(
    void* original_shared_global, std::size_t accessible_bytes,
    void* original_active_virtual_object,
    OriginalVirtualFirstMethod original_active_virtual_method,
    void** context_after_state_two_postincrement,
    OriginalFrameBackendResolver original_backend_resolver,
    OriginalFrameVtableMethod0x20 original_slot_0x20_method,
    OriginalFrameVtableMethod0x38 original_slot_0x38_method) noexcept;

} // namespace ds_port::original_runtime
