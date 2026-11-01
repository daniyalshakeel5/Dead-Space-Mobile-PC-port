#include "ds_port/original_embedded_provider.hpp"
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
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(x>>(8u*i));
}
std::uint32_t get(const std::uint8_t* p) {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
}
int main(){
    using S=OriginalEmbeddedProviderStatus;
    std::array<std::uint8_t,0xf8> embedded{};
    put(embedded.data(),kOriginalEmbeddedProviderVtable);
    put(embedded.data()+0x90u,3u);
    auto mode=original_embedded_provider_mode_0x26c3c4(
        embedded.data(),embedded.size());
    assert(mode.status==S::Completed&&mode.original_r0==3);
    mode=original_embedded_provider_mode_0x26c3c4(embedded.data(),0x93);
    assert(mode.status==S::InvalidMappedProvider);
    // Original +0x28 memory-backed branch: a4==1,
    // +4 is non-null and selector 0 returns the cursor at +0xC.
    put(embedded.data()+0xa4u,1u);
    put(embedded.data()+4u,0x50000u);
    put(embedded.data()+0x0cu,37u);
    auto pos=original_embedded_provider_position_0x271794(
        embedded.data(),embedded.size(),0u);
    assert(pos.status==S::Completed&&pos.original_r0==37u);
    pos=original_embedded_provider_position_0x271794(
        embedded.data(),embedded.size(),2u);
    assert(pos.status==S::NeedsOriginalMemoryService);
    put(embedded.data()+4u,0);
    pos=original_embedded_provider_position_0x271794(
        embedded.data(),embedded.size(),0);
    assert(pos.status==S::Completed&&pos.original_r0==0);
    put(embedded.data()+0xa4u,0);
    pos=original_embedded_provider_position_0x271794(
        embedded.data(),embedded.size(),0);
    assert(pos.status==S::NeedsOriginalFileService);
    pos=original_embedded_provider_position_0x271794(
        embedded.data(),embedded.size(),7u);
    assert(pos.status==S::Completed&&pos.original_r0==0);
    pos=original_embedded_provider_position_0x271794(
        embedded.data(),0xa7u,0u);
    assert(pos.status==S::InvalidMappedProvider);
    // The original resource owner contains the same provider at
    // owner+0x804. It must initialize with that actual vtable
    // without a guessed callback at +0x14.
    std::array<std::uint8_t,4> singleton{};
    put(singleton.data(),0x490000u);
    OriginalNativeResourceEngine owner(0x2000u);
    OriginalOwnerConstructorInputs0x27a768 args{};
    args.mapped_allocator_global_word=singleton.data();
    args.mapped_allocator_global_bytes=singleton.size();
    args.original_pool_arm_va=0x490000u;
    args.original_provider_arm_va=owner.original_owner_va()+0x804u;
    const auto built=owner.initialize(args);
    assert(built.status==OriginalOwnerConstructorStatus0x27a768::Completed);
    assert(get(owner.mapped_bytes())==kOriginalOwnerVtable0x27a768);
    const auto* original=owner.mapped_bytes()+0x804u;
    assert(get(original)==kOriginalEmbeddedProviderVtable);
    mode=original_embedded_provider_mode_0x26c3c4(
        original,owner.mapped_size()-0x804u);
    assert(mode.status==S::Completed&&mode.original_r0==0);
    assert(get(owner.mapped_bytes()+0x8acu)==0x2804u);
    // The original activation can now read the provider mode
    // DIRECTLY from mapped native memory without a callback.
    auto active=owner.bind_original_owner_0x278778();
    assert(active.status==
           OriginalOwnerBindStatus0x278778::UnreconstructedProviderBinding);
    assert(active.activation_status==
           OriginalReceiverActivationStatus0x278e58::UnreconstructedOwnerApproval);
    // Original 0x26C3C4 directly reads embedded+0x90. When a
    // real mode-1 state exists it reaches native string construction.
    std::array<std::uint8_t,kOriginalOwnerObjectBytes> state{};
    put(state.data()+0x8acu,0x2804u);
    put(state.data()+0x804u+0x90u,1u);
    OriginalReceiverActivationInputs0x278e58 init{
        0x2000u,state.data(),state.size(),nullptr,nullptr,nullptr,1u};
    auto outcome=original_execute_receiver_activation_0x278e58(init);
    assert(outcome.status==OriginalReceiverActivationStatus0x278e58::
           ReachedUnreconstructedStreamInitialization);
    return 0;
}
