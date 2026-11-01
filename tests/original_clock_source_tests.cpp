#include "ds_port/original_clock_source.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

using namespace ds_port::original_runtime;

#define REQUIRE(x) do { if(!(x)) {     std::cerr<<"Original gettimeofday timing mismatch at "<<__LINE__<<": "<<#x<<"\n";     return 1; } } while(0)

namespace {
unsigned read_calls=0;
bool should_fail=false;
std::int32_t fake_secs=12;
std::int32_t fake_micros=345678;
bool deterministic_gettimeofday(std::int32_t* seconds,
                                std::int32_t* micros) noexcept {
    ++read_calls;
    if (should_fail || !seconds || !micros) return false;
    *seconds=fake_secs;
    *micros=fake_micros;
    return true;
}
std::uint64_t load_original_le64(const std::uint8_t* bytes) {
    std::uint64_t result=0;
    for(unsigned i=0;i<8;++i)
        result |= std::uint64_t(bytes[0x40+i]) << (i*8);
    return result;
}
}

int main() {
    std::array<std::uint8_t,96> frame{};
    frame.fill(0x7b);
    for(std::size_t i=0x40;i<0x48;++i) frame[i]=0;
    const auto original_frame=frame;

    REQUIRE(original_seed_time_using_gettimeofday(
        nullptr,frame.size(),deterministic_gettimeofday)
        ==OriginalGetTimeOfDaySeedResult::InvalidContext);
    REQUIRE(original_seed_time_using_gettimeofday(
        frame.data(),0x47,deterministic_gettimeofday)
        ==OriginalGetTimeOfDaySeedResult::InvalidContext);
    REQUIRE(read_calls==0);
    REQUIRE(frame==original_frame);

    REQUIRE(original_seed_time_using_gettimeofday(
        frame.data(),frame.size(),nullptr)
        ==OriginalGetTimeOfDaySeedResult::OriginalClockProviderUnavailable);
    REQUIRE(frame==original_frame);

    should_fail=true;
    REQUIRE(original_seed_time_using_gettimeofday(
        frame.data(),frame.size(),deterministic_gettimeofday)
        ==OriginalGetTimeOfDaySeedResult::OriginalClockProviderUnavailable);
    REQUIRE(read_calls==1);
    REQUIRE(frame==original_frame);
    should_fail=false;

    fake_micros=1000000; // gettimeofday usec is always 0..999999
    REQUIRE(original_seed_time_using_gettimeofday(
        frame.data(),frame.size(),deterministic_gettimeofday)
        ==OriginalGetTimeOfDaySeedResult::OriginalClockProviderUnavailable);
    REQUIRE(frame==original_frame);
    fake_micros=345678;

    REQUIRE(original_seed_time_using_gettimeofday(
        frame.data(),frame.size(),deterministic_gettimeofday)
        ==OriginalGetTimeOfDaySeedResult::OriginalSlotSeeded);
    REQUIRE(load_original_le64(frame.data())==12345678u);
    REQUIRE(read_calls==3);
    for(std::size_t i=0;i<frame.size();++i)
        if(i<0x40||i>=0x48) REQUIRE(frame[i]==original_frame[i]);

    const auto seeded_frame=frame;
    REQUIRE(original_seed_time_using_gettimeofday(
        frame.data(),frame.size(),deterministic_gettimeofday)
        ==OriginalGetTimeOfDaySeedResult::OriginalSlotAlreadyInitialized);
    REQUIRE(read_calls==3); // clock MUST NOT be called twice
    REQUIRE(frame==seeded_frame);

    frame=original_frame;
    frame[0x45]=1; // High 32-bit word nonzero, even if low word zero.
    const auto existing_frame=frame;
    REQUIRE(original_seed_time_using_gettimeofday(
        frame.data(),frame.size(),deterministic_gettimeofday)
        ==OriginalGetTimeOfDaySeedResult::OriginalSlotAlreadyInitialized);
    REQUIRE(read_calls==3);
    REQUIRE(frame==existing_frame);

    frame=original_frame;
    fake_secs=-2;
    fake_micros=500000;
    REQUIRE(original_seed_time_using_gettimeofday(
        frame.data(),frame.size(),deterministic_gettimeofday)
        ==OriginalGetTimeOfDaySeedResult::OriginalSlotSeeded);
    REQUIRE(static_cast<std::int64_t>(load_original_le64(frame.data()))
        ==-1500000LL);
    REQUIRE(read_calls==4);

    std::int32_t sec=0,usec=0;
    REQUIRE(!original_read_platform_gettimeofday(nullptr,&usec));
    REQUIRE(!original_read_platform_gettimeofday(&sec,nullptr));
    REQUIRE(original_read_platform_gettimeofday(&sec,&usec));
    REQUIRE(sec>0 && usec>=0 && usec<1000000);

    std::cout<<"Original gettimeofday source, one-time clock guard, ARM microsecond math pass\n";
    return 0;
}
