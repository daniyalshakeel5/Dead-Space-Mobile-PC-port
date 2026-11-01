#include "ds_port/original_frame_dispatch.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
using namespace ds_port::original_runtime;

#define REQUIRE(cond) do { if(!(cond)) { \
  std::cerr << "Original frame dispatcher mismatch at " << __LINE__ \
            << ": " << #cond << "\n"; return 1; } } while(0)

static std::array<std::uint8_t,32> memory{};
static int init_calls=0;
static bool init_saw_stage_zero=false;
static bool init_saw_exact_address=false;
static void original_init_test_only(void* context) noexcept {
    ++init_calls;
    init_saw_exact_address=(context==memory.data()+8);
    std::uint32_t stage=10;
    std::memcpy(&stage,memory.data(),sizeof(stage));
    init_saw_stage_zero=(stage==0);
}
static void set_state(std::uint32_t value) {
    std::memcpy(memory.data(),&value,sizeof(value));
}
static std::uint32_t get_state() {
    std::uint32_t value=0;
    std::memcpy(&value,memory.data(),sizeof(value));
    return value;
}
static std::array<int,2> original_call_order{};
static int chain_calls=0;
static bool same_context=true;
static void first_native_call_test_only(void* arg) noexcept {
    if(chain_calls>=2)return;
    original_call_order[chain_calls++]=1;
    same_context = same_context && (arg==memory.data()+8);
}
static void second_native_call_test_only(void* arg) noexcept {
    if(chain_calls>=2)return;
    original_call_order[chain_calls++]=2;
    same_context = same_context && (arg==memory.data()+8);
}
int main() {
    memory.fill(0x22);
    set_state(0);
    REQUIRE(execute_original_frame_prefix(
        memory.data(),memory.size(),nullptr)
        ==FramePrefixResult::MissingOriginalInitRoutine);
    REQUIRE(get_state()==0);
    REQUIRE(init_calls==0);
    REQUIRE(execute_original_frame_prefix(
        memory.data(),memory.size(),original_init_test_only)
        ==FramePrefixResult::OriginalInitCompleted);
    REQUIRE(init_calls==1);
    REQUIRE(init_saw_stage_zero);
    REQUIRE(init_saw_exact_address);
    REQUIRE(get_state()==1);
    for(std::size_t i=4;i<memory.size();++i) REQUIRE(memory[i]==0x22);

    REQUIRE(execute_original_frame_prefix(
        memory.data(),memory.size(),original_init_test_only)
        ==FramePrefixResult::OriginalActiveFrameNotYetReconstructed);
    REQUIRE(init_calls==1);
    REQUIRE(get_state()==1);

    set_state(2);
    REQUIRE(execute_original_frame_prefix(
        memory.data(),memory.size(),original_init_test_only)
        ==FramePrefixResult::OriginalOtherStateReturned);
    REQUIRE(get_state()==2);
    REQUIRE(init_calls==1);

    set_state(0xffffffffu);
    REQUIRE(execute_original_frame_prefix(
        memory.data(),memory.size(),original_init_test_only)
        ==FramePrefixResult::OriginalOtherStateReturned);
    REQUIRE(get_state()==0xffffffffu);
    REQUIRE(init_calls==1);

    REQUIRE(execute_original_frame_prefix(
        nullptr,memory.size(),original_init_test_only)
        ==FramePrefixResult::InvalidContext);
    REQUIRE(execute_original_frame_prefix(
        memory.data(),8,original_init_test_only)
        ==FramePrefixResult::InvalidContext);
    REQUIRE(execute_original_stage_zero_chain(
        memory.data()+8,first_native_call_test_only,second_native_call_test_only)
        ==StageZeroChainResult::OriginalTwoCallsCompleted);
    REQUIRE(chain_calls==2);
    REQUIRE(original_call_order[0]==1);
    REQUIRE(original_call_order[1]==2);
    REQUIRE(same_context);
    REQUIRE(execute_original_stage_zero_chain(
        memory.data()+8,nullptr,second_native_call_test_only)
        ==StageZeroChainResult::MissingNativeCallee);
    REQUIRE(chain_calls==2);

    REQUIRE(original_stage_one_normalized_result(0u)==1u);
    REQUIRE(original_stage_one_normalized_result(1u)==0u);
    REQUIRE(original_stage_one_normalized_result(2u)==1u);
    REQUIRE(original_stage_one_normalized_result(0xffffffffu)==1u);
        std::cout << "Original ARM frame state-zero transition and dispatch verified\n";
    return 0;
}
