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
int main() {
    GuestElf32 image;
    image.segments.push_back({0x10001000u,std::vector<std::uint8_t>(4096,0)});
    GuestLibcBridge svc;
    const auto p=0x10001100u,q=0x10001200u;
    auto* src=image.writeable(p,8);
    assert(src);
    std::memcpy(src,"CORRIDOR",8);
    std::array<std::uint32_t,16> regs{};
    std::string error;
    regs[0]=q;regs[1]=p;regs[2]=8;
    assert(svc.dispatch(1,regs,image,error)); // memcpy
    assert(regs[0]==q&&std::memcmp(image.read(q,8),"CORRIDOR",8)==0);
    regs[0]=q;regs[1]=0x41;regs[2]=2;
    assert(svc.dispatch(3,regs,image,error)); // memset
    assert(image.read(q,2)[0]=='A'&&image.read(q,2)[1]=='A');
    regs[0]=q;regs[1]=p;regs[2]=8;
    assert(svc.dispatch(4,regs,image,error)); // memcmp
    assert(static_cast<std::int32_t>(regs[0])<0);
    // Source and destination overlap, should work as memmove.
    regs[0]=p+1;regs[1]=p;regs[2]=7;
    assert(svc.dispatch(2,regs,image,error));
    assert(std::memcmp(image.read(p+1,7),"CORRIDO",7)==0);
    auto* txt=image.writeable(0x10001300,16);
    std::memcpy(txt,"Ishimura",9);
    regs[0]=0x10001300;
    assert(svc.dispatch(5,regs,image,error)); // strlen
    assert(regs[0]==8);
    std::memcpy(image.writeable(0x10001400,12),"Ishimura",9);
    regs[0]=0x10001300;regs[1]=0x10001400;
    assert(svc.dispatch(6,regs,image,error)); // strcmp
    assert(regs[0]==0);
    auto* second=image.writeable(0x10001400,10);
    second[3]='M';
    regs[0]=0x10001300;regs[1]=0x10001400;regs[2]=3;
    assert(svc.dispatch(7,regs,image,error)); // strncmp
    assert(regs[0]==0);
    regs[0]=0x10001300;regs[1]=0x10001400;regs[2]=8;
    assert(svc.dispatch(7,regs,image,error));
    assert(static_cast<std::int32_t>(regs[0])>0);
    // A guest pointer error must not read arbitrary host memory.
    regs[0]=0xeeeeeeee;regs[1]=p;regs[2]=8;
    assert(!svc.dispatch(1,regs,image,error)&&!error.empty());
    regs[0]=0xeeeeeeee;regs[1]=p;regs[2]=0;
    assert(svc.dispatch(1,regs,image,error)); // no dereference for zero size
    assert(!svc.dispatch(500,regs,image,error));
    std::uint8_t byte=0;
    auto veneer=svc.symbols().at("memcpy");
    assert(svc.code_byte(veneer,byte)&&byte==1);
    assert(svc.code_byte(veneer+3,byte)&&byte==0xef); // SVC #1
    assert(svc.code_byte(veneer+4,byte)&&byte==0x1e); // BX LR
    assert(!svc.code_byte(0x1000,byte));
    assert(!svc.symbols().count("glDrawArrays"));
    assert(!svc.symbols().count("malloc")); // not yet implemented
    return 0;
}
