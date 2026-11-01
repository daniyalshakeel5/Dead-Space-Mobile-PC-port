#include "ds_port/original_input_jni_bridge.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <array>
#include <cstdint>
using namespace ds_port::original_runtime;
namespace {
struct Harness {
    std::array<std::uint8_t,48> record{};
    int allocations=0,queues=0;
    std::array<std::uint8_t,4> frame_state{};
    std::uint32_t last_event=0;
    std::uint32_t last_key=0;
};
std::uint32_t get32(const std::uint8_t* p){
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
bool allocate(const OriginalInputAllocation& a,void* ctx,
              OriginalInputRecord* record) noexcept {
    auto& h=*static_cast<Harness*>(ctx);++h.allocations;
    assert(a.size==48&&a.stack0==8&&a.stack1==0);
    *record={0x5000,h.record.data(),h.record.size()};
    return true;
}
bool queue(const OriginalInputQueuedEvent& a,void* ctx) noexcept {
    auto& h=*static_cast<Harness*>(ctx);++h.queues;
    assert(a.original_record_arm_va==0x5000);
    assert(get32(h.record.data()+4)==0);
    h.last_event=a.event_id;h.last_key=get32(h.record.data()+0x18);
    return true;
}
}
int main(){
    Harness h{};
    original_unbind_input_services();
    Java_com_ea_blast_TouchSurfaceAndroid_NativeOnPointerEvent(
        nullptr,nullptr,kOriginalRawPointerDown,1000,1,123.f,70.f);
    assert(original_last_input_status()==OriginalInputStatus::MissingOriginalService);
    OriginalInputBinding b{
        {0x1000,0x2000,0x3000,0x4000,allocate,queue,&h},
        h.frame_state.data(),h.frame_state.size()
    };
    original_bind_input_services(b);
    Java_com_ea_blast_KeyboardAndroid_NativeOnKeyDown(nullptr,nullptr,600,99,1);
    assert(original_last_input_status()==OriginalInputStatus::NotReady);
    assert(h.allocations==0&&h.queues==0);
    // Directly simulate the original frame-state word that production
    // runtime will map from 0x0049CAA8; no readiness callback.
    h.frame_state[0]=1;
    assert(original_input_ready_0x27e97c(h.frame_state.data(),4));
    assert(!original_input_ready_0x27e97c(h.frame_state.data(),3));
    assert(!original_input_ready_0x27e97c(nullptr,4));
    Java_com_ea_blast_TouchSurfaceAndroid_NativeOnPointerEvent(
        nullptr,nullptr,kOriginalRawPointerDown,1000,1,123.f,70.f);
    assert(original_last_input_status()==OriginalInputStatus::Completed);
    assert(h.last_event==kOriginalRawPointerDown&&h.queues==1);
    assert(get32(h.record.data()+0x10)==1000);
    assert(get32(h.record.data()+0x20)==1);
    Java_com_ea_blast_KeyboardAndroid_NativeOnKeyDown(nullptr,nullptr,600,99,1);
    assert(h.last_event==kOriginalKeyDownEvent&&h.last_key==99);
    Java_com_ea_blast_KeyboardAndroid_NativeOnKeyUp(nullptr,nullptr,600,99,0);
    assert(h.last_event==kOriginalKeyUpEvent&&h.queues==3);
    assert(get32(h.record.data()+0x1c)==0);
    h.frame_state[0]=2;
    Java_com_ea_blast_KeyboardAndroid_NativeOnKeyDown(nullptr,nullptr,600,99,1);
    assert(original_last_input_status()==OriginalInputStatus::NotReady);
    assert(h.queues==3);
    h.frame_state[0]=1;
    original_unbind_input_services();
    Java_com_ea_blast_KeyboardAndroid_NativeOnKeyDown(nullptr,nullptr,600,99,1);
    assert(original_last_input_status()==OriginalInputStatus::MissingOriginalService);
    assert(h.queues==3);
    return 0;
}
