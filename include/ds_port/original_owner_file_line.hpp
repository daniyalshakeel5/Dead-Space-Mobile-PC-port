#pragma once
#include "ds_port/original_owner_line_utf8.hpp"
#include "ds_port/original_embedded_provider.hpp"
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {
// Native x86-64 HOST adapter for original owner mode-8 text streams.
// Reads real local user-owned asset files by calling the recovered
// provider methods: +0x34 (0x2716F8) -> read, +0x28 (0x271794)
// -> current position, +0x2C (0x273034) -> seek. Converts through
// original 0x26D5FC after original 0x285918 byte accumulation.
// Does NOT emulate ARM execution or implement the missing codec.
struct OriginalNativeFileLineSource {
    void* mapped_original_embedded_provider=nullptr;
    std::size_t provider_accessible_bytes=0;
    // The host application's previously OPENED file descriptor,
    // explicitly matching the provider's original 32-bit +0x10.
    // The caller retains file ownership and must close it.
    int native_host_fd=-1;
};
OriginalOwnerWideLineResult original_read_native_file_line_0x276d18(
    std::uint32_t original_owner_mode,
    const OriginalNativeFileLineSource& file,
    OriginalNativeUtf16String& output) noexcept;
} // namespace ds_port::original_runtime
