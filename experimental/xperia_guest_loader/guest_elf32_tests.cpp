#ifdef NDEBUG
#undef NDEBUG
#endif
#include "guest_elf32.hpp"
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
using namespace xperia;
static void put16(std::vector<std::uint8_t>& b,unsigned at,std::uint16_t v){
    b.at(at)=std::uint8_t(v);b.at(at+1)=std::uint8_t(v>>8);
}
static void put32(std::vector<std::uint8_t>& b,unsigned at,std::uint32_t v){
    put16(b,at,std::uint16_t(v));put16(b,at+2,std::uint16_t(v>>16));
}
static std::vector<std::uint8_t> fixture(){
    std::vector<std::uint8_t> b(2048,0);
    b[0]=127;b[1]='E';b[2]='L';b[3]='F';b[4]=1;b[5]=1;b[6]=1;
    put16(b,16,3);put16(b,18,40);put32(b,20,1);put32(b,28,52);
    put16(b,40,52);put16(b,42,32);put16(b,44,2);
    // Original virtual memory from 0x1000 through 0x17ff.
    put32(b,52,1);put32(b,56,0);put32(b,60,0x1000);
    put32(b,68,2048);put32(b,72,2048);put32(b,76,7);
    put32(b,84,2);put32(b,88,256);put32(b,92,0x1100);
    put32(b,100,6*8);put32(b,104,6*8);
    put32(b,256,17);put32(b,260,0x1280); // DT_REL
    put32(b,264,18);put32(b,268,16); // DT_RELSZ
    put32(b,272,19);put32(b,276,8); // DT_RELENT
    put32(b,280,2);put32(b,284,0); // DT_PLTRELSZ
    put32(b,288,0);
    // Two REL targets: one relative and one external JUMP_SLOT.
    put32(b,0x280,0x1350);put32(b,0x284,23);
    put32(b,0x288,0x1354);put32(b,0x28c,22);
    put32(b,0x350,0x00001234);
    return b;
}
int main(){
    GuestElf32 image;std::string error;
    auto bytes=fixture();
    assert(image.open(bytes,error));
    assert(error.empty());
    assert(image.relative_relocations==1);
    assert(image.remaining_relocations==1);
    assert(image.read_u32(0x1350)==0x10001234u);
    assert(image.read_u32(0x1354)==0u);
    assert(!image.read(0xffffffff,16));
    assert(!image.writeable(0xffffffff,16));
    bytes[18]=62; // Reject native Windows/x86 ELF.
    assert(!image.open(bytes,error));
    assert(!error.empty());
    assert(image.segments.empty());
    return 0;
}
