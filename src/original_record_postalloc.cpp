#include "ds_port/original_record_postalloc.hpp"
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {
namespace {
void arm_store32(std::uint8_t* memory,std::size_t offset,std::uint32_t value) noexcept {
    memory[offset] = static_cast<std::uint8_t>(value);
    memory[offset+1] = static_cast<std::uint8_t>(value>>8);
    memory[offset+2] = static_cast<std::uint8_t>(value>>16);
    memory[offset+3] = static_cast<std::uint8_t>(value>>24);
}
}
OriginalPostAllocateResult original_complete_allocator_postreturn_0x281d54(
    void* original_record_host_memory,
    std::size_t original_record_accessible_bytes,
    std::uint32_t original_owner_address_r5,
    std::uint32_t original_returned_record_address_r0,
    void* original_global_slot_host_memory,
    std::size_t original_global_slot_accessible_bytes) noexcept {
    if(!original_global_slot_host_memory || original_global_slot_accessible_bytes < 4)
        return OriginalPostAllocateResult::InvalidNativeAddressOrBounds;
    auto* original_global=static_cast<std::uint8_t*>(original_global_slot_host_memory);

    // Original CMP r0,#0 / BEQ 0x281DB0. On null the branch skips all
    // allocated record writes but still executes STR r0,[r3] at +0x64.
    if(original_returned_record_address_r0==0) {
        arm_store32(original_global,0,0);
        return OriginalPostAllocateResult::OriginalNullResultPublished;
    }
    if(!original_record_host_memory || original_record_accessible_bytes < 0x20)
        return OriginalPostAllocateResult::InvalidNativeAddressOrBounds;

    auto* record=static_cast<std::uint8_t*>(original_record_host_memory);
    const std::uint32_t embedded_slot=original_returned_record_address_r0+4u;

    // Exact original ARM store order after virtual allocation:
    // ADD r3,r0,#4; STR r5,[r0,#0x18]; STR 0,[r0,#0x1C];
    // STR r3,[r0,#8]; STR 0,[r0,#0x0C]; STRB 0,[r0,#0x10];
    // STR 0,[r0,#0x14]; STR r3,[r0,#4]; STR r0,[global].
    arm_store32(record,0x18,original_owner_address_r5);
    arm_store32(record,0x1c,0);
    arm_store32(record,0x08,embedded_slot);
    arm_store32(record,0x0c,0);
    record[0x10]=0; // STRB only: do not corrupt bytes 0x11..0x13.
    arm_store32(record,0x14,0);
    arm_store32(record,0x04,embedded_slot);
    arm_store32(original_global,0,original_returned_record_address_r0);
    return OriginalPostAllocateResult::OriginalRecordInitializedAndPublished;
}
} // namespace ds_port::original_runtime
