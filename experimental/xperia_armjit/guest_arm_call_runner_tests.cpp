#ifdef NDEBUG
#undef NDEBUG
#endif
#include "guest_arm_call_runner.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
using namespace xperia;
using u32=std::uint32_t;
static void put32(GuestElf32& elf,u32 address,u32 word){
    auto* ptr=elf.writeable(address,4);
    assert(ptr);
    for(int i=0;i<4;++i)ptr[i]=std::uint8_t(word>>(8*i));
}
int main(){
    constexpr u32 base=0x10001000u;
    GuestElf32 elf;
    elf.segments.push_back({base,std::vector<std::uint8_t>(4096,0)});
    GuestLibcBridge libc;
    GuestHeap heap(elf);
    // AAPCS subroutine:
    // PUSH {LR}; BLX R3 (imported memcpy); POP {PC}.
    put32(elf,base,0xe92d4000u);
    put32(elf,base+4,0xe12fff33u);
    put32(elf,base+8,0xe8bd8000u);
    const char original[]="VANDAL";
    std::memcpy(elf.writeable(base+0x200,6),original,6);
    GuestArmCallRunner runner(elf,libc,heap);
    std::string error;
    u32 result=0;
    assert(runner.invoke(base,{base+0x280,base+0x200,6,
           libc.symbols().at("memcpy")},result,error));
    assert(result==base+0x280);
    assert(std::memcmp(elf.read(base+0x280,6),original,6)==0);
    // Realistic game constructor order:
    // MOV R0,#7 or #9; LDR R1,[PC,#4]; STR R0,[R1]; BX LR.
    for(const auto init:std::array<std::pair<u32,u32>,2>{{
          {base+0x400,7},{base+0x420,9}}}){
        put32(elf,init.first,0xe3a00000u|init.second);
        put32(elf,init.first+4,0xe59f1004u);
        put32(elf,init.first+8,0xe5810000u);
        put32(elf,init.first+12,0xe12fff1eu);
        put32(elf,init.first+16,base+0x700);
    }
    elf.initialization_order={base+0x400,base+0x420};
    assert(runner.run_original_initializers(error));
    assert(elf.read_u32(base+0x700)==9u);
    // Fail closed rather than claim to boot unsupported game services.
    assert(!runner.invoke(0xffffffffu,{0,0,0,0},result,error));
    assert(!error.empty());
    std::cout<<"ARM32 guest function call + libc + ordered constructors: PASS\n";
    return 0;
}
