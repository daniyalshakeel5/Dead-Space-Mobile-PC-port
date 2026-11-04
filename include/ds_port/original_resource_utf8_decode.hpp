#pragma once
#include "ds_port/original_resource_utf16.hpp"
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
// Actual original ARM 0x28CC88 UTF-8 -> 16-bit conversion. The lookup
// table and numeric ranges were verified from original APK metadata in
// GitHub Actions run 41331509759. One output 16-bit unit is emitted
// even for four-byte UTF-8 input (high code point truncated).
enum class OriginalUtf8DecodeStatus : std::uint8_t {
    Completed, MalformedOriginalEncoding, InvalidHostInput, ExceedsHostLimit
};
struct OriginalUtf8DecodeResult {
    OriginalUtf8DecodeStatus status;
    std::int32_t original_r0;
};
// Output capacity includes the UTF-16 terminator. Set output=nullptr
// to emulate original ARM r0=0 length-only query; source bytes are
// bounded by original r3. Invalid prefixes return original r0=-1.
OriginalUtf8DecodeResult original_utf8_to_utf16_0x28cc88(
    char16_t* output,std::size_t output_capacity_code_units,
    const std::uint8_t* source,std::size_t source_bytes,
    std::size_t maximum_source_bytes=16u*1024u*1024u) noexcept;
// The two-call resource-record conversion phase in original 0x28611C:
// compute required units, reserve, then decode into an owned host
// UTF-16 string. This does NOT implement its subsequent record mutation.
OriginalUtf8DecodeStatus original_decode_record_text_0x28611c(
    OriginalNativeUtf16String& output,
    const std::uint8_t* source,std::size_t source_bytes,
    std::size_t maximum_source_bytes=16u*1024u*1024u) noexcept;
} // namespace ds_port::original_runtime
