#include "ds_port/original_stage_zero_type_two.hpp"
#include "ds_port/original_stage_zero_setup.hpp"
#include <array>
#include <cstdint>
#include <iostream>
using namespace ds_port::original_runtime;
#define CHECK(x) do {if(!(x)){std::cerr << "Original ARM type-2 mismatch " << __LINE__ << ": " << #x << "\n";return 1;}}while(0)
namespace {
std::uint32_t get32(const std::uint8_t* v) {
    return std::uint32_t(v[0]) |
        (std::uint32_t(v[1])<<8) |
        (std::uint32_t(v[2])<<16) |
        (std::uint32_t(v[3])<<24);
}
std::array<std::uint8_t,128> context{};
unsigned clock_calls=0,first_calls=0;
std::uint32_t original_token_observed=0;
bool simulate_clock_failure=false;
bool original_gettimeofday_fixture(std::int32_t* seconds,
                                   std::int32_t* microseconds) noexcept {
    ++clock_calls;
    if(simulate_clock_failure)return false;
    *seconds=3;*microseconds=400000;
    return true;
}
void original_first_followup_fixture(std::uint32_t token) noexcept {
    ++first_calls;original_token_observed=token;
    context[4]=0x78;context[5]=0x56;context[6]=0x34;context[7]=0x12;
}
}
int main(){
    std::array<std::uint8_t,44> object{};
    object.fill(0xab);
    std::array<std::uint8_t,4> source{{0x00,0x00,0x80,0x3f}};
    const auto untouched=object;
    CHECK(!original_initialize_type_two_0x28ca24(
        nullptr,object.size(),source.data(),source.size()));
    CHECK(!original_initialize_type_two_0x28ca24(
        object.data(),0x17,source.data(),source.size()));
    CHECK(!original_initialize_type_two_0x28ca24(
        object.data(),object.size(),nullptr,source.size()));
    CHECK(!original_initialize_type_two_0x28ca24(
        object.data(),object.size(),source.data(),3));
    CHECK(object==untouched);
    CHECK(original_initialize_type_two_0x28ca24(
        object.data()+1,0x18,source.data(),4));
    CHECK(get32(object.data()+1+0x10)==2);
    CHECK(get32(object.data()+1+0x14)==0x3f800000u);
    for(std::size_t i=0;i<object.size();++i) {
        const bool touched=(i>=1+0x10 && i<1+0x18);
        if(!touched)CHECK(object[i]==untouched[i]);
    }
    // VFP load and store copy bits including negative zero / NaN; they
    // do not modify the byte pattern or perform float arithmetic.
    const std::array<std::uint32_t,4> patterns{{
        0x80000000u,0x7fc00001u,0x3eaaaaabu,0xffffffffu}};
    for(const auto bits:patterns) {
        source={static_cast<std::uint8_t>(bits),
                static_cast<std::uint8_t>(bits>>8),
                static_cast<std::uint8_t>(bits>>16),
                static_cast<std::uint8_t>(bits>>24)};
        CHECK(original_initialize_type_two_0x28ca24(
            object.data()+1,0x18,source.data(),4));
        CHECK(get32(object.data()+1+0x10)==2u);
        CHECK(get32(object.data()+1+0x14)==bits);
    }
    // The real ARM frame caller uses type 2, so integrate BOTH fully
    // recovered functions 0x28CA24 and 0x2833F4 around the still-
    // unresolved 0x281D54 callback. Both calls use original field offsets.
    context.fill(0x55);
    for(int i=0x40;i<0x48;i++)context[i]=0;
    context[4]=0xef;context[5]=0xbe;context[6]=0xad;context[7]=0xde;
    source={{0x00,0x00,0x40,0x3f}}; // current original field: raw 0.75f
    CHECK(original_execute_stage_zero_setup_with_recovered_type_two(
        context.data(),0x57,source.data(),4,
        original_gettimeofday_fixture,original_first_followup_fixture)
        ==OriginalStageZeroSetupResult::InvalidContext);
    CHECK(original_execute_stage_zero_setup_with_recovered_type_two(
        context.data(),context.size(),source.data(),3,
        original_gettimeofday_fixture,original_first_followup_fixture)
        ==OriginalStageZeroSetupResult::MissingOriginalCallee);
    CHECK(clock_calls==0 && first_calls==0);
    CHECK(original_execute_stage_zero_setup_with_recovered_type_two(
        context.data(),context.size(),source.data(),4,
        original_gettimeofday_fixture,original_first_followup_fixture)
        ==OriginalStageZeroSetupResult::VerifiedPrefixExecuted);
    CHECK(clock_calls==1 && first_calls==1);
    CHECK(original_token_observed==0xdeadbeefu);
    CHECK(get32(context.data()+0x50)==2u);
    CHECK(get32(context.data()+0x54)==0x3f400000u);
    CHECK(get32(context.data()+0x40)==3400000u);
    CHECK(get32(context.data()+0x44)==0u);
    CHECK(get32(context.data()+0x34)==0x12345678u);
    CHECK(get32(context.data()+0x38)==0u);
    CHECK(get32(context.data()+0x3c)==0x12345678u);

    // A seeded slot never calls gettimeofday on repeated execution;
    // the other callbacks still execute in their verified order.
    CHECK(original_execute_stage_zero_setup_with_recovered_type_two(
        context.data(),context.size(),source.data(),4,
        nullptr,original_first_followup_fixture)
        ==OriginalStageZeroSetupResult::VerifiedPrefixExecuted);
    CHECK(clock_calls==1 && first_calls==2);

    std::cout<<"Original ARM type-2 selected complete switch case verified\n";
    return 0;
}
