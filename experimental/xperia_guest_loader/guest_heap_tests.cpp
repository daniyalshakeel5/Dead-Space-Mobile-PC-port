#ifdef NDEBUG
#undef NDEBUG
#endif
#include "guest_heap.hpp"
#include "guest_libc_bridge.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <string>
using namespace xperia;
int main(){
    GuestElf32 elf;
    GuestHeap heap(elf);
    GuestLibcBridge svc;
    std::array<std::uint32_t,16> regs{};
    std::string err;
    assert(svc.symbols().count("malloc")&&svc.symbols().count("realloc"));
    regs[0]=64;
    assert(svc.dispatch(8,regs,elf,err,&heap));
    const auto a=regs[0];
    assert(a>=0x50000000u&&heap.live_bytes()==64);
    assert(elf.writeable(a,64));
    elf.writeable(a,64)[0]=0xab;
    elf.writeable(a,64)[63]=0xcd;
    regs[0]=a;regs[1]=192;
    assert(svc.dispatch(11,regs,elf,err,&heap));
    const auto expanded=regs[0];
    assert(expanded && expanded!=a);
    assert(!elf.read(a,1));
    assert(elf.read(expanded,192)[0]==0xab);
    assert(elf.read(expanded,192)[63]==0xcd);
    regs[0]=4;regs[1]=16;
    assert(svc.dispatch(9,regs,elf,err,&heap));
    const auto zero=regs[0];
    assert(zero && elf.read(zero,64)[63]==0);
    regs[0]=expanded;
    assert(svc.dispatch(10,regs,elf,err,&heap));
    assert(!elf.read(expanded,1));
    regs[0]=expanded;
    assert(!svc.dispatch(10,regs,elf,err,&heap)); // double-free denied
    assert(!err.empty());
    regs[0]=zero;
    assert(svc.dispatch(10,regs,elf,err,&heap));
    assert(heap.live_bytes()==0);
    regs[0]=0;
    assert(svc.dispatch(10,regs,elf,err,&heap)); // free(nullptr)
    regs[0]=UINT32_MAX;regs[1]=UINT32_MAX;
    assert(svc.dispatch(9,regs,elf,err,&heap));
    assert(regs[0]==0); // multiplication overflow returns nullptr
    regs[0]=17u*1024u*1024u;
    assert(svc.dispatch(8,regs,elf,err,&heap));
    assert(regs[0]==0); // per-allocation cap
    regs[0]=5;
    assert(!svc.dispatch(8,regs,elf,err)); // no configured heap
    assert(!err.empty());
    return 0;
}
