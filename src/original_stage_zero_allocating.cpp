#include "ds_port/original_stage_zero_allocating.hpp"
#include "ds_port/original_stage_zero_type_two.hpp"
#include "ds_port/original_stage_zero_followup.hpp"
#include "ds_port/original_clock_source.hpp"
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {
namespace {
std::uint32_t arm_read32(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0]) |
        (std::uint32_t(p[1])<<8) |
        (std::uint32_t(p[2])<<16) |
        (std::uint32_t(p[3])<<24);
}
}
OriginalStageZeroAllocatingResult original_execute_stage_zero_allocating_prefix(
    const OriginalStageZeroAllocatingInputs& original) noexcept {
    if(!original.original_context || original.context_bytes<0x58 ||
       !original.original_type_two_float_source || original.type_two_float_bytes<4 ||
       !original.original_owner_host_bytes ||
       original.original_owner_host_bytes_length<4 ||
       !original.original_owner_vtable_host_bytes ||
       original.original_owner_vtable_host_bytes_length<16 ||
       !original.original_allocated_record_global_slot ||
       original.original_allocated_record_global_slot_bytes<4 ||
       original.original_owner_arm_address==0 ||
       original.original_owner_vtable_arm_address==0)
        return OriginalStageZeroAllocatingResult::InvalidOriginalMemory;
    if(!original.invoke_original_allocator)
        return OriginalStageZeroAllocatingResult::MissingOriginalAllocator;

    const auto* owner=static_cast<const std::uint8_t*>(original.original_owner_host_bytes);
    const auto* vtable=static_cast<const std::uint8_t*>(
        original.original_owner_vtable_host_bytes);
    if(arm_read32(owner)!=original.original_owner_vtable_arm_address ||
       arm_read32(vtable+12)==0)
        return OriginalStageZeroAllocatingResult::InvalidOriginalMemory;

    auto* context=static_cast<std::uint8_t*>(original.original_context);
    // Original ARM 0x280520:
    // MOV r1,#2; ADD r0,r0,#0x40; BL 0x28CA24.
    if(!original_initialize_type_two_0x28ca24(
        context+0x40,original.context_bytes-0x40,
        original.original_type_two_float_source,original.type_two_float_bytes))
        return OriginalStageZeroAllocatingResult::InvalidOriginalMemory;
    // Original conditional zero guard and SMLAL/STRD clock seed.
    const auto time_result=original_seed_time_using_gettimeofday(
        context,original.context_bytes,original.original_gettimeofday);
    if(time_result!=OriginalGetTimeOfDaySeedResult::OriginalSlotSeeded &&
       time_result!=OriginalGetTimeOfDaySeedResult::OriginalSlotAlreadyInitialized)
        return OriginalStageZeroAllocatingResult::OriginalClockSourceUnavailable;

    // Original LDR r0,[r4,#4], BL 0x281D54. The original ARM owner
    // token is compared to the caller's actual mapped ARM object before
    // dispatching through its vtable at offset +0x0C.
    if(arm_read32(context+4)!=original.original_owner_arm_address)
        return OriginalStageZeroAllocatingResult::InvalidOriginalMemory;
    const auto allocation=original_execute_allocator_call_0x281d54(
        original.original_owner_arm_address,
        original.original_owner_host_bytes,
        original.original_owner_host_bytes_length,
        original.original_owner_vtable_arm_address,
        original.original_owner_vtable_host_bytes,
        original.original_owner_vtable_host_bytes_length,
        original.original_allocated_record_global_slot,
        original.original_allocated_record_global_slot_bytes,
        original.invoke_original_allocator,
        original.original_allocator_context);
    if(allocation!=OriginalAllocatorCallStatus::OriginalAllocatorReturnedNull &&
       allocation!=OriginalAllocatorCallStatus::OriginalRecordInitializedAndPublished)
        return OriginalStageZeroAllocatingResult::OriginalAllocatorUnavailableAfterClock;

    // Original LDR r1,[r4,#4] AFTER the virtual call; BL 0x2833F4
    // with r0=context+0x18 and the freshly reloaded original ARM word.
    if(!original_followup_store_0x2833f4(
        context+0x18,original.context_bytes-0x18,arm_read32(context+4)))
        return OriginalStageZeroAllocatingResult::InvalidOriginalMemory;
    return allocation==OriginalAllocatorCallStatus::OriginalAllocatorReturnedNull
        ? OriginalStageZeroAllocatingResult::OriginalNullAllocationAndFollowupExecuted
        : OriginalStageZeroAllocatingResult::OriginalAllocatedRecordAndFollowupExecuted;
}
} // namespace ds_port::original_runtime
