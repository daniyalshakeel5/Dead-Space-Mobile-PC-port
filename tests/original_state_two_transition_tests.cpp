#include "ds_port/original_frame_dispatch.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
using namespace ds_port::original_runtime;

#define REQUIRE(x) do { if(!(x)) { \
    std::cerr<<"Original state-two instruction mismatch "<<__LINE__<<": "<<#x<<"\\n"; \
    return 1; } } while(0)

static unsigned native_virtual_calls=0;
static void* forwarded_virtual_object=nullptr;
static std::uint32_t native_virtual_return=0;
static std::uint32_t original_method_test_only(void* object) noexcept {
    ++native_virtual_calls;
    forwarded_virtual_object=object;
    return native_virtual_return;
}

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

    // The real ELF's frame and transition literal pools both address
    // the SAME state word. The composite stage-one path must use it,
    // rather than an unrelated test-state or fabricated update loop.
    context.fill(0x6d);
    context[0]=1;context[1]=0;context[2]=0;context[3]=0;
    const auto active_before=context;
    void* active_context=reinterpret_cast<void*>(0x1);
    void* vobject=context.data()+24;
    REQUIRE(original_execute_active_frame_prefix(
        context.data(),7,vobject,original_method_test_only,&active_context)
        ==OriginalActiveFramePrefixResult::InvalidOriginalContext);
    REQUIRE(original_execute_active_frame_prefix(
        context.data(),context.size(),vobject,nullptr,&active_context)
        ==OriginalActiveFramePrefixResult::MissingOriginalVirtualTarget);
    REQUIRE(native_virtual_calls==0);
    REQUIRE(context==active_before);

    native_virtual_return=0;
    REQUIRE(original_execute_active_frame_prefix(
        context.data(),context.size(),vobject,original_method_test_only,&active_context)
        ==OriginalActiveFramePrefixResult::OriginalNonzeroBranchUnimplemented);
    REQUIRE(native_virtual_calls==1);
    REQUIRE(forwarded_virtual_object==vobject);
    REQUIRE(context==active_before);
    REQUIRE(active_context==reinterpret_cast<void*>(0x1));

    native_virtual_return=1;
    REQUIRE(original_execute_active_frame_prefix(
        context.data(),context.size(),vobject,original_method_test_only,&active_context)
        ==OriginalActiveFramePrefixResult::OriginalStateTwoTransitionPrefixExecuted);
    REQUIRE(native_virtual_calls==2);
    REQUIRE(active_context==context.data()+8);
    REQUIRE(context[0]==2 && context[1]==0 && context[2]==0 && context[3]==0);
    for(std::size_t i=4;i<context.size();++i)
        REQUIRE(context[i]==active_before[i]);

    // Stage 2 cannot re-execute the stage-one virtual callback.
    const auto completed=context;
    REQUIRE(original_execute_active_frame_prefix(
        context.data(),context.size(),vobject,original_method_test_only,&active_context)
        ==OriginalActiveFramePrefixResult::OriginalStateWasNotOne);
    REQUIRE(context==completed);
    REQUIRE(native_virtual_calls==2);
    context=active_before;
    native_virtual_return=0xffffffffu;
    REQUIRE(original_execute_active_frame_prefix(
        context.data(),context.size(),vobject,original_method_test_only,&active_context)
        ==OriginalActiveFramePrefixResult::OriginalNonzeroBranchUnimplemented);
    REQUIRE(native_virtual_calls==3);
    REQUIRE(context==active_before);

    std::cout<<"Original ARM MOV #2 + STR postincrement at transition entry verified\\n";
    return 0;
}
