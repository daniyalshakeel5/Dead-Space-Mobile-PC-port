#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace ds_port::original_runtime {
struct OriginalNativeUtf16String {
    std::u16string contents;
};
enum class OriginalNativeUtf16Status:std::uint8_t {
    Completed,InvalidOriginalRange,ExceedsLimit,AllocationFailed
};
// Original ARM 0x285590 receives a pair of byte pointers, computes
// (end-begin)>>1, copies exactly two bytes per UTF-16 unit, and
// appends a UTF-16 NUL. Host uses std::u16string, not ARM allocation.
OriginalNativeUtf16Status original_copy_utf16_range_0x285590(
    OriginalNativeUtf16String& out,
    const std::uint8_t* mapped_bytes,std::size_t byte_count,
    std::size_t maximum_code_units=1024u*1024u) noexcept;
} // namespace ds_port::original_runtime
