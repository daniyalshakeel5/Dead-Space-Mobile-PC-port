#include "ds_port/original_clock_source.hpp"
#include <cstdint>
#include <limits>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/time.h>
#endif

namespace ds_port::original_runtime {

bool original_read_platform_gettimeofday(
    std::int32_t* seconds, std::int32_t* microseconds) noexcept {
    if (!seconds || !microseconds) return false;
    std::int64_t seconds64 = 0;
    std::int64_t micros64 = 0;
#ifdef _WIN32
    // gettimeofday is a POSIX API imported by the original ARM library.
    // Windows FILETIME is 100-ns ticks from 1601-01-01 UTC; convert to
    // POSIX Unix epoch and microseconds, not monotonic elapsed time.
    FILETIME now{};
    GetSystemTimePreciseAsFileTime(&now);
    ULARGE_INTEGER tick{};
    tick.LowPart = now.dwLowDateTime;
    tick.HighPart = now.dwHighDateTime;
    constexpr std::uint64_t kEpochOffset100ns=116444736000000000ULL;
    constexpr std::uint64_t kTicksPerSecond=10000000ULL;
    if (tick.QuadPart < kEpochOffset100ns) return false;
    const std::uint64_t unix_ticks = tick.QuadPart - kEpochOffset100ns;
    seconds64 = static_cast<std::int64_t>(unix_ticks / kTicksPerSecond);
    micros64 = static_cast<std::int64_t>((unix_ticks % kTicksPerSecond) / 10ULL);
#else
    timeval value{};
    if (gettimeofday(&value, nullptr) != 0) return false;
    seconds64 = static_cast<std::int64_t>(value.tv_sec);
    micros64 = static_cast<std::int64_t>(value.tv_usec);
#endif
    // The original binary has a 32-bit time_t/timeval. Do not silently
    // wrap out-of-range values across its ABI (notably after Y2038).
    if (seconds64 < std::numeric_limits<std::int32_t>::min() ||
        seconds64 > std::numeric_limits<std::int32_t>::max() ||
        micros64 < 0 || micros64 >= 1000000)
        return false;
    *seconds = static_cast<std::int32_t>(seconds64);
    *microseconds = static_cast<std::int32_t>(micros64);
    return true;
}

OriginalGetTimeOfDaySeedResult original_seed_time_using_gettimeofday(
    void* original_context, std::size_t available_bytes,
    OriginalGetTimeOfDay time_source) noexcept {
    if (!original_context || available_bytes < 0x48)
        return OriginalGetTimeOfDaySeedResult::InvalidContext;
    const auto* bytes=static_cast<const std::uint8_t*>(original_context);
    // Proven original ARM at 0x00280538:
    // LDR r1,[r4,#0x40]; LDR r3,[r4,#0x44]; ORRS r1,r1,r3;
    // BNE 0x0028056c. No clock call in this branch.
    for (std::size_t i=0x40;i<0x48;++i)
        if (bytes[i]!=0)
            return OriginalGetTimeOfDaySeedResult::OriginalSlotAlreadyInitialized;
    if (!time_source)
        return OriginalGetTimeOfDaySeedResult::OriginalClockProviderUnavailable;

    std::int32_t secs=0;
    std::int32_t micros=0;
    if (!time_source(&secs,&micros) || micros < 0 || micros >= 1000000)
        return OriginalGetTimeOfDaySeedResult::OriginalClockProviderUnavailable;

    // The existing independently ARM-proven SMLAL + STRD implementation
    // preserves the original signed 64-bit arithmetic and +0x40 offset.
    const auto state=original_seed_time_pair_if_zero(
        original_context,available_bytes,secs,micros);
    if (state == OriginalClockSeedResult::OriginalEmptySlotSeeded)
        return OriginalGetTimeOfDaySeedResult::OriginalSlotSeeded;
    if (state == OriginalClockSeedResult::ExistingValueKept)
        return OriginalGetTimeOfDaySeedResult::OriginalSlotAlreadyInitialized;
    return OriginalGetTimeOfDaySeedResult::InvalidContext;
}

} // namespace ds_port::original_runtime
