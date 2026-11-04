#pragma once

#include <chrono>
#include <cstdint>

namespace ds_port {

// Desktop host timing only. This does NOT assert the original Android engine's
// simulation rate, physics rules or the desired frame rate for gameplay.
struct TickBatch {
    std::uint32_t updates = 0;
    std::uint32_t dropped_updates = 0;
    double interpolation = 0.0;  // [0,1) for presentation only.
};

class FixedStepClock {
public:
    // Frequency must be 1..1000. Max updates must be 1..100.
    explicit FixedStepClock(std::uint32_t hz = 60, std::uint32_t max_updates = 8);

    TickBatch advance(std::chrono::nanoseconds elapsed);
    void reset();
    std::uint32_t frequency() const noexcept { return hz_; }
    std::uint64_t total_updates() const noexcept { return total_updates_; }
    std::uint64_t total_dropped_updates() const noexcept { return total_dropped_; }

private:
    std::uint32_t hz_;
    std::uint32_t max_updates_;
    std::uint64_t fractional_units_ = 0;
    std::uint64_t total_updates_ = 0;
    std::uint64_t total_dropped_ = 0;
};

} // namespace ds_port
