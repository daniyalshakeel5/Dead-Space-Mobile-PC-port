#pragma once
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {

// Recovered ARM signed-long multiply-accumulate in original native function
// ELF 0x00280520: high 32-bit input is sign-extended, then added to the
// signed lower input multiplied by EXACTLY 1,000,000. No guessed frame rate.
std::int64_t original_time_pair_scaled_sum(
    std::int32_t large_units, std::int32_t small_units) noexcept;

// Independent C++ equivalent of original ARM STRD of the 64-bit result into
// state object at byte offset 0x40. This is an isolated instruction subset,
// not complete initialization or platform clock handling.
bool original_store_time_pair_at_offset_0x40(
    void* state, std::size_t accessible_bytes,
    std::int32_t large_units, std::int32_t small_units) noexcept;

// Native 0x00280520 (AFTER its unresolved setup callback): load both
// words at +0x40/+0x44, ORRS, BNE on nonzero. Only for an all-zero pair does
// it obtain original time units and store the scaled 64-bit value at +0x40.
// Caller must pass those real clock values; this function does not generate
// or simulate time, and does not call the missing original setup routine.
enum class OriginalClockSeedResult : std::uint8_t {
    InvalidContext,
    ExistingValueKept,
    OriginalEmptySlotSeeded
};
OriginalClockSeedResult original_seed_time_pair_if_zero(
    void* state, std::size_t accessible_bytes,
    std::int32_t original_large_units,
    std::int32_t original_small_units) noexcept;

} // namespace ds_port::original_runtime
