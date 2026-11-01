#include "ds_port/original_receiver_activation.hpp"
#include "ds_port/original_resource_owner_bind.hpp"
#include "ds_port/original_native_resource_engine.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdint>
using namespace ds_port::original_runtime;
namespace {
void put(std::uint8_t* p,std::uint32_t x) {
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(x>>(i*8));
}
std::uint32_t get(const std::uint8_t* p) {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
struct Fixture {
    std::array<std::uint8_t,0x8fc> owner{};
    std::array<std::uint8_t,0xcd4> scratch{};
    std::array<std::uint8_t,4> allocator{};
    unsigned calls=0;
};
bool mode(std::uint32_t provider,std::uint32_t slot,void* context,
          std::uint32_t* val) noexcept {
    auto& f=*static_cast<Fixture*>(context);
    assert(provider==0x4000u&&slot==0x14u);
    ++f.calls;
    *val=1u;return true;
}
}
int main() {
    using S=OriginalReceiverActivationStatus0x278e58;
    Fixture f{};
    put(f.owner.data()+0x8acu,0x4000u);
    put(f.allocator.data(),0x490000u);
    f.scratch.fill(0xcc);
    OriginalDefaultPoolInputs0x271914 pool{
        f.allocator.data(),f.allocator.size(),nullptr};
    OriginalReceiverActivationScratch0x278e58 stack{
        0x100000u,f.scratch.data(),f.scratch.size(),&pool};
    OriginalReceiverActivationInputs0x278e58 input{
        0x2000u,f.owner.data(),f.owner.size(),nullptr,&f,
        mode,1u,&stack};
    auto result=original_execute_receiver_activation_0x278e58(input);
    assert(result.status==S::ConstructedThreeStringsAwaitingProviderStreams);
    assert(f.calls==1u);
    constexpr std::uint32_t starts[]={0xaa0u,0x888u,0x670u};
    for(const auto offset:starts) {
        const std::uint32_t addr=0x100000u+offset;
        assert(get(f.scratch.data()+offset+0x00u)==addr+0x18u);
        assert(get(f.scratch.data()+offset+0x04u)==addr+0x18u);
        assert(get(f.scratch.data()+offset+0x08u)==addr+0x218u);
        assert(get(f.scratch.data()+offset+0x0cu)==0x490000u);
        assert(get(f.scratch.data()+offset+0x10u)==0u);
        assert(get(f.scratch.data()+offset+0x14u)==addr+0x18u);
        assert(f.scratch[offset+0x18u]==0u);
        assert(f.scratch[offset+0x19u]==0u);
    }
    // The native receiver binder can execute this path but never
    // reports a loaded original resource until +0x2C/+0x28/+0x70
    // are reconstructed.
    f.calls=0;
    OriginalOwnerBindInputs0x278778 bind{
        0x2000u,f.owner.data(),f.owner.size(),nullptr,&f,mode,&stack};
    const auto bound=original_bind_resource_owner_0x278778(bind);
    assert(bound.status==OriginalOwnerBindStatus0x278778::
           UnreconstructedProviderBinding);
    assert(bound.activation_status==
           S::ConstructedThreeStringsAwaitingProviderStreams);
    assert(f.calls==1);
    // Do not read or write outside an undersized original frame.
    f.scratch.fill(0xaa);
    stack.accessible_bytes=0xcd3u;
    result=original_execute_receiver_activation_0x278e58(input);
    assert(result.status==S::OriginalStringConstructionBlocked);
    assert(f.scratch[0xaa0]==0xaa);
    stack.accessible_bytes=f.scratch.size();
    put(f.allocator.data(),0);
    result=original_execute_receiver_activation_0x278e58(input);
    assert(result.status==S::OriginalStringConstructionBlocked);
    // The 0x27541C constructor refuses to invent an allocator.
    assert(get(f.allocator.data())==0u);
    put(f.allocator.data(),0x490000u);
    // Malformed 32-bit stack VA must fail before storing pointers.
    stack.original_stack_arm_va=0xfffffff0u;
    result=original_execute_receiver_activation_0x278e58(input);
    assert(result.status==S::OriginalStringConstructionBlocked);
    return 0;
}
