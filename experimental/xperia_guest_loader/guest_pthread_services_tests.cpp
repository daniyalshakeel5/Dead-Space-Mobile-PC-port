#ifdef NDEBUG
#undef NDEBUG
#endif
#include "guest_libc_bridge.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
using namespace xperia;
namespace {
constexpr std::uint32_t base=0x10000000u;
void put(GuestElf32& elf,std::uint32_t where,std::uint32_t val){
    auto* ptr=elf.writeable(where,4u);assert(ptr);
    for(unsigned i=0;i<4u;++i)ptr[i]=std::uint8_t(val>>(i*8u));
}
}
int main(){
    GuestElf32 elf;
    elf.segments.push_back({base,std::vector<std::uint8_t>(65536,0)});
    elf.segments.push_back({0x30000000u,std::vector<std::uint8_t>(262144,0)});
    GuestLibcBridge svc;
    std::array<std::uint32_t,16> regs{};
    std::string error;
    const auto call=[&](std::uint32_t id,
            std::uint32_t a=0,std::uint32_t b=0,std::uint32_t c=0)->std::uint32_t{
        regs.fill(0);regs[0]=a;regs[1]=b;regs[2]=c;
        assert(svc.dispatch(id,regs,elf,error));
        return regs[0];
    };
    const char* names[]={
        "pthread_cond_init","pthread_cond_destroy","pthread_cond_signal",
        "pthread_cond_broadcast","pthread_cond_wait","pthread_cond_timedwait",
        "pthread_key_create","pthread_key_delete","pthread_setspecific",
        "pthread_getspecific","pthread_mutexattr_init",
        "pthread_mutexattr_destroy","pthread_mutexattr_settype",
        "pthread_mutexattr_setpshared","pthread_attr_setstack",
        "pthread_attr_getstack","pthread_getattr_np",
        "pthread_attr_setschedparam","pthread_getschedparam",
        "pthread_setschedparam","sem_timedwait"
    };
    for(const auto* name:names)assert(svc.symbols().count(name)==1u);
    constexpr auto cond=base+0x100u, mutex=base+0x104u;
    constexpr auto mattr=base+0x200u,keybuf=base+0x220u;
    assert(call(74,mattr)==0); // mutexattr_init
    assert(call(76,mattr,1u)==0); // recursive
    assert(call(77,mattr,0u)==0); // private
    assert(call(77,mattr,1u)==22u); // no interprocess sharing
    assert(call(33,mutex,mattr)==0); // mutex_init
    assert(call(35,mutex)==0);
    assert(call(35,mutex)==0); // same thread recursive lock
    assert(call(37,mutex)==0);
    assert(call(37,mutex)==0);
    assert(call(37,mutex)==1u); // third unlock EPERM
    assert(call(64,cond)==0);
    std::uint32_t generation=0;
    assert(call(35,mutex)==0);
    assert(svc.begin_cond_wait(cond,mutex,generation,error));
    assert(!svc.cond_signalled(cond,generation));
    assert(call(66,cond)==0);
    assert(svc.cond_signalled(cond,generation));
    assert(svc.end_cond_wait(mutex,error));
    assert(call(37,mutex)==0);
    assert(call(67,cond)==0);
    assert(call(65,cond)==0);
    assert(call(34,mutex)==0);
    assert(call(75,mattr)==0);
    // TLS is keyed by guest pthread_t, never a host thread_local x64 variable.
    assert(call(70,keybuf,0)==0);
    const auto key=elf.read_u32(keybuf);
    assert(key>0);
    assert(call(72,key,0xdeadbeefu)==0);
    assert(call(73,key)==0xdeadbeefu);
    svc.guest_threads().set_self(2);
    assert(call(73,key)==0u);
    assert(call(72,key,0xaabbaabbu)==0);
    assert(call(73,key)==0xaabbaabbu);
    svc.guest_threads().set_self(GuestThreads::main_thread);
    assert(call(73,key)==0xdeadbeefu);
    assert(call(71,key)==0);
    assert(call(73,key)==0); // deleted keys yield null
    assert(call(71,key)==22);
    // ARM32 24-byte attr struct: stack at offset 4 and size at offset 8.
    constexpr auto attr=base+0x300u;
    assert(call(54,attr)==0);
    assert(call(78,attr,0x30000000u,262144u)==0);
    assert(call(79,attr,base+0x340u,base+0x344u)==0);
    assert(elf.read_u32(base+0x340u)==0x30000000u);
    assert(elf.read_u32(base+0x344u)==262144u);
    put(elf,base+0x360u,2u);
    assert(call(81,attr,base+0x360u)==0);
    assert(elf.read_u32(attr+20u)==2u);
    assert(call(55,attr)==0);
    assert(call(80,GuestThreads::main_thread,attr)==0);
    assert(call(82,GuestThreads::main_thread,base+0x380u,base+0x384u)==0);
    assert(elf.read_u32(base+0x380u)==0);
    assert(elf.read_u32(base+0x384u)==0);
    assert(call(83,GuestThreads::main_thread,0,base+0x360u)==0);
    assert(call(83,123456u,0,base+0x360u)==3); // ESRCH
    assert(call(55,attr)==0);
    return 0;
}
