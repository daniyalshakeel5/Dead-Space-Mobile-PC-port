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
    // Production-style mapping fixture: original ARM BSS global
    // pointers + owner objects + 0x0C/0x20 virtual call slots.
    std::array<std::uint8_t,4> pool_global{},queue_global{};
    std::array<std::uint8_t,4> pool_owner{},queue_owner{};
    std::array<std::uint8_t,0x10> pool_table{};
    std::array<std::uint8_t,0x24> queue_table{};
    const auto put=[](std::uint8_t* p,std::uint32_t v) {
        for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(v>>(i*8));
    };
    put(pool_global.data(),0x1000);put(queue_global.data(),0x3000);
    put(pool_owner.data(),0x1110);put(queue_owner.data(),0x3330);
    put(pool_table.data()+0x0c,0x2000);
    put(queue_table.data()+0x20,0x4000);
    OriginalInputMappedOwners mapped{
        {pool_global.data(),pool_global.size(),0x1000,
         pool_owner.data(),pool_owner.size(),0x1110,
         pool_table.data(),pool_table.size()},
        {queue_global.data(),queue_global.size(),0x3000,
         queue_owner.data(),queue_owner.size(),0x3330,
         queue_table.data(),queue_table.size()}
    };
    mapped.allocator.original_owner_arm_va=0x9999;
    assert(!original_bind_input_mapped_runtime(
        mapped,h.frame_state.data(),4,allocate,queue,&h));
    assert(original_last_input_status()==OriginalInputStatus::MissingOriginalService);
    mapped.allocator.original_owner_arm_va=0x1000;
    assert(original_bind_input_mapped_runtime(
        mapped,h.frame_state.data(),4,allocate,queue,&h));
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
