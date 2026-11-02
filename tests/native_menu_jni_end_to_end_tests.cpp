#ifdef NDEBUG
#undef NDEBUG
#endif
#include "ds_port/native_menu_input.hpp"
#include "ds_port/original_input_jni_bridge.hpp"
#include "ds_port/original_input_events.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
using namespace ds_port;
using namespace ds_port::original_runtime;
struct EventHarness {
    std::array<std::uint8_t,48> record{};
    std::vector<std::uint32_t> events;
    std::uint32_t token=0x10000;
};
static bool alloc(const OriginalInputAllocation& request,void* ctx,
                  OriginalInputRecord* output) noexcept {
    auto& h=*static_cast<EventHarness*>(ctx);
    if(request.size!=48)return false;
    h.record.fill(0);
    *output={h.token,h.record.data(),h.record.size()};
    return true;
}
static bool enqueue(const OriginalInputQueuedEvent& event,void* ctx) noexcept {
    auto& h=*static_cast<EventHarness*>(ctx);
    if(event.original_record_arm_va!=h.token)return false;
    h.events.push_back(event.event_id);
    return true;
}
static std::uint32_t word(const std::array<std::uint8_t,48>& data,int off) {
    std::uint32_t result=0;
    for(int i=0;i<4;++i)result|=std::uint32_t(data[off+i])<<(i*8);
    return result;
}
int main() {
    EventHarness h;
    std::array<std::uint8_t,4> readiness={1,0,0,0};
    // Exercise production mapped-runtime binding rather than bypassing it
    // with a directly injected OriginalInputServices struct.
    std::array<std::uint8_t,4> allocator_slot{},queue_slot{};
    std::array<std::uint8_t,4> allocator_object{},queue_object{};
    std::array<std::uint8_t,48> allocator_vtable{},queue_vtable{};
    auto put=[](auto& bytes,int offset,std::uint32_t value) {
        for(int i=0;i<4;++i)bytes[offset+i]=std::uint8_t(value>>(i*8));
    };
    put(allocator_slot,0,0x1234);put(queue_slot,0,0x9abc);
    put(allocator_object,0,0x1110);put(queue_object,0,0x2220);
    put(allocator_vtable,0x0c,0x5678);
    put(queue_vtable,0x20,0xdef0);
    OriginalInputMappedOwners owners{};
    owners.allocator={allocator_slot.data(),allocator_slot.size(),
        0x1234,allocator_object.data(),allocator_object.size(),
        0x1110,allocator_vtable.data(),allocator_vtable.size()};
    owners.queue={queue_slot.data(),queue_slot.size(),
        0x9abc,queue_object.data(),queue_object.size(),
        0x2220,queue_vtable.data(),queue_vtable.size()};
    assert(original_bind_input_mapped_runtime(
        owners,readiness.data(),readiness.size(),alloc,enqueue,&h));
    assert(original_last_input_status()==OriginalInputStatus::NotReady);
    NativeMenuInputCallbacks callbacks{};
    callbacks.pointer=[](void*,int event,int module,int id,float x,float y) {
        Java_com_ea_blast_TouchSurfaceAndroid_NativeOnPointerEvent(
            nullptr,nullptr,event,module,id,x,y);
    };
    callbacks.key_down=[](void*,int module,int key,int alt) {
        Java_com_ea_blast_KeyboardAndroid_NativeOnKeyDown(
            nullptr,nullptr,module,key,alt);
    };
    callbacks.key_up=[](void*,int module,int key,int alt) {
        Java_com_ea_blast_KeyboardAndroid_NativeOnKeyUp(
            nullptr,nullptr,module,key,alt);
    };
    NativeMenuInputState input{};
    assert(native_menu_pointer(input,callbacks,0,50,25,100,100));
    assert(original_last_input_status()==OriginalInputStatus::Completed);
    assert(h.events.size()==1&&h.events.back()==kOriginalRawPointerDown);
    assert(word(h.record,0)==kOriginalInputRecordFinalVtable);
    assert(word(h.record,0x10)==600);
    assert(word(h.record,0x20)==0);
    float x=0,y=0;
    auto xb=word(h.record,0x18),yb=word(h.record,0x1c);
    std::memcpy(&x,&xb,4);std::memcpy(&y,&yb,4);
    assert(x==0.5f&&y==0.25f);
    assert(native_menu_pointer(input,callbacks,2,60,40,100,100));
    assert(h.events.back()==kOriginalRawPointerMove);
    assert(native_menu_pointer(input,callbacks,1,60,40,100,100));
    assert(h.events.back()==kOriginalRawPointerUp);
    assert(native_menu_key(input,callbacks,66,true));
    assert(h.events.back()==kOriginalKeyDownEvent);
    assert(word(h.record,0x18)==66);
    assert(native_menu_key(input,callbacks,66,false));
    assert(h.events.back()==kOriginalKeyUpEvent);
    assert(h.events.size()==5);
    readiness[0]=0;
    assert(native_menu_key(input,callbacks,4,true));
    assert(original_last_input_status()==OriginalInputStatus::NotReady);
    assert(h.events.size()==5);
    original_unbind_input_services();
    assert(original_last_input_status()==OriginalInputStatus::MissingOriginalService);
    assert(native_menu_key(input,callbacks,4,false));
    assert(original_last_input_status()==OriginalInputStatus::MissingOriginalService);
    return 0;
}
