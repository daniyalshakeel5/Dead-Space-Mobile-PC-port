#pragma once
#include "ds_port/original_owner_byte_line.hpp"
#include "ds_port/original_resource_utf16.hpp"
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {

// Actual original ARM 0x26D5FC is called from mode-8 owner 0x276D18
// AFTER its +0x68 byte-line read. Independent validation tables
// were recovered from the user's original APK (workflow 41340559730).
// Unlike resource decoder 0x28CC88, an invalid sequence returns the
// number of COMPLETE UTF-16 units preceding it, not an error -1.
enum class OriginalOwnerUtf8Status : std::uint8_t {
    Completed, StoppedAtInvalidSequence, InvalidInput, ExceedsLimit
};
struct OriginalOwnerUtf8Result {
    OriginalOwnerUtf8Status status;
    std::size_t original_r0_code_units;
    std::size_t consumed_source_bytes;
};
OriginalOwnerUtf8Result original_decode_stream_utf8_0x26d5fc(
    char16_t* output,std::size_t output_capacity_code_units,
    const std::uint8_t* input,std::size_t byte_count,
    std::size_t maximum_source_bytes=0x4000u) noexcept;

// 0x276D18 mode-8 native equivalent: read original byte line,
// resize its UTF-16 destination and decode with ACTUAL 0x26D5FC
// byte-to-wide semantics. The original provider vtable and later
// tree/scene registration are NOT implemented by this function.
enum class OriginalOwnerWideLineStatus : std::uint8_t {
    LineDecoded, LineDecodedPrefix, EndOfStream,
    UnsupportedOriginalMode, InvalidInput, LineTooLong,
    AllocationFailed
};
struct OriginalOwnerWideLineResult {
    OriginalOwnerWideLineStatus status;
    std::size_t consumed_stream_bytes;
    std::size_t decoded_code_units;
};
OriginalOwnerWideLineResult original_read_wide_line_0x276d18(
    std::uint32_t owner_mode_0x8b0,
    OriginalNativeByteSource& source,
    OriginalNativeUtf16String& output) noexcept;

} // namespace ds_port::original_runtime
