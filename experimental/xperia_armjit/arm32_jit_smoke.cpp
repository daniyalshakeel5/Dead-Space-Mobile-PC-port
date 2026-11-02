// Synthetic ARM instruction translation proof, not the EA game executable.
// Uses Dynarmic (0BSD).  Only fixed, original-free instructions are executed.
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include "dynarmic/interface/A32/a32.h"
#include "dynarmic/interface/A32/config.h"
using u8=std::uint8_t;
using u16=std::uint16_t;
using u32=std::uint32_t;
using u64=std::uint64_t;
class GuestMemory final : public Dynarmic::A32::UserCallbacks {
public:
    std::array<u8,4096> mem{};
    u64 ticks=0;
    bool invalid=false;
    u8 MemoryRead8(u32 a) override {
        if(a>=mem.size()){invalid=true;return 0;}
        return mem[a];
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
    void MemoryWrite8(u32 a,u8 value) override {
        if(a>=mem.size()){invalid=true;return;}
        mem[a]=value;
    }
    void MemoryWrite16(u32 a,u16 value) override {
        MemoryWrite8(a,u8(value));MemoryWrite8(a+1u,u8(value>>8u));
    }
    void MemoryWrite32(u32 a,u32 value) override {
        MemoryWrite16(a,u16(value));MemoryWrite16(a+2u,u16(value>>16u));
    }
    void MemoryWrite64(u32 a,u64 value) override {
        MemoryWrite32(a,u32(value));MemoryWrite32(a+4u,u32(value>>32u));
    }
    void InterpreterFallback(u32,size_t) override {std::abort();}
    void CallSVC(u32) override {invalid=true;}
    void ExceptionRaised(u32,Dynarmic::A32::Exception) override {invalid=true;}
    void AddTicks(u64 n) override {ticks=n>=ticks?0:ticks-n;}
    u64 GetTicksRemaining() override {return ticks;}
};
int main(){
    GuestMemory mem;
    // Real ARM A32 program:
    // MOV R0,#42; ADD R0,R0,#1; STR R0,[R1]; B .
    mem.MemoryWrite32(0,0xe3a0002a);
    mem.MemoryWrite32(4,0xe2800001);
    mem.MemoryWrite32(8,0xe5810000);
    mem.MemoryWrite32(12,0xeafffffe);
    Dynarmic::A32::UserConfig cfg;
    cfg.callbacks=&mem;
    Dynarmic::A32::Jit jit(cfg);
    jit.Regs()[1]=1024;
    jit.Regs()[15]=0;
    jit.SetCpsr(0x10); // User-mode A32 (not Thumb).
    mem.ticks=6;
    jit.Run();
    if(mem.invalid||jit.Regs()[0]!=43||mem.MemoryRead32(1024)!=43){
        std::cerr<<"ARM32 JIT synthetic regression FAILED\n";
        return 1;
    }
    std::cout<<"ARM32 JIT x86-64 translation passed: result="
             <<mem.MemoryRead32(1024)<<"\n";
    std::cout<<"Game startup is unavailable: ELF relocation, ARM32 imports, "
                "guest JNI and GLES have not been bound yet.\n";
    return 0;
}
