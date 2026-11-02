#ifdef NDEBUG
#undef NDEBUG
#endif
#include "guest_xperia_input.hpp"
#include "guest_libc_bridge.hpp"
#include "guest_heap.hpp"
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
using namespace xperia;
namespace {
void word(GuestElf32& elf,std::uint32_t at,std::uint32_t v) {
    auto* dst=elf.writeable(at,4);
    assert(dst);
    for(int i=0;i<4;++i)dst[i]=static_cast<std::uint8_t>(v>>(8*i));
}
}
int main(){
    constexpr std::uint32_t base=0x10001000u;
    GuestElf32 elf;
    elf.segments.push_back({base,std::vector<std::uint8_t>(4096,0)});
    GuestXperiaInput input;
    std::string error;
    assert(!input.bind(elf,error));
    // Stand-in A32 JNI exports record r2 (key/module), r3 (key/event),
    // and fifth stack argument into guest memory so the ABI is observable.
    word(elf,base,0xe59f1020u); // LDR R1,[PC,#32], target @+0x28
    word(elf,base+4,0xe5812000u); // STR R2,[R1]
    word(elf,base+8,0xe5813004u); // STR R3,[R1,#4]
    word(elf,base+12,0xe59d0000u); // LDR R0,[SP]
    word(elf,base+16,0xe5810008u); // STR R0,[R1,#8]
    word(elf,base+20,0xe12fff1eu); // BX LR
    word(elf,base+40,base+0x800);
    elf.jni_exports["Java_com_ea_blast_KeyboardAndroid_NativeOnKeyDown"]=base;
    elf.jni_exports["Java_com_ea_blast_KeyboardAndroid_NativeOnKeyUp"]=base;
    elf.jni_exports["Java_com_ea_blast_TouchSurfaceAndroid_NativeOnPointerEvent"]=base;
    assert(input.bind(elf,error));
    input.button(ds_port::Ds4Button::Cross,true);
    input.button(ds_port::Ds4Button::Cross,true);
    assert(input.pending()==1);
    GuestLibcBridge bridge;
    GuestHeap heap(elf);
    GuestJniVm jni;
    assert(jni.install(elf,error));
    GuestArmCallRunner runner(elf,bridge,heap,nullptr,&jni);
    assert(input.flush(runner,error));
    assert(elf.read_u32(base+0x800)==600);
    assert(elf.read_u32(base+0x804)==23);
    assert(elf.read_u32(base+0x808)==1);
    input.pointer(0x6000c,1000,3,300.f,200.f);
    assert(input.flush(runner,error));
    assert(elf.read_u32(base+0x800)==0x6000c);
    assert(elf.read_u32(base+0x804)==1000);
    assert(elf.read_u32(base+0x808)==3);
    input.sticks(32767,0,0,0);
    assert(input.pending()>0);
    input.disconnect();
    assert(input.pending()>1);
    assert(input.flush(runner,error));
    assert(input.pending()==0);
    return 0;
}
