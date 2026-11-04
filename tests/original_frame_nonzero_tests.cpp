#include "ds_port/original_frame_dispatch.hpp"
#include <array>
#include <cstdint>
#include <iostream>

using namespace ds_port::original_runtime;
#define REQUIRE(x) do { if(!(x)){ std::cerr<<"ARM nonzero branch mismatch at "<<__LINE__<<"\n"; return 1; } } while(0)

namespace {
std::array<std::uint8_t,160> global{};
int backend_first=10, backend_second=20;
void* selected_backend=&backend_first;
void* event_object=nullptr;
void* followup_object=nullptr;
void* resolver_state=nullptr;
unsigned resolver_calls=0, event_calls=0, followup_calls=0, active_calls=0;
std::array<int,2> order{};
std::uint32_t active_result=0;
bool erase_backend_after_event=false;
void* actual_backend_resolver(void* global_state) noexcept {
    ++resolver_calls;
    resolver_state=global_state;
    return selected_backend;
}
void event_virtual(void* object, std::uint32_t code, std::uint32_t a,
                   std::uint32_t b, std::uint32_t stack_zero) noexcept {
    ++event_calls;
    event_object=object;
    order[0]=1;
    if(code!=14 || a!=0 || b!=0 || stack_zero!=0) order[0]=-1;
    selected_backend=erase_backend_after_event ? nullptr : &backend_second;
}
void followup_virtual(void* object) noexcept {
    ++followup_calls;
    followup_object=object;
    order[1]=2;
}
std::uint32_t active_virtual(void* object) noexcept {
    ++active_calls;
    if(object!=global.data()+24) return 0xffffffffu;
    return active_result;
}
void reset() {
    global.fill(0xcd);
    global[0]=1;global[1]=0;global[2]=0;global[3]=0;
    selected_backend=&backend_first;
    resolver_state=nullptr;event_object=nullptr;followup_object=nullptr;
    resolver_calls=event_calls=followup_calls=active_calls=0;
    order={};active_result=0;erase_backend_after_event=false;
}
}
int main() {
    reset();
    auto before=global;
    REQUIRE(original_execute_nonzero_virtual_sequence(
        global.data(),nullptr,event_virtual,followup_virtual)
        ==OriginalNonzeroVirtualSequenceResult::MissingOriginalTargets);
    REQUIRE(original_execute_nonzero_virtual_sequence(
        global.data(),actual_backend_resolver,nullptr,followup_virtual)
        ==OriginalNonzeroVirtualSequenceResult::MissingOriginalTargets);
    REQUIRE(event_calls==0 && followup_calls==0 && resolver_calls==0);

    void* post_transition=reinterpret_cast<void*>(0x1);
    REQUIRE(original_execute_active_frame_verified_branches(
        global.data(),global.size(),global.data()+24,active_virtual,
        &post_transition,actual_backend_resolver,event_virtual,followup_virtual)
        ==OriginalActiveFramePrefixResult::OriginalNonzeroTwoVirtualCallsExecuted);
    REQUIRE(active_calls==1 && resolver_calls==2 && event_calls==1 && followup_calls==1);
    REQUIRE(resolver_state==global.data());
    REQUIRE(event_object==&backend_first);
    REQUIRE(followup_object==&backend_second); // RELOADED after first method
    REQUIRE(order[0]==1 && order[1]==2);
    REQUIRE(global==before);
    REQUIRE(post_transition==reinterpret_cast<void*>(0x1));

    reset();
    selected_backend=nullptr;
    REQUIRE(original_execute_nonzero_virtual_sequence(
        global.data(),actual_backend_resolver,event_virtual,followup_virtual)
        ==OriginalNonzeroVirtualSequenceResult::FirstBackendUnavailable);
    REQUIRE(resolver_calls==1 && event_calls==0 && followup_calls==0);

    reset();
    erase_backend_after_event=true;
    REQUIRE(original_execute_active_frame_verified_branches(
        global.data(),global.size(),global.data()+24,active_virtual,
        &post_transition,actual_backend_resolver,event_virtual,followup_virtual)
        ==OriginalActiveFramePrefixResult::OriginalNonzeroFirstCallOnly);
    REQUIRE(active_calls==1 && resolver_calls==2 && event_calls==1 && followup_calls==0);
    REQUIRE(global==before);

    // Original virtual method returning exactly 1 switches the same shared
    // global stage to 2; nonzero event methods must NOT be called.
    reset();
    active_result=1;
    post_transition=nullptr;
    REQUIRE(original_execute_active_frame_verified_branches(
        global.data(),global.size(),global.data()+24,active_virtual,
        &post_transition,actual_backend_resolver,event_virtual,followup_virtual)
        ==OriginalActiveFramePrefixResult::OriginalStateTwoTransitionPrefixExecuted);
    REQUIRE(active_calls==1 && resolver_calls==0 && event_calls==0 && followup_calls==0);
    REQUIRE(global[0]==2 && global[1]==0 && global[2]==0 && global[3]==0);
    REQUIRE(post_transition==global.data()+8);
    for(std::size_t i=4;i<global.size();++i) REQUIRE(global[i]==before[i]);

    reset();
    active_result=2;
    REQUIRE(original_execute_active_frame_verified_branches(
        global.data(),global.size(),global.data()+24,active_virtual,
        &post_transition,nullptr,event_virtual,followup_virtual)
        ==OriginalActiveFramePrefixResult::OriginalNonzeroBranchUnimplemented);
    REQUIRE(active_calls==1 && resolver_calls==0 && event_calls==0);

    reset();
    global[0]=3;
    REQUIRE(original_execute_active_frame_verified_branches(
        global.data(),global.size(),global.data()+24,active_virtual,
        &post_transition,actual_backend_resolver,event_virtual,followup_virtual)
        ==OriginalActiveFramePrefixResult::OriginalStateWasNotOne);
    REQUIRE(active_calls==0 && resolver_calls==0 && event_calls==0);

    std::cout<<"Original ARM nonzero two-vtable branch, event14, backend reload pass\n";
    return 0;
}
