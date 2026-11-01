#pragma once
#include "ds_port/original_static_singleton.hpp"
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {
// Proven original ARM 0x271914 PC-relative global pointer slot,
// verified from the real APK in GitHub Action 41336986652.
// It is ALSO the embedded 0x27A4F0 allocator slot: original owner
// construction and default pool use the same native singleton.
inline constexpr std::uint32_t kOriginalDefaultPoolSlotVA=0x0049bca8u;

struct OriginalDefaultPoolInputs0x271914 {
    void* mapped_global_pointer_slot=nullptr;
    std::size_t accessible_slot_bytes=0;
    const OriginalSingletonInputs* native_singleton_0x27e560=nullptr;
};
enum class OriginalDefaultPoolStatus0x271914:std::uint8_t {
    CompletedExisting,CompletedNewSingleton,InvalidGlobalPointer,
    MissingOriginalSingleton,OriginalSingletonBlocked
};
struct OriginalDefaultPoolResult0x271914 {
    OriginalDefaultPoolStatus0x271914 status;
    std::uint32_t original_r0;
};
OriginalDefaultPoolResult0x271914 original_default_pool_0x271914(
    const OriginalDefaultPoolInputs0x271914&) noexcept;
} // namespace ds_port::original_runtime
