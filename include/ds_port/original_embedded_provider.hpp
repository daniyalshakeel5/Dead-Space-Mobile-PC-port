#pragma once
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
// Original embedded provider vtable 0x004896F8, verified from the
// user's APK (run 41341080277). These are the REAL ARM targets:
// +0x14 -> 0x26C3C4, +0x28 -> 0x271794,
// +0x2C -> 0x273034, +0x34 -> 0x2716F8.
inline constexpr std::uint32_t kOriginalEmbeddedProviderVtable=0x004896f8u;
enum class OriginalEmbeddedProviderStatus : std::uint8_t {
    Completed, InvalidMappedProvider, NeedsOriginalFileService,
    NeedsOriginalMemoryService
};
struct OriginalEmbeddedProviderResult {
    OriginalEmbeddedProviderStatus status;
    std::uint32_t original_r0;
};
// +0x14 is exactly two ARM instructions:
// LDR r0,[r0,#0x90]; BX lr.
// Requires a caller-mapped original provider, never a host pointer token.
OriginalEmbeddedProviderResult original_embedded_provider_mode_0x26c3c4(
    const void* mapped_provider,std::size_t accessible_bytes) noexcept;
// +0x28 position: verified branch for provider+0xA4==1
// (in-memory provider), selector r1=0. Other branches call original
// file IO 0x35350 or vtable +0x20 and remain explicitly blocked.
OriginalEmbeddedProviderResult original_embedded_provider_position_0x271794(
    const void* mapped_provider,std::size_t accessible_bytes,
    std::uint32_t original_selector) noexcept;
} // namespace ds_port::original_runtime
