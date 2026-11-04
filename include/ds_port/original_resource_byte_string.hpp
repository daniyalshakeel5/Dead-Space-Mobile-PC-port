#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace ds_port::original_runtime {
// Native observable behavior of ARM 0x285918. Original object layout
// is [begin,end,capacity_end,allocator,vtable] in 32-bit address space;
// this host object deliberately owns data with native std::string.
struct OriginalResourceByteString {
    std::string contents;
    // Logical number of bytes including the original trailing NUL.
    std::size_t original_capacity_bytes=1;
};
enum class OriginalResourceAppendStatus : std::uint8_t {
    Completed, InvalidRange, ExceedsLimit, AllocationFailed
};
// Original ARM 0x285918 appends [begin,end) and maintains a trailing 0.
// The original capacity path uses max(required, capacity<=8?8:2*capacity)
// plus one byte for the terminator; capacity here excludes that NUL.
// This helper processes actual mapped source data. It is not ARM execution.
OriginalResourceAppendStatus original_append_bytes_0x285918(
    OriginalResourceByteString& target,
    const std::uint8_t* bytes, std::size_t byte_count,
    std::size_t maximum_content_bytes=16u*1024u*1024u) noexcept;
// Instruction-verified 0x28650C..0x2865B4 parser staging branches:
// compare source range length with the existing temporary string length;
// only append the as-yet uncopied suffix using original 0x285918.
// A corresponding branch repeats for the second parser string.
OriginalResourceAppendStatus original_complete_parser_range_0x2863ec(
    OriginalResourceByteString& target,
    const std::uint8_t* mapped_begin, std::size_t original_range_bytes,
    std::size_t maximum_content_bytes=16u*1024u*1024u) noexcept;
}
