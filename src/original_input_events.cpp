#include "ds_port/original_input_events.hpp"
#include <cstdint>
#include <cstring>
#include <mutex>
namespace ds_port::original_runtime {
namespace {
void store32(std::uint8_t* p,std::uint32_t value) noexcept {
    for(unsigned i=0;i<4;i++)p[i]=static_cast<std::uint8_t>(value>>(8*i));
}
// The original Bionic __atomic_swap(new_value, volatile int32_t*)
// takes r0=0,r1=record+4; the ignored return is the prior word.
// Serialize all of this port's mapped ARM32 atomic swaps with one lock.
// memcpy avoids undefined type punning on a byte-addressed ARM image.
void original_atomic_swap_zero(std::uint8_t* mapped_word) noexcept {
    static std::mutex swap_mutex;
    std::lock_guard<std::mutex> guard(swap_mutex);
    store32(mapped_word,0);
}
void store_float(std::uint8_t* p,float value) noexcept {
    static_assert(sizeof(float)==4,"original ABI uses float32");
    std::uint32_t bits=0;
    std::memcpy(&bits,&value,4);
    store32(p,bits);
std::uint32_t read32(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1])<<8) |
           (std::uint32_t(p[2])<<16) | (std::uint32_t(p[3])<<24);
}
bool resolve_owner(const OriginalInputMappedOwner& x,std::size_t slot_offset,
                   std::uint32_t* owner,std::uint32_t* method) noexcept {
    if(!owner || !method || !x.singleton_slot_bytes ||
       x.singleton_slot_size<4 || !x.original_owner_arm_va ||
       !x.owner_bytes || x.owner_size<4 || !x.original_vtable_arm_va ||
       !x.vtable_bytes || x.vtable_size<slot_offset+4)
        return false;
    const auto global=read32(static_cast<const std::uint8_t*>(x.singleton_slot_bytes));
    if(global!=x.original_owner_arm_va)return false;
    const auto vt=read32(static_cast<const std::uint8_t*>(x.owner_bytes));
    if(vt!=x.original_vtable_arm_va)return false;
    const auto target=read32(static_cast<const std::uint8_t*>(x.vtable_bytes)+slot_offset);
    if(!target)return false;
    *owner=global;*method=target;
    return true;
}

}
}
bool original_resolve_input_virtual_owners(
    const OriginalInputMappedOwners& mapped,
    OriginalInputAllocate allocate_callback,OriginalInputQueue queue_callback,
    void* context,OriginalInputServices* out) noexcept {
    if(!out || !allocate_callback || !queue_callback)return false;
    OriginalInputServices resolved{};
    if(!resolve_owner(mapped.allocator,0x0cu,
                      &resolved.pool_owner_arm_va,&resolved.allocator_method_arm_va))
        return false;
    if(!resolve_owner(mapped.queue,0x20u,
                      &resolved.queue_owner_arm_va,&resolved.queue_method_arm_va))
        return false;
    resolved.allocate=allocate_callback;
    resolved.queue=queue_callback;
    resolved.context=context;
    *out=resolved;
    return true;
}
OriginalInputStatus original_dispatch_input_event(
    bool ready,const OriginalInputPacket& in,
    const OriginalInputServices& svc) noexcept {
    // All three original JNI wrappers call ARM 0x27E97C and return
    // before allocating when its readiness predicate is false.
    if(!ready)return OriginalInputStatus::NotReady;
    if(!svc.pool_owner_arm_va || !svc.allocator_method_arm_va ||
       !svc.queue_owner_arm_va || !svc.queue_method_arm_va ||
       !svc.allocate || !svc.queue)
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
    // STR initial table; atomic swap zero at +4; STR pool at +8;
    // STR final table at +0. The imported helper stays explicit.
    auto* record=static_cast<std::uint8_t*>(allocated.mapped_bytes);
    store32(record,kOriginalInputRecordInitialVtable);
    // ELF PLT 0x35194 was independently identified by R_ARM_JUMP_SLOT
    // relocation as Bionic __atomic_swap. Original r0=0, r1=record+4;
    // r2 still contains a type pointer but is ignored by this import.
    original_atomic_swap_zero(record+4);
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
