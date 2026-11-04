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
    // This is a synthetic host-memory fixture for original slots:
    // allocator 0x00490B30, queue 0x00490B24. Their values, owner
    // object vtable words and method targets must agree before dispatch.
    std::array<std::uint8_t,4> allocator_slot{},queue_slot{};
    std::array<std::uint8_t,4> allocator_owner{},queue_owner{};
    std::array<std::uint8_t,0x10> allocator_table{};
    std::array<std::uint8_t,0x24> queue_table{};
    const auto put=[](std::uint8_t* p,std::uint32_t v){
        for(unsigned i=0;i<4;++i)p[i]=static_cast<std::uint8_t>(v>>(8*i));
    };
    put(allocator_slot.data(),0x1000);put(queue_slot.data(),0x3000);
    put(allocator_owner.data(),0x1110);put(queue_owner.data(),0x3330);
    put(allocator_table.data()+0x0c,0x2000);
    put(queue_table.data()+0x20,0x4000);
    OriginalInputMappedOwners mapped{
        {allocator_slot.data(),allocator_slot.size(),0x1000,
         allocator_owner.data(),allocator_owner.size(),
         0x1110,allocator_table.data(),allocator_table.size()},
        {queue_slot.data(),queue_slot.size(),0x3000,
         queue_owner.data(),queue_owner.size(),
         0x3330,queue_table.data(),queue_table.size()}
    };
    OriginalInputServices svc{};
    assert(original_resolve_input_virtual_owners(mapped,allocate,queue,&t,&svc));
    assert(svc.pool_owner_arm_va==0x1000&&svc.allocator_method_arm_va==0x2000);
    assert(svc.queue_owner_arm_va==0x3000&&svc.queue_method_arm_va==0x4000);
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

    // Original ARM singleton data must not be guessed or silently accepted.
    OriginalInputServices out=svc;
    out.pool_owner_arm_va=0xdeadbeef;
    put(allocator_slot.data(),0x7777);
    assert(!original_resolve_input_virtual_owners(mapped,allocate,queue,&t,&out));
    assert(out.pool_owner_arm_va==0xdeadbeef);
    put(allocator_slot.data(),0x1000);
    put(queue_table.data()+0x20,0);
    assert(!original_resolve_input_virtual_owners(mapped,allocate,queue,&t,&out));
    put(queue_table.data()+0x20,0x4000);
    mapped.queue.vtable_size=0x20;
    assert(!original_resolve_input_virtual_owners(mapped,allocate,queue,&t,&out));
    return 0;
}
