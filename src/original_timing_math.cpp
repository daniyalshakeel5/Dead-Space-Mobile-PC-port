#include "ds_port/original_timing_math.hpp"

namespace ds_port::original_runtime {
std::int64_t original_time_pair_scaled_sum(
    std::int32_t large_units, std::int32_t small_units) noexcept {
    // ARM MOV r1,#0xf4000; ADD r1,r1,#0x240 -> exactly 0xF4240 = 1000000
    // ARM ASR r3,r2,#31; SMLAL r2,r3,r1,r0 (signed 32x32 plus signed 64).
    return static_cast<std::int64_t>(large_units) * 1000000LL
         + static_cast<std::int64_t>(small_units);
}

bool original_store_time_pair_at_offset_0x40(
    void* state, std::size_t accessible_bytes,
    std::int32_t large_units, std::int32_t small_units) noexcept {
    if (!state || accessible_bytes < 0x48) return false;
    const auto result = static_cast<std::uint64_t>(
        original_time_pair_scaled_sum(large_units,small_units));
    auto* bytes = static_cast<std::uint8_t*>(state);
    // ARM STRD r2,r3,[r4,#0x40], original library is ELF32 little-endian.
    for(unsigned i=0;i<8;++i)
        bytes[0x40+i] = static_cast<std::uint8_t>((result>>(i*8))&0xffu);
    return true;
}
} // namespace ds_port::original_runtime
