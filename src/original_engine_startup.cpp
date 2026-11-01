#include "ds_port/original_engine_startup.hpp"
namespace ds_port::original_runtime {
OriginalEngineStartupResult original_execute_known_engine_startup(
    const OriginalEngineStartupInputs& in) noexcept {
    OriginalEngineStartupResult out{
        OriginalEngineStartupStatus::InvalidOriginalContext,
        OriginalStartupFirstStatus::InvalidOriginalObject,
        OriginalStageZeroAllocatingResult::InvalidOriginalMemory,
        OriginalStartupSecondTailStatus::InvalidOriginalContext
    };
    if(!in.frame_global_0x0049caa8 || in.frame_global_bytes<0x60)
        return out;
    auto* state=static_cast<std::uint8_t*>(in.frame_global_0x0049caa8);
    // In original 0x27EF68, the routine invoked at stage zero receives
    // pointer to exactly global+8. That same context reaches both
    // original 0x27F790 and 0x280520.
    if(in.original_second_0x280520.original_context!=state+8 ||
       in.original_second_0x280520.context_bytes > in.frame_global_bytes-8 ||
       in.original_second_0x280520.context_bytes<0x58)
        return out;
    if(state[0]!=0 || state[1]!=0 || state[2]!=0 || state[3]!=0) {
        out.status=OriginalEngineStartupStatus::OriginalStageNotZero;
        return out;
    }
    out.first_status=original_execute_startup_first_0x27f790(
        in.original_first_0x27f790);
    if(out.first_status!=OriginalStartupFirstStatus::CompletedOriginalFirstRoutine) {
        out.status=OriginalEngineStartupStatus::FirstStartupRoutineBlocked;
        return out;
    }
    // Original 0x27F870 tail-branches to 0x280520 after first completes.
    out.second_status=original_execute_stage_zero_allocating_prefix(
        in.original_second_0x280520);
    if(out.second_status!=
           OriginalStageZeroAllocatingResult::OriginalAllocatedRecordAndFollowupExecuted &&
       out.second_status!=
           OriginalStageZeroAllocatingResult::OriginalNullAllocationAndFollowupExecuted) {
        out.status=OriginalEngineStartupStatus::SecondStartupRoutineBlocked;
        return out;
    }
    // Original 0x280520 continues AFTER 0x2833F4 at 0x280580:
    // 0x27FD7C -> virtual+0x54 -> 0x2803E4 -> more allocations
    // and constructors. Do not store 1 prematurely, even though this
    // prefix has genuinely run the original mapped-memory operations.
    if(!in.original_second_tail_0x280580) {
        out.status=OriginalEngineStartupStatus::ReachedUnreconstructedOriginalTail;
        return out;
    }
    const auto& tail=*in.original_second_tail_0x280580;
    if(tail.original_context!=state+8 ||
       tail.context_bytes<0x58 ||
       tail.context_bytes>in.frame_global_bytes-8) {
        out.status=OriginalEngineStartupStatus::OriginalSecondTailBlocked;
        return out;
    }
    out.tail_status=original_execute_stage_zero_tail_0x280580(tail);
    if(out.tail_status!=OriginalStartupSecondTailStatus::CompletedOriginalSecondRoutine) {
        out.status=OriginalEngineStartupStatus::OriginalSecondTailBlocked;
        return out;
    }
    // Original NativeOnDrawFrame at 0x27EF98: stage advances to 1
    // only after the actual full 0x27F870 -> 0x27F790 -> 0x280520
    // chain returned successfully. The callbacks used for unresolved
    // virtual targets must themselves implement original game behavior.
    state[0]=1;state[1]=state[2]=state[3]=0;
    out.status=OriginalEngineStartupStatus::OriginalStageOneEntered;
    return out;
}
}
