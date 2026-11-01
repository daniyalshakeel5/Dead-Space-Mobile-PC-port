#include "ds_port/original_mode1_resource_factory.hpp"
#include "ds_port/original_resource_handle.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
using namespace ds_port::original_runtime;
namespace {
struct Fixture {
    unsigned calls=0;
    bool refuse=false;
    std::uint32_t next=0;
};
std::uint32_t load32(const std::uint8_t* p) {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
bool resolve(std::uint32_t owner,std::string_view name,void* context,
             std::uint32_t* out) noexcept {
    auto& f=*static_cast<Fixture*>(context);
    ++f.calls;
    assert(owner==0x3000u && name=="fixture");
    if(f.refuse)return false;
    *out=f.next;
    return true;
}
}
int main() {
    std::array<std::uint8_t,16> handle{};
    Fixture f{};
    f.next=0x654321u;
    OriginalMode1FactoryInputs inputs{
        handle.data(),handle.size(),resolve,&f};
    auto result=original_construct_mode1_handle_0x28b3a4(
        inputs,0x3000u,"fixture");
    assert(result.status==
           OriginalMode1FactoryStatus::CompletedOriginalMode1Constructor);
    assert(f.calls==1);
    assert(load32(handle.data())==kOriginalMode1HandleVtable);
    assert(load32(handle.data()+4)==1u);
    assert(load32(handle.data()+8)==0x654321u);
    assert(load32(handle.data()+12)==0x3000u);
    auto ready=original_resource_ready_0x28a5f4(result.handle,nullptr,nullptr);
    assert(ready.status==OriginalResourceHandleStatus::Completed);
    assert(ready.original_r0==1u);
    f.next=0;
    result=original_construct_mode1_handle_0x28b3a4(
        inputs,0x3000u,"fixture");
    assert(result.status==
           OriginalMode1FactoryStatus::CompletedOriginalMode1Constructor);
    ready=original_resource_ready_0x28a5f4(result.handle,nullptr,nullptr);
    assert(ready.status==OriginalResourceHandleStatus::Completed);
    assert(ready.original_r0==0u);
    f.refuse=true;
    result=original_construct_mode1_handle_0x28b3a4(
        inputs,0x3000u,"fixture");
    assert(result.status==OriginalMode1FactoryStatus::OriginalResolverRefused);
    assert(load32(handle.data()+8)==0u);
    inputs.original_0x28a718=nullptr;
    result=original_construct_mode1_handle_0x28b3a4(
        inputs,0x3000u,"fixture");
    assert(result.status==OriginalMode1FactoryStatus::MissingOriginalResolver);
    inputs.original_0x28a718=resolve;
    inputs.handle_bytes=15;
    result=original_construct_mode1_handle_0x28b3a4(
        inputs,0x3000u,"fixture");
    assert(result.status==OriginalMode1FactoryStatus::InvalidMemory);
    return 0;
}
