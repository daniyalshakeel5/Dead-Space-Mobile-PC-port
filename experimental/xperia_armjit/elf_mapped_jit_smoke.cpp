// Synthetic ELF32 + ARM32 JIT integration (no original game bytes).
#include "guest_elf32.hpp"
#include "dynarmic/interface/A32/a32.h"
#include "dynarmic/interface/A32/config.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
using u8=std::uint8_t;using u16=std::uint16_t;
using u32=std::uint32_t;using u64=std::uint64_t;
static void put16(std::vector<u8>& b,unsigned p,u16 v){
    b.at(p)=u8(v);b.at(p+1)=u8(v>>8);
}
static void put32(std::vector<u8>& b,unsigned p,u32 v){
    put16(b,p,u16(v));put16(b,p+2,u16(v>>16));
}
static std::vector<u8> elf_fixture(){
    std::vector<u8> b(2048,0);
    b[0]=127;b[1]='E';b[2]='L';b[3]='F';b[4]=1;b[5]=1;b[6]=1;
    put16(b,16,3);put16(b,18,40);put32(b,20,1);put32(b,28,52);
    put16(b,40,52);put16(b,42,32);put16(b,44,2);
    put32(b,52,1);put32(b,56,0);put32(b,60,0x1000);
    put32(b,68,2048);put32(b,72,2048);put32(b,76,7);
    put32(b,84,2);put32(b,88,256);put32(b,92,0x1100);
    put32(b,100,48);put32(b,104,48);
    put32(b,256,17);put32(b,260,0x1280);
    put32(b,264,18);put32(b,268,8);
    put32(b,272,19);put32(b,276,8);
    put32(b,280,2);put32(b,284,0);
    put32(b,288,0);
    put32(b,0x280,0x1600);put32(b,0x284,23); // Relative relocated target
    put32(b,0x600,0x00005678); // addend rebases to 0x10005678
    // ARM guest code at vaddr 0x1400, data at 0x1500.
    put32(b,0x400,0xe3a0002a); // mov r0,#42
    put32(b,0x404,0xe2800001); // add r0,r0,#1
    put32(b,0x408,0xe5810000); // str r0,[r1]
    put32(b,0x40c,0xeafffffe); // b .
    return b;
}
class ElfHost final : public Dynarmic::A32::UserCallbacks {
public:
    xperia::GuestElf32& image;
    u64 left=0;
    bool error=false;
    explicit ElfHost(xperia::GuestElf32& elf):image(elf){}
    u8 MemoryRead8(u32 a) override {
        const auto* p=image.read(a,1);
        if(!p){error=true;return 0;}return *p;
    }
    u16 MemoryRead16(u32 a) override {
        return u16(MemoryRead8(a))|u16(MemoryRead8(a+1u))<<8u;
    }
    u32 MemoryRead32(u32 a) override {
        return u32(MemoryRead16(a))|u32(MemoryRead16(a+2u))<<16u;
    }
    u64 MemoryRead64(u32 a) override {
        return u64(MemoryRead32(a))|u64(MemoryRead32(a+4u))<<32u;
    }
    void MemoryWrite8(u32 a,u8 v) override {
        auto* p=image.writeable(a,1);
        if(!p){error=true;return;}*p=v;
    }
    void MemoryWrite16(u32 a,u16 v) override {
        MemoryWrite8(a,u8(v));MemoryWrite8(a+1u,u8(v>>8));
    }
    void MemoryWrite32(u32 a,u32 v) override {
        MemoryWrite16(a,u16(v));MemoryWrite16(a+2u,u16(v>>16));
    }
    void MemoryWrite64(u32 a,u64 v) override {
        MemoryWrite32(a,u32(v));MemoryWrite32(a+4u,u32(v>>32));
    }
    void InterpreterFallback(u32,size_t) override {std::abort();}
    void CallSVC(u32) override {error=true;}
    void ExceptionRaised(u32,Dynarmic::A32::Exception) override {error=true;}
    void AddTicks(u64 n) override {left=n>=left?0:left-n;}
    u64 GetTicksRemaining() override {return left;}
};
int main(){
    std::string why;xperia::GuestElf32 elf;
    if(!elf.open(elf_fixture(),why)){std::cerr<<why<<"\n";return 1;}
    if(elf.relative_relocations!=1 ||
       elf.remaining_relocations!=0 ||
       elf.read_u32(0x1600)!=0x10005678u)return 2;
    ElfHost host(elf);
    Dynarmic::A32::UserConfig cfg;
    cfg.callbacks=&host;
    Dynarmic::A32::Jit jit(cfg);
    jit.Regs()[1]=0x1500;
    jit.Regs()[15]=0x1400;
    jit.SetCpsr(0x10);
    host.left=8;
    jit.Run();
    if(host.error||jit.Regs()[0]!=43||elf.read_u32(0x1500)!=43)
        return 3;
    std::cout<<"Xperia guest ELF sparse PT_LOAD + RELATIVE reloc "
             <<"-> ARM JIT -> guest memory: PASSED\n";
    std::cout<<"Android imports are intentionally unbound; "
               "no EA code executed.\n";
    return 0;
}
