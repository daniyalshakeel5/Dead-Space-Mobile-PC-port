#include "ds_port/timing.hpp"

#include <algorithm>
#include <stdexcept>

namespace ds_port {
namespace {
constexpr std::uint64_t one_second_ns = 1'000'000'000ULL;
constexpr std::uint64_t max_elapsed_ns = 250'000'000ULL;
}

FixedStepClock::FixedStepClock(std::uint32_t hz, std::uint32_t max_updates)
    : hz_(hz), max_updates_(max_updates) {
    if (hz == 0 || hz > 1000 || max_updates == 0 || max_updates > 100)
        throw std::invalid_argument("invalid desktop update scheduler configuration");
}

TickBatch FixedStepClock::advance(std::chrono::nanoseconds elapsed) {
    // Negative durations are possible after a clock reset or caller bug.
    const std::uint64_t duration = elapsed.count() > 0
        ? static_cast<std::uint64_t>(elapsed.count()) : 0;
    // Limit very long stalls, e.g. minimized window, debugger breaks.
    const std::uint64_t bounded = std::min(duration, max_elapsed_ns);
    fractional_units_ += bounded * hz_;
    const std::uint64_t whole = fractional_units_ / one_second_ns;
    fractional_units_ %= one_second_ns;

    TickBatch result;
    result.updates = static_cast<std::uint32_t>(
        std::min(whole, static_cast<std::uint64_t>(max_updates_)));
    result.dropped_updates = static_cast<std::uint32_t>(whole - result.updates);
    result.interpolation = static_cast<double>(fractional_units_) /
                           static_cast<double>(one_second_ns);
    total_updates_ += result.updates;
    total_dropped_ += result.dropped_updates;
    return result;
}

void FixedStepClock::reset() {
    // Reset both the fractional time and cumulative diagnostics.
    fractional_units_ = 0;
    total_updates_ = 0;
    total_dropped_ = 0;
}
} // namespace ds_port
