#pragma once
#include "ds_port/original_resource_utf8_decode.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

namespace ds_port::original_runtime {
// Original 0x28611C accepts a source byte-string range and a mutable
// three-word UTF-16 destination [begin,end,capacity_end]. Host-owned
// storage replaces ARM pointers but preserves contents and capacity
// decisions observable to subsequent original parser operations.
struct OriginalRecordUtf16Buffer {
    std::u16string contents;
    // Logical native equivalent of (capacity_end - begin)/2.
    // This INCLUDES the final UTF-16 NUL; an empty buffer needs one unit.
    std::size_t original_capacity_code_units=1u;
};
enum class OriginalRecordReplaceStatus : std::uint8_t {
    Completed, MalformedEncoding, InvalidInput, ExceedsLimit, AllocationFailed,
    InvalidDestination
};
// Entire original 0x28611C text-replacement branch: first count UTF-16
// units through 0x28CC88, compare to old length, choose exact growth
// when needed, decode into destination and update end/terminator.
// The subsequent 0x2862E8 owner vtable dispatch is NOT included here.
OriginalRecordReplaceStatus original_replace_record_text_0x28611c(
    OriginalRecordUtf16Buffer& target,
    const std::uint8_t* source,std::size_t source_bytes,
    std::size_t maximum_code_units=1024u*1024u) noexcept;
} // namespace ds_port::original_runtime
