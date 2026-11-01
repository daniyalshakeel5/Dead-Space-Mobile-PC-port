#include "ds_port/original_frame_dispatch.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
using namespace ds_port::original_runtime;

#define REQUIRE(x) do { if(!(x)) { \
    std::cerr<<"Original state-two instruction mismatch "<<__LINE__<<": "<<#x<<"\\n"; \
    return 1; } } while(0)

int main() {
    std::array<std::uint8_t,48> context{};
    context.fill(0xb6);
    void* next=reinterpret_cast<void*>(0x1); // sentinel, never dereference
    auto original=context;
    REQUIRE(original_write_state_two_transition_prefix(nullptr,48,&next)
        ==OriginalStateTwoPrefixResult::InvalidOriginalStateObject);
    REQUIRE(original_write_state_two_transition_prefix(context.data(),7,&next)
        ==OriginalStateTwoPrefixResult::InvalidOriginalStateObject);
    REQUIRE(original_write_state_two_transition_prefix(context.data(),48,nullptr)
        ==OriginalStateTwoPrefixResult::InvalidOriginalStateObject);
    REQUIRE(context==original);
    REQUIRE(next==reinterpret_cast<void*>(0x1));
    REQUIRE(original_write_state_two_transition_prefix(context.data(),48,&next)
        ==OriginalStateTwoPrefixResult::OriginalStateTwoStored);
    REQUIRE(next==context.data()+8);
    REQUIRE(context[0]==2 && context[1]==0 && context[2]==0 && context[3]==0);
    for(std::size_t i=4;i<context.size();++i)REQUIRE(context[i]==original[i]);

    // When an old frame stage was active, original STR overwrites it.
    context.fill(0);
    context[0]=1;
    next=nullptr;
    REQUIRE(original_write_state_two_transition_prefix(context.data(),8,&next)
        ==OriginalStateTwoPrefixResult::OriginalStateTwoStored);
    REQUIRE(next==context.data()+8);
    REQUIRE(context[0]==2);
    for(std::size_t i=1;i<context.size();++i)REQUIRE(context[i]==0);

    std::cout<<"Original ARM MOV #2 + STR postincrement at transition entry verified\\n";
    return 0;
}
