#ifdef NDEBUG
#undef NDEBUG
#endif
#include "guest_elf32.hpp"
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
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
    put32(b,100,11*8);put32(b,104,11*8);
    put32(b,256,17);put32(b,260,0x1280); // DT_REL
    put32(b,264,18);put32(b,268,16); // DT_RELSZ
    put32(b,272,19);put32(b,276,8); // DT_RELENT
    put32(b,280,2);put32(b,284,0); // DT_PLTRELSZ
    put32(b,288,4);put32(b,292,0x1200); // DT_HASH
    put32(b,296,5);put32(b,300,0x1240); // DT_STRTAB
    put32(b,304,6);put32(b,308,0x1220); // DT_SYMTAB
    put32(b,312,10);put32(b,316,32); // DT_STRSZ
    put32(b,320,11);put32(b,324,16); // DT_SYMENT
    put32(b,328,0);
    // SysV hash reports two ELF dynsym entries.
    put32(b,0x200,1);put32(b,0x204,2);
    put32(b,0x230,1); // dynsym[1].st_name
    b[0x23c]=0x12; // defined? no; global function symbol
    const char symbol[]="guest_android_fn";
    b[0x240]=0;
    for(std::size_t i=0;i<sizeof(symbol);++i)
        b[0x241+i]=static_cast<std::uint8_t>(symbol[i]);
    // Two REL targets: one relative and one external JUMP_SLOT.
    put32(b,0x280,0x1350);put32(b,0x284,23);
    put32(b,0x288,0x1354);put32(b,0x28c,22|(1u<<8));
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
    assert(image.read_u32(0x10001350)==0x10001234u);
    assert(image.read_u32(0x10001354)==0u);
    assert(image.pending_imports.size()==1u);
    assert(image.pending_imports[0].name=="guest_android_fn");
    assert(image.pending_imports[0].symbol_type==2);
    // The linker must fail atomically until the matching guest thunk
    // address is supplied; no external function is ever invoked.
    assert(!image.bind_imports({},error));
    assert(!error.empty()&&image.read_u32(0x10001354)==0u);
    const std::unordered_map<std::string,std::uint32_t> services{
        {"guest_android_fn",0x30001000u}};
    assert(image.bind_imports(services,error));
    assert(error.empty()&&image.remaining_relocations==0);
    assert(image.read_u32(0x10001354)==0x30001000u);
    // Old incorrectly unrebased guest virtual addresses MUST NOT work.
    assert(image.read(0x1350,4)==nullptr);
    assert(image.writeable(0x1350,4)==nullptr);
    assert(!image.read(0xffffffff,16));
    assert(!image.writeable(0xffffffff,16));
    // Synthetic original ARM startup: DT_INIT followed by two
    // relocated DT_INIT_ARRAY entries and a real named JNI export.
    auto startup=fixture();
    put32(startup,100,13*8);put32(startup,104,13*8);
    put32(startup,268,32); // four ARM REL records
    put32(startup,0x290,0x1360);put32(startup,0x294,23);
    put32(startup,0x298,0x1364);put32(startup,0x29c,23);
    put32(startup,0x360,0x1400);put32(startup,0x364,0x1411);
    put32(startup,328,25);put32(startup,332,0x1360); // DT_INIT_ARRAY
    put32(startup,336,27);put32(startup,340,8); // DT_INIT_ARRAYSZ
    put32(startup,344,12);put32(startup,348,0x1420); // DT_INIT
    put32(startup,352,0);
    const char jni[]="JNI_OnLoad";
    for(std::size_t i=0;i<sizeof(jni);++i)
        startup[0x241+i]=std::uint8_t(jni[i]);
    put32(startup,0x234,0x1400); // original export st_value
    put16(startup,0x23e,1); // now a defined symbol
    GuestElf32 initialized;
    assert(initialized.open(startup,error));
    assert(initialized.initialization_order.size()==3);
    assert(initialized.initialization_order[0]==0x10001420u);
    assert(initialized.initialization_order[1]==0x10001400u);
    assert(initialized.initialization_order[2]==0x10001411u);
    assert(initialized.jni_exports.at("JNI_OnLoad")==0x10001400u);
    // Incomplete init-array metadata must fail closed.
    put32(startup,340,3);
    assert(!initialized.open(startup,error));
    assert(initialized.initialization_order.empty());
    assert(initialized.jni_exports.empty());
    bytes[18]=62; // Reject native Windows/x86 ELF.
    assert(!image.open(bytes,error));
    assert(!error.empty());
    assert(image.segments.empty());
    return 0;
}
