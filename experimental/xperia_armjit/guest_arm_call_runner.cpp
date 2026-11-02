#include "guest_arm_call_runner.hpp"
#include "dynarmic/interface/A32/a32.h"
#include "dynarmic/interface/A32/config.h"
#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace xperia {
namespace {
using u8=std::uint8_t; using u16=std::uint16_t;
using u32=std::uint32_t; using u64=std::uint64_t;
constexpr u32 kStack=0x60000000u;
constexpr u32 kStackBytes=256u*1024u;
constexpr u32 kReturnTrap=0x71000000u;
constexpr u32 kReturnSvc=0xff00u;
constexpr unsigned kMaxTraps=256;
struct Host final : Dynarmic::A32::UserCallbacks {
    GuestElf32& image;
    const GuestLibcBridge& bridge;
    Dynarmic::A32::Jit* jit=nullptr;
    u64 budget=0;
    u32 trapped=0;
    bool fault=false;
    Host(GuestElf32& elf,const GuestLibcBridge& svc):image(elf),bridge(svc){}
    u8 MemoryRead8(u32 p) override {
        u8 value=0;
        if(bridge.code_byte(p,value))return value;
        if(p>=kReturnTrap && p<kReturnTrap+4u){
            const u32 word=0xef000000u|kReturnSvc;
            return u8(word>>(8u*(p-kReturnTrap)));
        }
        const auto* data=image.read(p,1);
        if(!data){fault=true;return 0;}return *data;
    }
    u16 MemoryRead16(u32 p) override {
        return u16(MemoryRead8(p))|u16(MemoryRead8(p+1u))<<8u;
    }
    u32 MemoryRead32(u32 p) override {
        return u32(MemoryRead16(p))|u32(MemoryRead16(p+2u))<<16u;
    }
    u64 MemoryRead64(u32 p) override {
        return u64(MemoryRead32(p))|u64(MemoryRead32(p+4u))<<32u;
    }
    void MemoryWrite8(u32 p,u8 value) override {
        auto* dst=image.writeable(p,1);
        if(!dst){fault=true;return;}*dst=value;
    }
    void MemoryWrite16(u32 p,u16 value) override {
        MemoryWrite8(p,u8(value));MemoryWrite8(p+1u,u8(value>>8u));
    }
    void MemoryWrite32(u32 p,u32 value) override {
        MemoryWrite16(p,u16(value));MemoryWrite16(p+2u,u16(value>>16u));
    }
    void MemoryWrite64(u32 p,u64 value) override {
        MemoryWrite32(p,u32(value));MemoryWrite32(p+4u,u32(value>>32u));
    }
    void InterpreterFallback(u32,size_t) override {
        fault=true;if(jit)jit->HaltExecution();
    }
    void CallSVC(u32 immediate) override {
        trapped=immediate;if(jit)jit->HaltExecution();
    }
    void ExceptionRaised(u32,Dynarmic::A32::Exception) override {
        fault=true;if(jit)jit->HaltExecution();
    }
    void AddTicks(u64 n) override {budget=n>=budget?0:budget-n;}
    u64 GetTicksRemaining() override {return budget;}
};
Dynarmic::A32::UserConfig config(Host& host){
    Dynarmic::A32::UserConfig cfg{};
    cfg.callbacks=&host;
    return cfg;
}
}
struct GuestArmCallRunner::Impl {
    GuestElf32& image;
    GuestLibcBridge& bridge;
    GuestHeap& heap;
    Host host;
    Dynarmic::A32::Jit jit;
    Impl(GuestElf32& elf,GuestLibcBridge& services,GuestHeap& memory)
        :image(elf),bridge(services),heap(memory),host(elf,services),
         jit(config(host)){
        // One owned 256 KiB guest stack, disjoint from game code/heap
        // and import veneers, allocated only once for the runner.
        if(image.read(kStack,1)||image.read(kStack+kStackBytes-1,1))
            throw std::runtime_error("ARM guest stack overlaps loaded ELF");
        image.segments.push_back({kStack,std::vector<u8>(kStackBytes,0)});
        host.jit=&jit;
    }
    bool invoke(u32 address,const std::array<u32,4>& args,
                u32& result,std::string& error) {
        try{
            if(!image.read(address&~1u,2))
                throw std::runtime_error("Original ARM function entry unmapped");
            // Constructors / JNI guest calls must start from a predictable
            // AAPCS context. No recursive JIT Run or fabricated OS state.
            jit.Reset();
            jit.Regs().fill(0);
            jit.SetCpsr(0x10u | ((address&1u)?0x20u:0u));
            for(int i=0;i<4;++i)jit.Regs()[i]=args[i];
            jit.Regs()[13]=kStack+kStackBytes; // 8/16 aligned
            jit.Regs()[14]=kReturnTrap;
            jit.Regs()[15]=address&~1u;
            for(unsigned i=0;i<kMaxTraps;++i){
                host.fault=false;
                host.trapped=0;
                host.budget=200000u;
                jit.ClearHalt(Dynarmic::HaltReason::UserDefined1);
                jit.Run();
                if(host.fault)
                    throw std::runtime_error("ARM guest instruction/memory fault");
                const auto id=host.trapped;
                if(id==kReturnSvc) {
                    result=jit.Regs()[0];
                    error.clear();return true;
                }
                if(id==0)
                    throw std::runtime_error("ARM guest exceeded bounded instruction budget");
                const auto expected=GuestLibcBridge::veneer_base+
                    std::uint64_t(id-1u)*8u;
                if(expected>UINT32_MAX || id>11)
                    throw std::runtime_error("Unsupported Android SVC/import");
                std::array<u32,16> copy=jit.Regs();
                if(!bridge.dispatch(id,copy,image,error,&heap))
                    return false;
                jit.Regs()[0]=copy[0];
                jit.Regs()[15]=static_cast<u32>(expected)+4u; // BX LR
            }
            throw std::runtime_error("ARM guest exceeded host service-call limit");
        }catch(const std::exception& ex){
            error=ex.what();return false;
        }
    }
};
GuestArmCallRunner::GuestArmCallRunner(GuestElf32& image,
       GuestLibcBridge& services,GuestHeap& heap)
    :impl_(std::make_unique<Impl>(image,services,heap)){}
GuestArmCallRunner::~GuestArmCallRunner()=default;
bool GuestArmCallRunner::invoke(u32 address,const std::array<u32,4>& args,
                               u32& result,std::string& error){
    return impl_->invoke(address,args,result,error);
}
bool GuestArmCallRunner::run_original_initializers(std::string& error){
    // Original ELF parser returned ordered DT_INIT and DT_INIT_ARRAY,
    // already relocated to the exact guest ARM32 address space.
    for(const auto address:impl_->image.initialization_order){
        u32 ignored=0;
        if(!impl_->invoke(address,{0,0,0,0},ignored,error))
            return false;
    }
    error.clear();return true;
}
} // namespace xperia
