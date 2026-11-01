#include "ds_port/original_stage_zero_type_two.hpp"
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {
namespace {
void arm_store_le32(std::uint8_t* out, std::size_t offset,
                    std::uint32_t bits) noexcept {
    out[offset]   = static_cast<std::uint8_t>(bits);
    out[offset+1] = static_cast<std::uint8_t>(bits >> 8);
    out[offset+2] = static_cast<std::uint8_t>(bits >> 16);
    out[offset+3] = static_cast<std::uint8_t>(bits >> 24);
}
std::uint32_t arm_read_le32(const std::uint8_t* bytes) noexcept {
    return std::uint32_t(bytes[0]) |
        (std::uint32_t(bytes[1])<<8) |
        (std::uint32_t(bytes[2])<<16) |
        (std::uint32_t(bytes[3])<<24);
}
}

bool original_initialize_type_two_0x28ca24(
    void* original_destination, std::size_t destination_bytes,
    const void* runtime_float_field_0x0049cd1c,
    std::size_t source_bytes) noexcept {
    if (!original_destination || destination_bytes < 0x18 ||
        !runtime_float_field_0x0049cd1c || source_bytes < 4)
        return false;
    auto* bytes=static_cast<std::uint8_t*>(original_destination);
    // Original ARM 0x28CA24: VLDR s15,[PC,#0xF4] reads
    // 0x3F800000 (1.0f) from original literal pool 0x28CB20;
    // VSTR s15,[r0,#0x14], STR r1,[r0,#0x10], with r1=2.
    arm_store_le32(bytes,0x14,0x3f800000u);
    arm_store_le32(bytes,0x10,2u);
    // ARM type-2 jump table selects 0x28CAD8. The VLDR at
    // 0x28CAE0 loads raw bits from original runtime VA 0x49CD1C,
    // then VSTR overwrites [r0,#0x14]. No value is fabricated.
    // Read AFTER the initial writes, matching native order.
    const auto bits=arm_read_le32(
        static_cast<const std::uint8_t*>(runtime_float_field_0x0049cd1c));
    arm_store_le32(bytes,0x14,bits);
    return true;
}
} // namespace ds_port::original_runtime
