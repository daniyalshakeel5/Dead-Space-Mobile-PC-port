#pragma once
#include "ds_port/original_allocator_call.hpp"
#include "ds_port/original_clock_source.hpp"
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {

// Bounded, portable reconstruction of the verified original ARM 0x280520
// execution prefix: type-2 setup -> conditional gettimeofday clock seed ->
// vtable allocator call and record global publication -> final 0x2833F4
// three-store followup. These are ORIGINAL code behaviors, not a fabricated
// desktop gameplay engine or substitute allocator implementation.
struct OriginalStageZeroAllocatingInputs {
    void* original_context;
    std::size_t context_bytes;
    const void* original_type_two_float_source;
    std::size_t type_two_float_bytes;
    OriginalGetTimeOfDay original_gettimeofday;
    std::uint32_t original_owner_arm_address;
    const void* original_owner_host_bytes;
    std::size_t original_owner_host_bytes_length;
    std::uint32_t original_owner_vtable_arm_address;
    const void* original_owner_vtable_host_bytes;
    std::size_t original_owner_vtable_host_bytes_length;
    void* original_allocated_record_global_slot;
    std::size_t original_allocated_record_global_slot_bytes;
    OriginalAllocatorDispatch invoke_original_allocator;
    void* original_allocator_context;
};

enum class OriginalStageZeroAllocatingResult : std::uint8_t {
    InvalidOriginalMemory,
    MissingOriginalAllocator,
    OriginalClockSourceUnavailable,
    OriginalAllocatorUnavailableAfterClock,
    OriginalNullAllocationAndFollowupExecuted,
    OriginalAllocatedRecordAndFollowupExecuted,
};

// The owner is read from ORIGINAL context+4 after clock setup. A supplied
// owner token must match that 32-bit field; no host pointer conversion.
// The original allocator callback may update context+4. This function
// ALWAYS reloads context+4 before its final original ARM 0x2833F4 store.
// If the virtual allocator cannot be invoked, the final store is NOT
// fabricated, and no successful initialization status is returned.
OriginalStageZeroAllocatingResult original_execute_stage_zero_allocating_prefix(
    const OriginalStageZeroAllocatingInputs& original) noexcept;

} // namespace ds_port::original_runtime
