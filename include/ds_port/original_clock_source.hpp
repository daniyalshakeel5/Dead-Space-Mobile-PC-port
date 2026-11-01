#pragma once
#include "ds_port/original_timing_math.hpp"
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {

// Verified from original ARM ELF: 0x000357c4 is a gettimeofday PLT
// thunk (Ghidra resolution). At 0x00280520 the native engine passes
// a timeval-compatible stack buffer as r0, reads signed 32-bit seconds
// and microseconds, multiplies seconds by 1000000 and stores at +0x40.
//
// Source callback is injectable to verify exact call/no-call behavior
// without depending on the computer's wall clock. Its contract is
// gettimeofday(&seconds,&microseconds) semantics, NOT monotonic time.
// The rest of original routine 0x00280520 remains unimplemented.
using OriginalGetTimeOfDay = bool (*)(std::int32_t* seconds,
                                      std::int32_t* microseconds) noexcept;

enum class OriginalGetTimeOfDaySeedResult : std::uint8_t {
    InvalidContext,
    OriginalClockProviderUnavailable,
    OriginalSlotAlreadyInitialized,
    OriginalSlotSeeded
};

// OS-native counterpart of gettimeofday's two 32-bit output words.
// Linux uses libc gettimeofday; Windows uses FILETIME converted to the
// same Unix epoch / microsecond representation. This adapter is NOT a
// port of the remainder of the game's ARM frame initialization routine.
bool original_read_platform_gettimeofday(
    std::int32_t* seconds, std::int32_t* microseconds) noexcept;

// The verified original zero guard happens BEFORE the native clock
// call. The callback is never invoked for an existing 64-bit value.
// Failure leaves the original memory entirely unchanged.
OriginalGetTimeOfDaySeedResult original_seed_time_using_gettimeofday(
    void* original_context, std::size_t available_bytes,
    OriginalGetTimeOfDay time_source) noexcept;

} // namespace ds_port::original_runtime
