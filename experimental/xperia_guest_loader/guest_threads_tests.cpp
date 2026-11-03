#ifdef NDEBUG
#undef NDEBUG
#endif
#include "guest_libc_bridge.hpp"
#include "guest_threads.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
using namespace xperia;
namespace {
void word(GuestElf32& elf,std::uint32_t addr,std::uint32_t v){
    auto* p=elf.writeable(addr,4);
    assert(p);
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(v>>(8u*i));
}
}
int main(){
    constexpr std::uint32_t base=0x10001000u;
    GuestElf32 elf;
    elf.segments.push_back({base,std::vector<std::uint8_t>(4096,0)});
    word(elf,base+0x100,0xe12fff1eu); // original-free BX LR worker
    GuestLibcBridge svc;
    std::array<std::uint32_t,16> r{};
    std::string error;
    const auto dispatch=[&](std::uint32_t svc_id){
        return svc.dispatch(svc_id,r,elf,error);
    };
    const auto attr=base+0x300;
    r[0]=attr;
    assert(dispatch(54) && r[0]==0);
    assert(elf.read_u32(attr+8u)==GuestThreads::default_stack);
    assert(elf.read_u32(attr+12u)==GuestThreads::default_guard);
    r.fill(0);r[0]=attr;r[1]=1;
    assert(dispatch(56)&&r[0]==0); // detached
    r.fill(0);r[0]=attr;r[1]=base+0x350;
    assert(dispatch(57)&&r[0]==0);
    assert(elf.read_u32(base+0x350)==1);
    r.fill(0);r[0]=base+0x200;r[1]=attr;r[2]=base+0x100;r[3]=0x9090u;
    assert(dispatch(49)&&r[0]==0);
    const auto detached=elf.read_u32(base+0x200);
    assert(svc.guest_threads().pending()==1);
    GuestThreads::Task task;
    assert(svc.guest_threads().take_next(task));
    assert(task.id==detached && task.entry==base+0x100 &&
           task.arg==0x9090u && task.detached);
    assert(svc.guest_threads().complete(task.id,123u));
    assert(svc.guest_threads().live()==0); // detached reaped
    r.fill(0);r[0]=detached;
    assert(dispatch(50)&&r[0]==3); // ESRCH
    r.fill(0);r[0]=attr;r[1]=0;
    assert(dispatch(56)&&r[0]==0); // joinable
    r.fill(0);r[0]=base+0x200;r[1]=attr;r[2]=base+0x100;r[3]=5;
    assert(dispatch(49)&&r[0]==0);
    const auto joinable=elf.read_u32(base+0x200);
    r.fill(0);r[0]=joinable;r[1]=base+0x400;
    assert(dispatch(50)&&r[0]==16); // not yet executed
    assert(svc.guest_threads().take_next(task));
    assert(task.id==joinable && task.arg==5 && !task.detached);
    svc.guest_threads().set_self(task.id);
    r.fill(0);
    assert(dispatch(52)&&r[0]==joinable);
    r.fill(0);r[0]=joinable;r[1]=base+0x400;
    assert(dispatch(50)&&r[0]==35); // EDEADLK self join
    svc.guest_threads().set_self(GuestThreads::main_thread);
    assert(svc.guest_threads().complete(joinable,0xfaceu));
    r.fill(0);r[0]=joinable;r[1]=base+0x400;
    assert(dispatch(50)&&r[0]==0);
    assert(elf.read_u32(base+0x400)==0xfaceu);
    assert(svc.guest_threads().live()==0);
    r.fill(0);r[0]=attr;r[1]=8192u;
    assert(dispatch(58)&&r[0]==22u); // below minimum ARM stack
    r.fill(0);r[0]=attr;r[1]=262144u;
    assert(dispatch(58)&&r[0]==0);
    r.fill(0);r[0]=attr;r[1]=base+0x440;
    assert(dispatch(59)&&r[0]==0);
    assert(elf.read_u32(base+0x440)==262144u);
    r.fill(0);r[0]=attr;
    assert(dispatch(55)&&r[0]==0);
    r.fill(0);r[0]=base+0x200;r[1]=0;r[2]=base+0x100;r[3]=0;
    assert(dispatch(49)&&r[0]==0);
    assert(svc.guest_threads().pending()==1);
    assert(svc.guest_threads().take_next(task) && !task.detached);
    assert(svc.guest_threads().detach(task.id)==0);
    assert(svc.guest_threads().complete(task.id,99u));
    assert(svc.guest_threads().live()==0);
    // Unknown ARM entry points cannot be scheduled as fake threads.
    r.fill(0);r[0]=base+0x200;r[2]=0x40000000u;
    assert(!dispatch(49));
    return 0;
}
