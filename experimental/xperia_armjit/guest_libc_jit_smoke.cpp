// End-to-end synthetic proof: real A32 BLX -> SVC import trampoline ->
// halted Dynarmic -> native libc host service -> A32 BX LR -> guest caller.
// Absolutely no original EA game machine code or textures are embedded.
#include "guest_elf32.hpp"
#include "guest_libc_bridge.hpp"
#include "dynarmic/interface/A32/a32.h"
#include "dynarmic/interface/A32/config.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
using u8=std::uint8_t;using u16=std::uint16_t;
using u32=std::uint32_t;using u64=std::uint64_t;
struct Host final : Dynarmic::A32::UserCallbacks {
    xperia::GuestElf32& game;
    xperia::GuestLibcBridge& services;
    Dynarmic::A32::Jit* jit=nullptr;
    u64 ticks=0;
    u32 svc_id=0;
    bool bad=false;
    explicit Host(xperia::GuestElf32& g,xperia::GuestLibcBridge& s)
        :game(g),services(s){}
    u8 MemoryRead8(u32 p) override {
        u8 value=0;
        if(services.code_byte(p,value))return value;
        const auto* ptr=game.read(p,1);
        if(!ptr){bad=true;return 0;}return *ptr;
    }
    u16 MemoryRead16(u32 p) override {
        return u16(MemoryRead8(p))|u16(MemoryRead8(p+1))<<8u;
    }
    u32 MemoryRead32(u32 p) override {
        return u32(MemoryRead16(p))|u32(MemoryRead16(p+2))<<16u;
    }
    u64 MemoryRead64(u32 p) override {
        return u64(MemoryRead32(p))|u64(MemoryRead32(p+4))<<32u;
    }
    void MemoryWrite8(u32 p,u8 x) override {
        auto* ptr=game.writeable(p,1);
        if(!ptr){bad=true;return;}*ptr=x;
    }
    void MemoryWrite16(u32 p,u16 x) override {
        MemoryWrite8(p,u8(x));MemoryWrite8(p+1,u8(x>>8));
    }
    void MemoryWrite32(u32 p,u32 x) override {
        MemoryWrite16(p,u16(x));MemoryWrite16(p+2,u16(x>>16));
    }
    void MemoryWrite64(u32 p,u64 x) override {
        MemoryWrite32(p,u32(x));MemoryWrite32(p+4,u32(x>>32));
    }
    void InterpreterFallback(u32,size_t) override {bad=true;if(jit)jit->HaltExecution();}
    void CallSVC(u32 number) override {
        svc_id=number;
        if(jit)jit->HaltExecution();
    }
    void ExceptionRaised(u32,Dynarmic::A32::Exception) override {
        bad=true;if(jit)jit->HaltExecution();
    }
    void AddTicks(u64 n) override {ticks=n>=ticks?0:ticks-n;}
    u64 GetTicksRemaining() override {return ticks;}
};
static void put32(xperia::GuestElf32& image,u32 p,u32 value){
    auto* dst=image.writeable(p,4);
    assert(dst);
    for(int i=0;i<4;i++)dst[i]=u8(value>>(8*i));
}
int main(){
    xperia::GuestElf32 game;
    game.segments.push_back({0x10001000u,std::vector<u8>(4096,0)});
    xperia::GuestLibcBridge services;
    const u32 base=0x10001000u;
    // BLX R3, MOV R4,R0, infinite branch (tick budget stops execution).
    put32(game,base+0,0xe12fff33u);
    put32(game,base+4,0xe1a04000u);
    put32(game,base+8,0xeafffffeu);
    auto* string=game.writeable(base+0x100,8);
    assert(string);
    const char source[8]={'D','E','A','D','S','P','A','C'};
    for(int i=0;i<8;i++)string[i]=u8(source[i]);
    Host host(game,services);
    Dynarmic::A32::UserConfig cfg{};
    cfg.callbacks=&host;
    Dynarmic::A32::Jit jit(cfg);
    host.jit=&jit;
    jit.SetCpsr(0x10);
    jit.Regs()[0]=base+0x200; // guest destination
    jit.Regs()[1]=base+0x100; // guest source
    jit.Regs()[2]=8;
    jit.Regs()[3]=services.symbols().at("memcpy");
    jit.Regs()[15]=base;
    host.ticks=32;
    jit.Run();
    if(host.bad||host.svc_id!=1){
        std::cerr<<"ARM SVC service trampoline was not reached\n";
        return 1;
    }
    auto snapshot=jit.Regs();
    std::string error;
    if(!services.dispatch(host.svc_id,snapshot,game,error)) {
        std::cerr<<"Guest native host call failed: "<<error<<"\n";return 2;
    }
    jit.Regs()[0]=snapshot[0];
    // The SVC callback stopped execution. Resume at actual BX LR
    // instruction so original guest call-return semantics are checked.
    jit.Regs()[15]=services.symbols().at("memcpy")+4;
    host.svc_id=0;host.ticks=16;
    jit.ClearHalt(Dynarmic::HaltReason::UserDefined1);
    jit.Run();
    const auto* copied=game.read(base+0x200,8);
    if(host.bad||!copied||jit.Regs()[4]!=(base+0x200)){
        std::cerr<<"Translated guest did not return from host libc\n";return 3;
    }
    for(int i=0;i<8;i++)if(copied[i]!=u8(source[i]))return 4;
    std::cout<<"A32 BLX->SVC->bounded native memcpy->BX LR->caller: PASS\n";
    return 0;
}
