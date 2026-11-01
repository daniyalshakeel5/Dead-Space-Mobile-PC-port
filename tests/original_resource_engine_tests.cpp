#include "ds_port/original_native_resource_engine.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdint>

using namespace ds_port::original_runtime;
namespace {
std::uint32_t word(const std::uint8_t* p) {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
void put(std::uint8_t* p,std::uint32_t x) {
    for(unsigned n=0;n<4;++n)p[n]=std::uint8_t(x>>(8*n));
}
struct Fixture {
    std::array<std::uint8_t,4> allocator_slot{};
    unsigned default_pool_calls=0;
    unsigned provider_calls=0;
    bool refuse_pool=false;
    bool refuse_provider=false;
};
bool pool(std::uint32_t fn,void* context,std::uint32_t* out) noexcept {
    auto& f=*static_cast<Fixture*>(context);
    ++f.default_pool_calls;
    assert(fn==0x00271914u);
    if(f.refuse_pool)return false;
    *out=0x00037000u;
    return true;
}
bool provider(std::uint32_t token,std::uint32_t slot,
              void* context,std::uint32_t* out) noexcept {
    auto& f=*static_cast<Fixture*>(context);
    ++f.provider_calls;
    assert(token==0x00051000u&&slot==0x14u);
    if(f.refuse_provider)return false;
    *out=2u; // original owner+0x8B4 is (r0>>1)&1
    return true;
}
bool reset(std::uint32_t owner,void* context) noexcept {
    assert(owner==0x00002000u&&context!=nullptr);
    return true;
}
}
int main() {
    using S=OriginalOwnerConstructorStatus0x27a768;
    Fixture f{};
    put(f.allocator_slot.data(),0x0049c544u);
    // This is a native mapped original object, not an ARM runtime.
    OriginalNativeResourceEngine engine(0x2000u);
    OriginalOwnerConstructorInputs0x27a768 inputs{};
    inputs.mapped_allocator_global_word=f.allocator_slot.data();
    inputs.mapped_allocator_global_bytes=f.allocator_slot.size();
    inputs.original_pool_arm_va=0x00037000u;
    inputs.context=&f;
    assert(!engine.constructed());
    assert(engine.set_mode(1,1).status==
           OriginalReceiverModeStatus0x26c410::InvalidOriginalOwner);
    assert(engine.activate().status==
           OriginalReceiverActivationStatus0x278e58::InvalidOwner);
    auto result=engine.initialize(inputs);
    assert(result.status==S::Completed);
    assert(engine.constructed());
    const auto* p=engine.mapped_bytes();
    assert(engine.mapped_size()==kOriginalOwnerObjectBytes);
    assert(word(p)==kOriginalOwnerVtable0x27a768);
    assert(word(p+0x804u)==kOriginalEmbeddedVtable0x27a4f0);
    assert(word(p+0x808u)==0);
    assert(word(p+0x814u)==0xffffffffu);
    assert(word(p+0x818u)==0x2830u);
    assert(word(p+0x81cu)==0x2830u);
    assert(word(p+0x820u)==0x2890u);
    assert(word(p+0x824u)==0x0049c544u);
    assert(word(p+0x828u)==0);
    assert(word(p+0x82cu)==0x2830u);
    assert(word(p+0x830u)==0u); // embedded+0x2c stores a NUL byte
    assert(p[0x830u]==0u);
    assert(word(p+0x890u)==0);
    assert(word(p+0x894u)==0);
    assert(word(p+0x898u)==0);
    assert(word(p+0x89cu)==0);
    assert(word(p+0x8a4u)==0xfffffffeu);
    assert(word(p+0x8b0u)==8u);
    assert(p[0x8b4u]==0u&&p[0x8b5u]==1u&&p[0x8b6u]==0u);
    assert(word(p+0x8acu)==0);
    assert(word(p+0x8b8u)==0);
    assert(word(p+0x8c0u)==0x28c0u);
    assert(word(p+0x8c4u)==0x28c0u);
    assert(word(p+0x8c8u)==0);
    assert(word(p+0x8d4u)==0x00037000u);
    assert(word(p+0x8e0u)==0x28e0u);
    assert(word(p+0x8e4u)==0x28e0u);
    assert(word(p+0x8f4u)==0x00037000u);
    assert(word(p+0x8f8u)==0);
    auto mode=engine.set_mode(2u,0x34567890u);
    assert(mode.status==OriginalReceiverModeStatus0x26c410::Completed);
    assert(word(p+0x8b8u)==0x34567890u);
    mode=engine.set_mode(1u,0u,reset,&f);
    assert(mode.status==OriginalReceiverModeStatus0x26c410::Completed);
    assert(p[0x8b5u]==0u);
    auto activation=engine.activate();
    assert(activation.status==
           OriginalReceiverActivationStatus0x278e58::CompletedEmptyProviderReset);
    assert(word(p+0x8c8u)==0);
    // With no provider or token-to-game-object mapping, the engine
    // cannot claim that a resource has been loaded.
    auto receiver=engine.process_receiver_prefix(
        u"original",{},nullptr,nullptr);
    assert(receiver.status==
           OriginalReceiverPrefixStatus0x278018::OriginalActivationBlocked);
    result=engine.initialize(inputs);
    assert(result.status==S::InvalidMemory); // cannot re-construct live owner

    OriginalNativeResourceEngine absent_allocator(0x2000u);
    put(f.allocator_slot.data(),0u);
    result=absent_allocator.initialize(inputs);
    assert(result.status==S::MissingOriginalAllocator);
    assert(!absent_allocator.constructed());
    assert(word(absent_allocator.mapped_bytes())==0u);
    put(f.allocator_slot.data(),0x0049c544u);

    // Original r2==0: 0x27A768 calls real default pool function
    // 0x271914, never synthesizes a fake pool pointer.
    OriginalNativeResourceEngine missing_pool(0x2000u);
    inputs.original_pool_arm_va=0u;
    result=missing_pool.initialize(inputs);
    assert(result.status==S::MissingDefaultPool);
    assert(word(missing_pool.mapped_bytes())==0u);
    inputs.default_pool_0x271914=pool;
    f.refuse_pool=true;
    result=missing_pool.initialize(inputs);
    assert(result.status==S::OriginalDefaultPoolRefused);
    assert(!missing_pool.constructed());
    f.refuse_pool=false;
    result=missing_pool.initialize(inputs);
    assert(result.status==S::Completed);
    assert(f.default_pool_calls==2);
    assert(word(missing_pool.mapped_bytes()+0x8f4u)==0x37000u);

    OriginalNativeResourceEngine with_provider(0x3000u);
    inputs.original_pool_arm_va=0x37000u;
    inputs.original_provider_arm_va=0x51000u;
    inputs.default_pool_0x271914=nullptr;
    result=with_provider.initialize(inputs);
    assert(result.status==S::MissingOriginalProviderMethod);
    assert(!with_provider.constructed());
    inputs.provider_flags_0x14=provider;
    f.refuse_provider=true;
    result=with_provider.initialize(inputs);
    assert(result.status==S::OriginalProviderRefused);
    assert(!with_provider.constructed());
    f.refuse_provider=false;
    result=with_provider.initialize(inputs);
    assert(result.status==S::Completed);
    assert(with_provider.mapped_bytes()[0x8b4u]==1u);
    assert(word(with_provider.mapped_bytes()+0x8acu)==0x51000u);
    auto started=with_provider.activate();
    assert(started.status==
           OriginalReceiverActivationStatus0x278e58::UnreconstructedProviderActivation);
    assert(started.original_provider_arm_va==0x51000u);
    // Verify the actual first two original calls from 0x286918,
    // not an invented "engine ready" state.
    std::array<std::uint8_t,12> loader{};
    put(loader.data()+4,0x37000u);
    OriginalNativeResourceEngine boot(0x42000u);
    inputs.original_pool_arm_va=0u; // must reload loader+4
    inputs.original_provider_arm_va=0x51000u; // must overwrite from arg
    auto startup=boot.begin_loader_0x286918(
        loader.data(),loader.size(),0u,inputs);
    assert(startup.status==
           OriginalLoaderPrefixStatus0x286918::ReachedOriginalNullProviderExit);
    assert(startup.constructor_status==S::Completed);
    assert(startup.mode_status==OriginalReceiverModeStatus0x26c410::Completed);
    assert(startup.binding_status==OriginalOwnerBindStatus0x278778::ReachedOriginalNullProviderExit);
    assert(boot.constructed());
    const auto* bp=boot.mapped_bytes();
    assert(word(bp)==kOriginalOwnerVtable0x27a768);
    assert(word(bp+0x8acu)==0u); // original provider arg r1
    assert(word(bp+0x8d4u)==0x37000u); // from original [r0+4]
    assert(bp[0x8b5u]==1u);
    // Reusing the same ARM stack owner must fail rather than
    // reconstructing over live allocated/registered state.
    startup=boot.begin_loader_0x286918(
        loader.data(),loader.size(),0u,inputs);
    assert(startup.status==OriginalLoaderPrefixStatus0x286918::InvalidLoaderContext);
    OriginalNativeResourceEngine no_pool(0x50000u);
    put(loader.data()+4,0u);
    inputs.default_pool_0x271914=nullptr;
    startup=no_pool.begin_loader_0x286918(
        loader.data(),loader.size(),0u,inputs);
    assert(startup.status==OriginalLoaderPrefixStatus0x286918::ConstructorBlocked);
    assert(startup.constructor_status==S::MissingDefaultPool);
    assert(!no_pool.constructed());
    inputs.default_pool_0x271914=pool;
    startup=no_pool.begin_loader_0x286918(
        loader.data(),loader.size(),0u,inputs);
    assert(startup.status==
           OriginalLoaderPrefixStatus0x286918::ReachedOriginalNullProviderExit);
    assert(no_pool.constructed());
    assert(word(no_pool.mapped_bytes()+0x8f4u)==0x37000u);
    return 0;
}
