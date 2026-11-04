#include "ds_port/timing.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
using ds_port::FixedStepClock;

#define REQUIRE(expr) do { if (!(expr)) { \
    std::cerr << "FAIL line " << __LINE__ << ": " << #expr << '\n'; return 1; \
} } while (false)

int main() {
    using namespace std::chrono;

    // One exact second broken into 100 ms slices = exactly 60 updates.
    FixedStepClock sixty;
    unsigned ticks = 0;
    for (int i = 0; i < 10; ++i) {
        auto batch = sixty.advance(milliseconds(100));
        REQUIRE(batch.updates == 6);
        REQUIRE(batch.dropped_updates == 0);
        REQUIRE(batch.interpolation >= 0.0 && batch.interpolation < 1.0);
        ticks += batch.updates;
    }
    REQUIRE(ticks == 60);
    REQUIRE(sixty.total_updates() == 60);

    // A 144 Hz display does not force the update loop to 144 Hz.
    FixedStepClock high_refresh;
    for (int i = 0; i < 144; ++i)
        high_refresh.advance(nanoseconds(i < 96 ? 6'944'444 : 6'944'445));
    REQUIRE(high_refresh.total_updates() >= 59);
    REQUIRE(high_refresh.total_updates() <= 60);

    // 30 Hz and 60 Hz schedulers are independent of the display loop.
    FixedStepClock thirty(30);
    for (int i = 0; i < 10; ++i)
        REQUIRE(thirty.advance(milliseconds(100)).updates == 3);
    REQUIRE(thirty.total_updates() == 30);

    // Long stall is clamped to 250ms, and only eight updates may run at once.
    FixedStepClock capped;
    auto lag = capped.advance(hours(2));
    REQUIRE(lag.updates == 8);
    REQUIRE(lag.dropped_updates == 7);
    REQUIRE(capped.total_dropped_updates() == 7);
    REQUIRE(capped.advance(nanoseconds(-5)).updates == 0);
    capped.reset();
    REQUIRE(capped.total_updates() == 0);
    REQUIRE(capped.total_dropped_updates() == 0);
    REQUIRE(capped.advance(nanoseconds(0)).updates == 0);

    // Refuse invalid or pathological configurations.
    bool invalid = false;
    try { FixedStepClock bad(0); (void)bad; }
    catch (const std::invalid_argument&) { invalid = true; }
    REQUIRE(invalid);
    invalid = false;
    try { FixedStepClock bad(60, 0); (void)bad; }
    catch (const std::invalid_argument&) { invalid = true; }
    REQUIRE(invalid);

    // 16'666'667ns * 60 crosses one simulation tick.
    auto single = capped.advance(nanoseconds(16'666'667));
    REQUIRE(single.updates == 1);
    REQUIRE(single.interpolation >= 0.0 && single.interpolation < 1.0);
    std::cout << "Desktop timing scheduler tests passed.\n";
    return 0;
}
