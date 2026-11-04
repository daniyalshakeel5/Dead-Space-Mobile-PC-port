#include "ds_port/original_allocator_call.hpp"
#include "ds_port/original_record_postalloc.hpp"
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {
namespace {
std::uint32_t arm_le32(const void* value) noexcept {
    const auto* p=static_cast<const std::uint8_t*>(value);
    return std::uint32_t(p[0]) |
        (std::uint32_t(p[1])<<8) |
        (std::uint32_t(p[2])<<16) |
        (std::uint32_t(p[3])<<24);
}
}

OriginalAllocatorCallStatus original_execute_allocator_call_0x281d54(
    std::uint32_t original_owner_arm_address,
    const void* original_owner_host_bytes,
    std::size_t original_owner_accessible_bytes,
    std::uint32_t original_vtable_arm_address,
    const void* original_vtable_host_bytes,
    std::size_t original_vtable_accessible_bytes,
    void* original_record_global_slot_0x0049cb94,
    std::size_t original_global_slot_accessible_bytes,
    OriginalAllocatorDispatch invoke_original_allocator,
    void* caller_context) noexcept {
    // The original ARM code uses 32-bit address tokens. Host memory is
    // explicitly mapped; no ARM virtual address is directly dereferenced.
    // All rejectable inputs are checked before invoking the allocator.
    if(original_owner_arm_address==0 ||
       original_owner_accessible_bytes<4 ||
       !original_owner_host_bytes ||
       !original_record_global_slot_0x0049cb94 ||
       original_global_slot_accessible_bytes<4)
        return OriginalAllocatorCallStatus::InvalidOriginalMemory;
    if(original_vtable_arm_address==0 ||
       original_vtable_accessible_bytes<16 ||
       !original_vtable_host_bytes ||
       arm_le32(original_owner_host_bytes)!=original_vtable_arm_address)
        return OriginalAllocatorCallStatus::InvalidOriginalVtable;
    const auto* table=static_cast<const std::uint8_t*>(original_vtable_host_bytes);
    // ARM at 0x281D70 LDR ip,[r0], then 0x281D84 LDR pc,[ip,#0x0C].
    const std::uint32_t method=arm_le32(table+12);
    if(method==0)
        return OriginalAllocatorCallStatus::InvalidOriginalVtable;
    if(!invoke_original_allocator)
        return OriginalAllocatorCallStatus::MissingOriginalAllocator;

    // Exact original argument order and values. r2 points to the descriptor
    // recovered from the original PC-relative ARM literal at 0x281DC4.
    const OriginalAllocatorCall call{
        method,original_owner_arm_address,
        kOriginalAllocatorRequestSize,kOriginalAllocatorDescriptorVA,
        kOriginalAllocatorFlag,kOriginalAllocatorStackAlignment,
        kOriginalAllocatorStackZero
    };
    OriginalAllocatorReturn returned{0,nullptr,0};
    if(!invoke_original_allocator(call,caller_context,&returned))
        return OriginalAllocatorCallStatus::OriginalAllocatorRefused;

    // The original code CMP r0,#0 and branches directly to publishing zero
    // on null; a host allocation pointer must not be inferred in that case.
    const auto outcome=original_complete_allocator_postreturn_0x281d54(
        returned.record_host_bytes,returned.accessible_record_bytes,
        original_owner_arm_address,returned.original_allocated_record_arm_address,
        original_record_global_slot_0x0049cb94,original_global_slot_accessible_bytes);
    if(outcome==OriginalPostAllocateResult::InvalidNativeAddressOrBounds)
        return OriginalAllocatorCallStatus::InvalidOriginalMemory;
    return outcome==OriginalPostAllocateResult::OriginalNullResultPublished
        ? OriginalAllocatorCallStatus::OriginalAllocatorReturnedNull
        : OriginalAllocatorCallStatus::OriginalRecordInitializedAndPublished;
}

} // namespace ds_port::original_runtime
