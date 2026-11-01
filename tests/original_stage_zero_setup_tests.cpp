#include "ds_port/original_stage_zero_setup.hpp"
#include <array>
#include <cstdint>
#include <iostream>
using namespace ds_port::original_runtime;
#define CHECK(expr) do { if(!(expr)){std::cerr<<"ARM stage zero mismatch: "<<__LINE__<<" "<<#expr<<"\n";return 1;} } while(0)
namespace {
std::array<std::uint8_t,128> state{};
int stage=0,clock_calls=0,setup_calls=0,first_calls=0,second_calls=0;
void* seen_setup=nullptr;void* seen_context=nullptr;
std::uint32_t first_value=0,second_value=0;
bool fail_clock=false;
void setup(void* context,std::uint32_t value) noexcept {
    ++setup_calls; if(stage!=0||context!=state.data()+0x40||value!=2)stage=-10;
    else stage=1;
    seen_setup=context;
}
bool clock(std::int32_t* seconds,std::int32_t* microseconds) noexcept {
    ++clock_calls;
    if(stage!=1)stage=-10;
    if(fail_clock)return false;
    *seconds=2;*microseconds=350000;
    stage=2;return true;
}
void followup_a(std::uint32_t original_token) noexcept {
    ++first_calls;first_value=original_token;
    if(stage!=2 && stage!=1)stage=-10;else stage=3;
    // Original LDR reload must observe changed bytes from this callback.
    state[4]=0x78;state[5]=0x56;state[6]=0x34;state[7]=0x12;
}
void followup_b(void* addr,std::uint32_t original_token) noexcept {
    ++second_calls;seen_context=addr;second_value=original_token;
    if(stage!=3)stage=-10;else stage=4;
}
void reset() {
    state.fill(0);state[4]=0xef;state[5]=0xbe;state[6]=0xad;state[7]=0xde;
    stage=clock_calls=setup_calls=first_calls=second_calls=0;
    seen_setup=seen_context=nullptr;first_value=second_value=0;
    fail_clock=false;
}
}
int main(){
    reset();
    CHECK(original_execute_stage_zero_setup_prefix(
        state.data(),0x47,setup,clock,followup_a,followup_b)
        ==OriginalStageZeroSetupResult::InvalidContext);
    CHECK(original_execute_stage_zero_setup_prefix(
        state.data(),state.size(),nullptr,clock,followup_a,followup_b)
        ==OriginalStageZeroSetupResult::MissingOriginalCallee);
    CHECK(setup_calls==0 && clock_calls==0);
    CHECK(original_execute_stage_zero_setup_prefix(
        state.data(),state.size(),setup,clock,followup_a,followup_b)
        ==OriginalStageZeroSetupResult::VerifiedPrefixExecuted);
    CHECK(stage==4 && setup_calls==1 && clock_calls==1);
    CHECK(first_calls==1 && second_calls==1);
    CHECK(seen_setup==state.data()+0x40);
    CHECK(seen_context==state.data()+0x18);
    CHECK(first_value==0xdeadbeefu && second_value==0x12345678u);
    constexpr std::uint64_t expected=2350000u;
    for(unsigned i=0;i<8;++i)
        CHECK(state[0x40+i]==static_cast<std::uint8_t>((expected>>(8*i))&255u));
    // Nonzero slot: gettimeofday must NOT run, but both later calls must.
    reset();state[0x44]=7;
    CHECK(original_execute_stage_zero_setup_prefix(
        state.data(),state.size(),setup,nullptr,followup_a,followup_b)
        ==OriginalStageZeroSetupResult::VerifiedPrefixExecuted);
    CHECK(stage==4 && clock_calls==0 && setup_calls==1);
    CHECK(state[0x44]==7);
    // Time-source failure: first setup may have run, later original
    // callbacks must not be executed with uninitialized stack input.
    reset();fail_clock=true;
    CHECK(original_execute_stage_zero_setup_prefix(
        state.data(),state.size(),setup,clock,followup_a,followup_b)
        ==OriginalStageZeroSetupResult::OriginalClockUnavailableAfterSetup);
    CHECK(setup_calls==1 && clock_calls==1 && first_calls==0 && second_calls==0);
    CHECK(state[0x40]==0);
    std::cout<<"Original ARM stage-zero initialization instruction prefix passed\n";
    return 0;
}
