#include "ds_port/original_input_events.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <array>
#include <cstdint>
#include <cstring>
using namespace ds_port::original_runtime;
namespace {
std::uint32_t load32(const std::uint8_t* b) {
    return std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|
        (std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);
}
struct Trace {
    std::array<std::uint8_t,48> data{};
    int allocations=0,queues=0;
    bool fail_alloc=false,fail_queue=false;
    bool null_result=false;
    OriginalInputPacket expected{};
};
bool allocate(const OriginalInputAllocation& a,void* ctx,
              OriginalInputRecord* out) noexcept {
    auto& t=*static_cast<Trace*>(ctx);++t.allocations;
    assert(a.original_pool_arm_va==0x1000);
    assert(a.original_allocator_method_arm_va==0x2000);
    assert(a.size==0x30&&a.r2==0&&a.r3==0&&a.stack0==8&&a.stack1==0);
    if(t.fail_alloc)return false;
    *out={t.null_result?0u:0x5000u,t.data.data(),t.data.size()};
    return true;
}
bool queue(const OriginalInputQueuedEvent& q,void* ctx) noexcept {
    auto& t=*static_cast<Trace*>(ctx);++t.queues;
    assert(q.original_queue_owner_arm_va==0x3000);
    assert(q.original_queue_method_arm_va==0x4000);
    assert(q.original_record_arm_va==0x5000&&q.r3==0&&q.stack0==0);
    assert(load32(t.data.data())==kOriginalInputRecordFinalVtable);
    assert(load32(t.data.data()+4)==0);
    assert(load32(t.data.data()+8)==0x1000);
    assert(load32(t.data.data()+0x10)==t.expected.module_id);
    if(t.expected.kind==OriginalInputPacket::Kind::Pointer) {
        std::uint32_t xb=0,yb=0;
        std::memcpy(&xb,&t.expected.x,4);std::memcpy(&yb,&t.expected.y,4);
        assert(q.event_id==t.expected.event_id);
        assert(load32(t.data.data()+0x18)==xb);
        assert(load32(t.data.data()+0x1c)==yb);
        assert(load32(t.data.data()+0x20)==t.expected.pointer_id);
    } else {
        assert(q.event_id==(t.expected.kind==OriginalInputPacket::Kind::KeyDown?
            kOriginalKeyDownEvent:kOriginalKeyUpEvent));
        assert(load32(t.data.data()+0x18)==t.expected.android_key);
        assert(load32(t.data.data()+0x1c)==t.expected.alt_pressed);
    }
    return !t.fail_queue;
}
}
int main() {
    static_assert(kOriginalRawPointerDown==0x6000c);
    static_assert(kOriginalRawPointerMove==0x4000c);
    static_assert(kOriginalRawPointerUp==0x8000c);
    Trace t{};
    OriginalInputServices svc{0x1000,0x2000,0x3000,0x4000,
        allocate,queue,&t};
    OriginalInputPacket p{OriginalInputPacket::Kind::Pointer,
        kOriginalRawPointerDown,1000,7,123.25f,-48.5f,0,0};
    t.expected=p;
    auto run=[&](bool ready=true){
        return original_dispatch_input_event(ready,t.expected,svc);
    };
    assert(run(false)==OriginalInputStatus::NotReady);
    assert(t.allocations==0&&t.queues==0);
    t.data.fill(0xA5);
    assert(run()==OriginalInputStatus::Completed);
    assert(t.allocations==1&&t.queues==1);
    for(auto raw:{kOriginalRawPointerMove,kOriginalRawPointerUp}){
        t.expected.event_id=raw;
        assert(run()==OriginalInputStatus::Completed);
    }
    t.expected={OriginalInputPacket::Kind::KeyDown,0,600,0,0,0,99,1};
    assert(run()==OriginalInputStatus::Completed);
    t.expected.kind=OriginalInputPacket::Kind::KeyUp;
    assert(run()==OriginalInputStatus::Completed);
    t.null_result=true;
    assert(run()==OriginalInputStatus::AllocationReturnedNull);
    assert(t.queues==5);
    t.null_result=false;t.fail_alloc=true;
    assert(run()==OriginalInputStatus::AllocationRefused);
    t.fail_alloc=false;t.fail_queue=true;
    assert(run()==OriginalInputStatus::QueueRefused);
    t.fail_queue=false;svc.queue=nullptr;
    assert(run()==OriginalInputStatus::MissingOriginalService);
    return 0;
}
