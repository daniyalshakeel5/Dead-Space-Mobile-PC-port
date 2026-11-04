#include "ds_port/original_stage_zero_followup.hpp"
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {
namespace {
void arm_le32_store(std::uint8_t* bytes, std::size_t offset,
                    std::uint32_t value) noexcept {
    bytes[offset]     = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}
}

bool original_followup_store_0x2833f4(
    void* original_object, std::size_t accessible_bytes,
    std::uint32_t original_r1_value) noexcept {
    if (!original_object || accessible_bytes < 0x28)
        return false;
    auto* bytes = static_cast<std::uint8_t*>(original_object);
    // Preserve the original ARM store order, not merely final values.
    arm_le32_store(bytes, 0x20, 0u);
    arm_le32_store(bytes, 0x1c, original_r1_value);
    arm_le32_store(bytes, 0x24, original_r1_value);
    return true;
}
} // namespace ds_port::original_runtime
