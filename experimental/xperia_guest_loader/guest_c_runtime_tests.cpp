#ifdef NDEBUG
#undef NDEBUG
#endif
#include "guest_libc_bridge.hpp"
#include "guest_heap.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
using namespace xperia;
namespace {
constexpr std::uint32_t base=0x10000000u;
void word(GuestElf32& elf,std::uint32_t pos,std::uint32_t value){
    auto* bytes=elf.writeable(pos,4u);assert(bytes);
    for(unsigned i=0;i<4u;++i)bytes[i]=std::uint8_t(value>>(i*8u));
}
void str(GuestElf32& elf,std::uint32_t pos,const char* data){
    const auto n=std::strlen(data)+1u;
    auto* dst=elf.writeable(pos,n);assert(dst);std::memcpy(dst,data,n);
}
std::string value(GuestElf32& elf,std::uint32_t pos){
    const auto* p=elf.read(pos,1);assert(p);return reinterpret_cast<const char*>(p);
}
}
int main(){
    GuestElf32 elf;
    elf.segments.push_back({base,std::vector<std::uint8_t>(65536,0)});
    GuestHeap heap(elf);
    GuestLibcBridge libc;
    std::array<std::uint32_t,16> r{};
    std::string error;
    const auto call=[&](unsigned id,std::uint32_t a=0,
                        std::uint32_t b=0,std::uint32_t c=0)->std::uint32_t{
        r.fill(0);r[0]=a;r[1]=b;r[2]=c;
        assert(libc.dispatch(id,r,elf,error,&heap));
        return r[0];
    };
    str(elf,base+0x100,"VANDAL");
    assert(call(85,base+0x200,base+0x100)==base+0x200);
    assert(value(elf,base+0x200)=="VANDAL");
    assert(call(86,base+0x240,base+0x100,4u)==base+0x240);
    assert(std::memcmp(elf.read(base+0x240,4u),"VAND",4u)==0);
    assert(call(87,base+0x200,base+0x100)==base+0x200);
    assert(value(elf,base+0x200)=="VANDALVANDAL");
    assert(call(88,base+0x100,'N')==base+0x102);
    assert(call(89,base+0x100,'A')==base+0x104);
    assert(call(88,base+0x100,0)==base+0x106);
    str(elf,base+0x300,"DAL");
    assert(call(90,base+0x100,base+0x300)==base+0x103);
    assert(call(90,base+0x100,base+0x100)==base+0x100);
    const auto dup=call(91,base+0x100);
    assert(dup && value(elf,dup)=="VANDAL");
    str(elf,base+0x340,"-123tail");
    assert(static_cast<std::int32_t>(call(92,base+0x340))==-123);
    assert(static_cast<std::int32_t>(call(93,base+0x340,base+0x380,10u))==-123);
    assert(elf.read_u32(base+0x380)==base+0x344);
    str(elf,base+0x390,"ffnext");
    assert(call(94,base+0x390,base+0x3a0,16u)==255u);
    assert(elf.read_u32(base+0x3a0)==base+0x392);
    const auto sp=base+0xf000u;
    str(elf,base+0x400,"%s:%08x/%d");
    r.fill(0);r[0]=base+0x500;r[1]=64;r[2]=base+0x400;
    r[3]=base+0x100;r[13]=sp;
    word(elf,sp,0x12);word(elf,sp+4u,static_cast<std::uint32_t>(-9));
    assert(libc.dispatch(95,r,elf,error,&heap));
    assert(value(elf,base+0x500)=="VANDAL:00000012/-9");
    assert(r[0]==std::strlen("VANDAL:00000012/-9"));
    str(elf,base+0x450,"%lld");
    const std::int64_t ll=-1234567890123LL;
    const auto llbits=static_cast<std::uint64_t>(ll);
    word(elf,sp,static_cast<std::uint32_t>(llbits));
    word(elf,sp+4u,static_cast<std::uint32_t>(llbits>>32u));
    r.fill(0);r[0]=base+0x500;r[1]=64;r[2]=base+0x450;
    r[13]=sp;
    assert(libc.dispatch(95,r,elf,error,&heap));
    assert(value(elf,base+0x500)=="-1234567890123");
    str(elf,base+0x470,"%.2f");
    const double d=1.5;
    std::uint64_t bits=0;std::memcpy(&bits,&d,8u);
    word(elf,sp,static_cast<std::uint32_t>(bits));
    word(elf,sp+4u,static_cast<std::uint32_t>(bits>>32u));
    r.fill(0);r[0]=base+0x500;r[1]=64;r[2]=base+0x470;r[13]=sp;
    assert(libc.dispatch(95,r,elf,error,&heap));
    assert(value(elf,base+0x500)=="1.50");
    str(elf,base+0x490,"%u/%s");
    word(elf,base+0x600,42u);
    word(elf,base+0x604,base+0x100);
    r.fill(0);r[0]=base+0x500;r[1]=64;r[2]=base+0x490;r[3]=base+0x600;
    assert(libc.dispatch(97,r,elf,error,&heap));
    assert(value(elf,base+0x500)=="42/VANDAL");
    // vsprintf takes va_list as its third argument (r2), not r3.
    r.fill(0);r[0]=base+0x500;r[1]=base+0x490;r[2]=base+0x600;
    assert(libc.dispatch(98,r,elf,error,&heap));
    assert(value(elf,base+0x500)=="42/VANDAL");
    str(elf,base+0x4b0,"%n");
    r.fill(0);r[0]=base+0x500;r[1]=64;r[2]=base+0x4b0;r[3]=base+0x200;
    assert(!libc.dispatch(95,r,elf,error,&heap));
    assert(error.find("%n")!=std::string::npos);
    str(elf,base+0x4d0,"TAG");
    r.fill(0);r[0]=3;r[1]=base+0x4d0;r[2]=base+0x490;
    r[3]=42;r[13]=sp;
    word(elf,sp,base+0x100);
    assert(libc.dispatch(99,r,elf,error,&heap));
    assert(r[0]==9u);
    r.fill(0);r[0]=3;r[1]=base+0x4d0;r[2]=base+0x490;r[3]=base+0x600;
    assert(libc.dispatch(100,r,elf,error,&heap));
    assert(r[0]==9u);
    str(elf,base+0x700,"HOME");
    assert(call(101,base+0x700)==0u); // host filesystem never leaked
    return 0;
}
