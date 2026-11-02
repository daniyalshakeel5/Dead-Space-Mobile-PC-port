#include "guest_arm_call_runner.hpp"
#ifdef _WIN32
#include "guest_sdl_host.hpp"
#endif
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
constexpr unsigned kMaxNestedCalls=4;
constexpr u32 kStackStride=0x00100000u;
struct Host final : Dynarmic::A32::UserCallbacks {
    GuestElf32& image;
    const GuestLibcBridge& bridge;
    const GuestJniVm* java=nullptr;
    const GuestSdlHost* sdl=nullptr;
    Dynarmic::A32::Jit* jit=nullptr;
    u64 budget=0;
    u32 trapped=0;
    bool fault=false;
    Host(GuestElf32& elf,const GuestLibcBridge& svc,
         const GuestJniVm* jni,const GuestSdlHost* graphics)
        :image(elf),bridge(svc),java(jni),sdl(graphics){}
    u8 MemoryRead8(u32 p) override {
        u8 value=0;
        if(bridge.code_byte(p,value))return value;
        if(java && java->code_byte(p,value))return value;
#ifdef _WIN32
        if(sdl && sdl->code_byte(p,value))return value;
#endif
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
    GuestFileSystem* files;
    GuestJniVm* java;
    GuestSdlHost* sdl;
    Host host;
    Dynarmic::A32::Jit jit;
    Impl(GuestElf32& elf,GuestLibcBridge& services,GuestHeap& memory,
         GuestFileSystem* mount,GuestJniVm* jni,GuestSdlHost* graphics)
        :image(elf),bridge(services),heap(memory),files(mount),java(jni),
         sdl(graphics),host(elf,services,jni,graphics),
         jit(config(host)){
        // One owned 256 KiB guest stack, disjoint from game code/heap
        // and import veneers, allocated only once for the runner.
        if(image.read(kStack,1)||image.read(kStack+kStackBytes-1,1))
            throw std::runtime_error("ARM guest stack overlaps loaded ELF");
        for(unsigned depth=0;depth<kMaxNestedCalls;++depth){
            const u32 start=kStack+depth*kStackStride;
            if(image.read(start,1)||image.read(start+kStackBytes-1,1))
                throw std::runtime_error("ARM guest callback stack overlaps loaded ELF");
            image.segments.push_back({start,std::vector<u8>(kStackBytes,0)});
        }
        host.jit=&jit;
    }
    bool invoke(u32 address,const std::array<u32,4>& args,
                u32& result,std::string& error,unsigned depth=0,
                const std::vector<u32>& stack_args={}) {
        try{
            if(depth>=kMaxNestedCalls)
                throw std::runtime_error("ARM guest callback nesting limit");
            if(!image.read(address&~1u,2))
                throw std::runtime_error("Original ARM function entry unmapped");
            // Constructors / JNI guest calls must start from a predictable
            // AAPCS context. No recursive JIT Run or fabricated OS state.
            jit.Reset();
            jit.Regs().fill(0);
            jit.SetCpsr(0x10u | ((address&1u)?0x20u:0u));
            for(int i=0;i<4;++i)jit.Regs()[i]=args[i];
            if(stack_args.size()>12u)
                throw std::runtime_error("ARM guest AAPCS stack argument cap");
            const u32 bytes=static_cast<u32>((stack_args.size()*4u+7u)&~7u);
            const u32 sp=kStack+depth*kStackStride+kStackBytes-bytes;
            if(bytes) {
                auto* dst=image.writeable(sp,bytes);
                if(!dst)throw std::runtime_error("Unmapped ARM guest argument stack");
                std::fill_n(dst,bytes,0);
                for(std::size_t index=0;index<stack_args.size();++index)
                    for(unsigned byte=0;byte<4;++byte)
                        dst[index*4u+byte]=u8(stack_args[index]>>(8u*byte));
            }
            jit.Regs()[13]=sp;
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
#ifdef _WIN32
                if(sdl && sdl->handles(id)){
                    std::array<u32,16> gl_regs=jit.Regs();
                    if(!sdl->dispatch(id,gl_regs,image,error))return false;
                    jit.Regs()[0]=gl_regs[0];
                    jit.Regs()[15]=sdl->return_pc(id);
                    continue;
                }
#endif
                if(java && java->handles(id)) {
                    std::array<u32,16> regs=jit.Regs();
                    if(!java->dispatch(id,regs,image,error))
                        return false;
                    const auto callback=java->take_callback();
                    if(callback.address){
                        const auto saved=jit.Regs();
                        const auto cpsr=jit.GetCpsr();
                        u32 ignored=0;
                        if(!invoke(callback.address,callback.arguments,
                                   ignored,error,depth+1))
                            return false;
                        jit.Reset();
                        jit.Regs()=saved;
                        jit.SetCpsr(cpsr);
                    }
                    jit.Regs()[0]=regs[0];
                    jit.Regs()[15]=java->return_pc(id);
                    continue;
                }
                const auto expected=GuestLibcBridge::veneer_base+
                    std::uint64_t(id-1u)*8u;
                if(expected>UINT32_MAX)
                    throw std::runtime_error("Invalid original ARM import SVC address");
                if(id>GuestLibcBridge::last_svc) {
                    const auto missing=bridge.unresolved_import(id);
                    if(!missing.empty())
                        throw std::runtime_error(
                            "Original Xperia ARM invoked unsupported host import: "+missing);
                    throw std::runtime_error("Unknown Android SVC/import trap");
                }
                std::array<u32,16> copy=jit.Regs();
                if(id==GuestLibcBridge::finalize_svc) {
                    // Save the suspended original caller; running an ARM
                    // destructor uses a separate stack and a JIT reset.
                    const auto saved=jit.Regs();
                    const auto cpsr=jit.GetCpsr();
                    const auto callbacks=bridge.take_destructors(saved[0]);
                    for(const auto& callback:callbacks) {
                        u32 ignored=0;
                        if(!invoke(callback.function,
                                   {callback.argument,0,0,0},
                                   ignored,error,depth+1))
                            return false;
                    }
                    jit.Reset();
                    jit.Regs()=saved;
                    jit.SetCpsr(cpsr);
                    copy[0]=0;
                } else if(id==GuestLibcBridge::once_svc) {
                    // Android pthread_once invokes the actual original ARM
                    // function, not a host stub. This runner has one guest
                    // thread; concurrent callers must be rejected explicitly.
                    const u32 control=copy[0],callback=copy[1];
                    auto* word=image.writeable(control,4);
                    if(!word || (control&3u))
                        throw std::runtime_error("Invalid guest pthread_once control");
                    const auto state=image.read_u32(control);
                    if(state==0){
                        if(!image.read(callback&~1u,2))
                            throw std::runtime_error("Unmapped guest pthread_once callback");
                        const auto saved=jit.Regs();
                        const auto cpsr=jit.GetCpsr();
                        word[0]=1;word[1]=word[2]=word[3]=0;
                        u32 ignored=0;
                        if(!invoke(callback,{0,0,0,0},ignored,error,depth+1))
                            return false;
                        word=image.writeable(control,4);
                        if(!word || image.read_u32(control)!=1)
                            throw std::runtime_error("Guest pthread_once control overwritten");
                        word[0]=2;word[1]=word[2]=word[3]=0;
                        jit.Reset();
                        jit.Regs()=saved;
                        jit.SetCpsr(cpsr);
                    }else if(state!=2)
                        throw std::runtime_error("Reentrant pthread_once requires guest scheduler");
                    copy[0]=0;
                } else if(!bridge.dispatch(id,copy,image,error,&heap,files))
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
       GuestLibcBridge& services,GuestHeap& heap,GuestFileSystem* files,GuestJniVm* java,GuestSdlHost* sdl)
    :impl_(std::make_unique<Impl>(image,services,heap,files,java,sdl)){} 
GuestArmCallRunner::~GuestArmCallRunner()=default;
bool GuestArmCallRunner::invoke(u32 address,const std::array<u32,4>& args,
                               u32& result,std::string& error){
    return impl_->invoke(address,args,result,error);
}
bool GuestArmCallRunner::invoke_aapcs(u32 address,
        const std::vector<u32>& arguments,
        u32& result,std::string& error){
    if(arguments.size()>16u){
        error="ARM guest AAPCS argument count exceeds 16";
        return false;
    }
    std::array<u32,4> first{};
    for(std::size_t i=0;i<std::min<std::size_t>(arguments.size(),4u);++i)
        first[i]=arguments[i];
    std::vector<u32> stack;
    if(arguments.size()>4)
        stack.assign(arguments.begin()+4,arguments.end());
    return impl_->invoke(address,first,result,error,0,stack);
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
