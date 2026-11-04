#pragma once
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
// Original ARM 0x00281D54 after virtual allocator (vtable offset 0x0C).
// Original virtual allocator is NOT reconstructed. Inputs represent
// original ARM r5 (owner) and original allocator r0 (record pointer).
// ARM pointers stay 32-bit numeric tokens; never reinterpret them as
// native x86-64 pointers. A separate caller-owned host memory span
// represents the object returned by the original allocator.
//
// If returned_ptr == 0, the original skips record stores and publishes
// a zero pointer in the global slot. Otherwise it writes all exact
// original record fields at +0x04,+0x08,+0x0C,+0x10 (single BYTE),
// +0x14,+0x18,+0x1C and publishes returned_ptr.
// Invalid bounds fail BEFORE any writes.
enum class OriginalPostAllocateResult : std::uint8_t {
    InvalidNativeAddressOrBounds,
    OriginalNullResultPublished,
    OriginalRecordInitializedAndPublished
};
OriginalPostAllocateResult original_complete_allocator_postreturn_0x281d54(
    void* original_record_host_memory,
    std::size_t original_record_accessible_bytes,
    std::uint32_t original_owner_address_r5,
    std::uint32_t original_returned_record_address_r0,
    void* original_global_slot_host_memory,
    std::size_t original_global_slot_accessible_bytes) noexcept;
} // namespace ds_port::original_runtime
