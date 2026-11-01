#include "ds_port/original_input_events.hpp"
#include <cstdint>
#include <cstring>
namespace ds_port::original_runtime {
namespace {
void store32(std::uint8_t* p,std::uint32_t value) noexcept {
    for(unsigned i=0;i<4;i++)p[i]=static_cast<std::uint8_t>(value>>(8*i));
}
void store_float(std::uint8_t* p,float value) noexcept {
    static_assert(sizeof(float)==4,"original ABI uses float32");
    std::uint32_t bits=0;
    std::memcpy(&bits,&value,4);
    store32(p,bits);
}
}
OriginalInputStatus original_dispatch_input_event(
    bool ready,const OriginalInputPacket& in,
    const OriginalInputServices& svc) noexcept {
    // All three original JNI wrappers call ARM 0x27E97C and return
    // before allocating when its readiness predicate is false.
    if(!ready)return OriginalInputStatus::NotReady;
    if(!svc.pool_owner_arm_va || !svc.allocator_method_arm_va ||
       !svc.queue_owner_arm_va || !svc.queue_method_arm_va ||
       !svc.allocate || !svc.constructor_import || !svc.queue)
        return OriginalInputStatus::MissingOriginalService;

    const OriginalInputAllocation request{
        svc.pool_owner_arm_va,svc.allocator_method_arm_va,0x30u,0u,0u,8u,0u
    };
    OriginalInputRecord allocated{};
    if(!svc.allocate(request,svc.context,&allocated))
        return OriginalInputStatus::AllocationRefused;
    // Original CMP r0,#0 / BEQ: no constructor or queue on null.
    if(allocated.arm_va==0)return OriginalInputStatus::AllocationReturnedNull;
    if(allocated.arm_va>0xfffffffbu || !allocated.mapped_bytes ||
       allocated.accessible_bytes<kOriginalInputRecordSize)
        return OriginalInputStatus::InvalidRecord;

    // Complete native stores for original function 0x2807A0:
    // STR initial table; imported helper; STR original pool at +8;
    // STR final table at +0. The imported helper stays explicit.
    auto* record=static_cast<std::uint8_t*>(allocated.mapped_bytes);
    store32(record,kOriginalInputRecordInitialVtable);
    if(!svc.constructor_import(0,allocated.arm_va+4,
          kOriginalInputRecordInitialVtable,record,
          allocated.accessible_bytes,svc.context))
        return OriginalInputStatus::ConstructorRefused;
    store32(record+8,svc.pool_owner_arm_va);
    store32(record,kOriginalInputRecordFinalVtable);

    std::uint32_t event_type=in.event_id;
    if(in.kind==OriginalInputPacket::Kind::Pointer) {
        // 0x27F9B4: float x/y at +0x18,+0x1C; r3 module at +0x10;
        // pointer id (first stacked argument) at +0x20.
        store_float(record+0x18,in.x);
        store_float(record+0x1c,in.y);
        store32(record+0x10,in.module_id);
        store32(record+0x20,in.pointer_id);
    } else {
        // 0x284828/0x284770: r2 module at +0x10; r3 key at
        // +0x18; first stacked word altPressed at +0x1C.
        store32(record+0x10,in.module_id);
        store32(record+0x18,in.android_key);
        store32(record+0x1c,in.alt_pressed);
        event_type=in.kind==OriginalInputPacket::Kind::KeyDown
            ? kOriginalKeyDownEvent:kOriginalKeyUpEvent;
    }
    // Original event queue singleton virtual method +0x20:
    // (queueOwner,eventId,recordArmToken,0,stack0=0).
    const OriginalInputQueuedEvent queued{
        svc.queue_owner_arm_va,svc.queue_method_arm_va,event_type,
        allocated.arm_va,0,0
    };
    if(!svc.queue(queued,svc.context))
        return OriginalInputStatus::QueueRefused;
    return OriginalInputStatus::Completed;
}
}
