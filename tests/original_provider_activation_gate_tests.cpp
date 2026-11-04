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
void put(std::uint8_t* p,std::uint32_t v) noexcept {
    for(unsigned n=0;n<4;++n)p[n]=std::uint8_t(v>>(n*8));
}
struct Fixture {
    std::array<std::uint8_t,0x8fc> owner{};
    std::array<std::uint8_t,4> allocator{};
    std::uint32_t first=2,second=2;
    unsigned queries=0;
    bool deny=false;
};
bool query(std::uint32_t provider,std::uint32_t slot,
           void* context,std::uint32_t* result) noexcept {
    auto& f=*static_cast<Fixture*>(context);
    assert(provider==0x4000u || provider==0x2804u ||
           provider==0x51000u);
    assert(slot==0x14u);
    ++f.queries;
    if(f.deny)return false;
    *result=f.queries==1?f.first:f.second;
    return true;
}
}
int main(){
    using A=OriginalReceiverActivationStatus0x278e58;
    using B=OriginalOwnerBindStatus0x278778;
    Fixture f{};
    put(f.owner.data()+0x8ac,0x4000);
    OriginalReceiverActivationInputs0x278e58 input{
        0x2000u,f.owner.data(),f.owner.size(),nullptr,&f,query,1u
    };
    auto a=original_execute_receiver_activation_0x278e58(input);
    assert(a.status==A::CompletedProviderRejected);
    assert(a.original_provider_arm_va==0x4000u);
    assert(f.queries==2);
    assert(f.owner[0x8b6]==0);
    // True provider mode match reaches the ORIGINAL 0x27541C
    // string construction stage, not a fake engine-ready result.
    f.queries=0;f.first=1;
    a=original_execute_receiver_activation_0x278e58(input);
    assert(a.status==A::ReachedUnreconstructedStreamInitialization);
    assert(f.queries==1);
    // The special (request=1, provider mode=3) branch invokes
    // real owner virtual +0x60 (0x26C4CC); refuse to emulate it.
    f.queries=0;f.first=2;f.second=3;
    a=original_execute_receiver_activation_0x278e58(input);
    assert(a.status==A::UnreconstructedOwnerApproval);
    assert(f.queries==2);
    // Mode changing from mismatched to matching between the two
    // original +0x14 queries also requires owner virtual+0x60.
    f.queries=0;f.second=1;
    a=original_execute_receiver_activation_0x278e58(input);
    assert(a.status==A::UnreconstructedOwnerApproval);
    // Even a mismatched mode may be handled by the original owner's
    // embedded provider at owner+0x804, so no artificial rejection.
    f.queries=0;f.second=2;
    put(f.owner.data()+0x8ac,0x2804);
    a=original_execute_receiver_activation_0x278e58(input);
    assert(a.status==A::UnreconstructedOwnerApproval);
    put(f.owner.data()+0x8ac,0x4000);
    f.queries=0;f.deny=true;
    a=original_execute_receiver_activation_0x278e58(input);
    assert(a.status==A::ProviderModeQueryRefused&&f.queries==1);
    f.deny=false;
    // If the original provider callback was not supplied, it remains
    // an explicitly incomplete native engine service.
    input.query_provider_mode_0x14=nullptr;
    a=original_execute_receiver_activation_0x278e58(input);
    assert(a.status==A::UnreconstructedProviderActivation);
    input.query_provider_mode_0x14=query;
    f.queries=0;
    OriginalOwnerBindInputs0x278778 bind{
        0x2000u,f.owner.data(),f.owner.size(),nullptr,&f,query
    };
    auto b=original_bind_resource_owner_0x278778(bind);
    assert(b.status==B::ReachedOriginalProviderRejectedExit);
    assert(b.activation_status==A::CompletedProviderRejected);
    assert(f.queries==2);
    // Verify a *constructed* engine owner reaches the same verified
    // original branch from actual 0x286918 startup ordering.
    f.queries=0;
    put(f.allocator.data(),0x6000u);
    std::array<std::uint8_t,12> loader{};
    put(loader.data()+4,0x7000u);
    OriginalOwnerConstructorInputs0x27a768 services{};
    services.mapped_allocator_global_word=f.allocator.data();
    services.mapped_allocator_global_bytes=f.allocator.size();
    services.provider_flags_0x14=query;
    services.context=&f;
    OriginalNativeResourceEngine engine(0x2000u);
    // Original provider VA in the same fixture has to match query.
    auto boot=engine.begin_loader_0x286918(
        loader.data(),loader.size(),0x4000u,services);
    assert(boot.status==
       OriginalLoaderPrefixStatus0x286918::ReachedOriginalProviderRejectedExit);
    assert(boot.binding_status==B::ReachedOriginalProviderRejectedExit);
    assert(boot.constructor_status==
       OriginalOwnerConstructorStatus0x27a768::Completed);
    assert(engine.constructed()&&f.queries==3);
    assert(engine.mapped_bytes()[0x8b6u]==0);
    // An original null provider still exits with its distinct path.
    OriginalNativeResourceEngine empty(0x3000u);
    boot=empty.begin_loader_0x286918(
        loader.data(),loader.size(),0u,services);
    assert(boot.status==
       OriginalLoaderPrefixStatus0x286918::ReachedOriginalNullProviderExit);
    return 0;
}
