#include "ds_port/original_receiver_activation.hpp"
#include "ds_port/original_receiver_prefix.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdint>

using namespace ds_port::original_runtime;
namespace {
void put(std::uint8_t* p,std::uint32_t x) {
    for(unsigned k=0;k<4;++k)p[k]=std::uint8_t(x>>(k*8));
}
std::uint32_t get(const std::uint8_t* p) {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
struct Fixture {
    std::array<std::uint8_t,0x8d4> owner{};
    unsigned release_calls=0;
    bool deny_release=false;
};
bool release(std::uint32_t tree,std::uint32_t root,void* context) noexcept {
    auto& f=*static_cast<Fixture*>(context);
    ++f.release_calls;
    assert(tree==0x28bcu&&root==0x5000u);
    return !f.deny_release;
}
}
int main() {
    using S=OriginalReceiverActivationStatus0x278e58;
    Fixture f{};
    auto in=OriginalReceiverActivationInputs0x278e58{
        0x2000u,f.owner.data(),f.owner.size(),nullptr,&f};
    f.owner[0x8b6u]=1u;
    f.owner[0x8ccu]=1u;
    put(f.owner.data()+0x8d0u,0x1234u);
    put(f.owner.data()+0x8c8u,0u); // original no tree
    auto out=original_execute_receiver_activation_0x278e58(in);
    assert(out.status==S::CompletedEmptyProviderReset);
    assert(out.previous_tree_root==0&&out.original_provider_arm_va==0);
    assert(get(f.owner.data()+0x8c0u)==0x28c0u);
    assert(get(f.owner.data()+0x8c4u)==0x28c0u);
    assert(get(f.owner.data()+0x8c8u)==0);
    assert(f.owner[0x8b6u]==0);
    assert(f.owner[0x8ccu]==0);
    assert(get(f.owner.data()+0x8d0u)==0);
    // Existing tree: original 0x273570 cleanup is required; refuse
    // loss of original allocations when that service is unavailable.
    put(f.owner.data()+0x8c8u,0x5000u);
    f.owner[0x8b6u]=1;
    const auto previous_sentinel=get(f.owner.data()+0x8c4u);
    out=original_execute_receiver_activation_0x278e58(in);
    assert(out.status==S::MissingOriginalTreeDestructor);
    assert(get(f.owner.data()+0x8c8u)==0x5000u);
    assert(f.owner[0x8b6u]==1);
    assert(get(f.owner.data()+0x8c4u)==previous_sentinel);
    in.release_existing_tree=release;
    f.deny_release=true;
    out=original_execute_receiver_activation_0x278e58(in);
    assert(out.status==S::OriginalTreeReleaseRefused);
    assert(f.release_calls==1&&get(f.owner.data()+0x8c8u)==0x5000u);
    f.deny_release=false;
    out=original_execute_receiver_activation_0x278e58(in);
    assert(out.status==S::CompletedEmptyProviderReset);
    assert(f.release_calls==2&&get(f.owner.data()+0x8c8u)==0);
    // Nonzero original provider at owner+0x8AC must not be replaced
    // by a fake successful activation.
    put(f.owner.data()+0x8acu,0x6000u);
    f.owner[0x8b6u]=1;
    out=original_execute_receiver_activation_0x278e58(in);
    assert(out.status==S::UnreconstructedProviderActivation);
    assert(out.original_provider_arm_va==0x6000u);
    assert(f.owner[0x8b6u]==0);
    in.owner_bytes=0x8d3u;
    out=original_execute_receiver_activation_0x278e58(in);
    assert(out.status==S::InvalidOwner);
    // Native 0x278018's alternate branch calls the original
    // owner's +0x64. It can execute the mapped state reset, but
    // must report unfinished owner activation rather than success.
    in.owner_bytes=f.owner.size();
    put(f.owner.data()+0x8acu,0);
    f.owner[0x8b6u]=0;
    OriginalReceiverPrefixInputs0x278018 receiver{};
    receiver.original_owner_arm_va=in.owner_arm_va;
    receiver.mapped_owner=f.owner.data();
    receiver.owner_accessible_bytes=f.owner.size();
    receiver.native_activation=&in;
    auto prefix=original_execute_receiver_prefix_0x278018(receiver,u"test");
    assert(prefix.status==
           OriginalReceiverPrefixStatus0x278018::OriginalActivationBlocked);
    assert(f.owner[0x8b6u]==0);
    assert(get(f.owner.data()+0x8c0u)==0x28c0u);
    return 0;
}
