#ifdef NDEBUG
#undef NDEBUG
#endif
#include "guest_jni_vm.hpp"
#include <cassert>
#include <cstdint>
#include <string>
using namespace xperia;
int main(){
    GuestElf32 elf;
    GuestJniVm vm;
    std::string error;
    assert(vm.install(elf,error));
    assert(!vm.install(elf,error));
    assert(elf.read_u32(vm.vm())==vm.table_address);
    assert(elf.read_u32(vm.env_address)==vm.env_table_address);
    std::uint8_t byte=0;
    const auto getenv_ptr=elf.read_u32(vm.table_address+6*4);
    assert(vm.code_byte(getenv_ptr,byte) && byte==6);
    assert(vm.code_byte(getenv_ptr+3,byte) && byte==0xef);
    const auto version_ptr=elf.read_u32(vm.env_table_address+4*4);
    assert(version_ptr!=0 && vm.code_byte(version_ptr,byte));
    elf.segments.push_back({0x10001000u,std::vector<std::uint8_t>(4096,0)});
    std::array<std::uint32_t,16> r{};
    r[0]=vm.vm();r[1]=0x10001100;r[2]=0x00010006u;
    assert(vm.dispatch(vm.vm_svc_base+6,r,elf,error));
    assert(r[0]==0 && elf.read_u32(0x10001100)==vm.env_address);
    r[0]=vm.vm();r[1]=0x10001100;r[2]=0x00010009u;
    assert(vm.dispatch(vm.vm_svc_base+6,r,elf,error));
    assert(static_cast<std::int32_t>(r[0])==-3 && elf.read_u32(0x10001100)==0);
    r[0]=vm.env_address;
    assert(vm.dispatch(vm.env_svc_base+4,r,elf,error) && r[0]==0x00010006u);
    r[0]=vm.env_address;
    assert(!vm.dispatch(vm.env_svc_base+6,r,elf,error));
    assert(error.find("6")!=std::string::npos);
    r[0]=0x10001000;
    assert(!vm.dispatch(vm.vm_svc_base+6,r,elf,error));
    return 0;
}
