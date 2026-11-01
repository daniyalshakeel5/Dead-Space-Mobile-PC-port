#include "ds_port/original_startup_registration.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
using namespace ds_port::original_runtime;
namespace {
void put(std::uint8_t* out,std::uint32_t value){
    for(unsigned i=0;i<4;++i)out[i]=static_cast<std::uint8_t>(value>>(8*i));
}
struct Fixture {
    std::array<std::array<std::uint8_t,4>,27> got{};
    std::array<std::array<std::uint8_t,4>,27> values{};
    int calls=0;
    int lookups=0;
    int refuse_call=-1;
    bool wrong_pointer=false;
};
bool lookup(std::uint32_t original_slot,void* context,
            OriginalRegistrationMappedGlobal* out) noexcept {
    auto& f=*static_cast<Fixture*>(context);
    assert(f.lookups<27);
    std::size_t i=static_cast<std::size_t>(f.lookups++);
    assert(original_slot==kOriginalRegistrationSequence0x27fd7c[i].global_slot_arm_va);
    *out={0x6000u+static_cast<std::uint32_t>(i)*4u,
          f.got[i].data(),f.got[i].size(),
          f.values[i].data(),f.values[i].size()};
    return true;
}
bool call(const OriginalRegistrationCall& call,void* context) noexcept {
    auto& f=*static_cast<Fixture*>(context);
    const int i=f.calls++;
    assert(i>=0&&i<27);
    const auto& original=kOriginalRegistrationSequence0x27fd7c[static_cast<std::size_t>(i)];
    assert(call.original_callsite_va==original.callsite_arm_va);
    assert(call.descriptor_arm_va==original.descriptor_arm_va);
    assert(call.helper_arm_va==0x00284178u);
    assert(call.original_global_value==0x7000u+static_cast<std::uint32_t>(i));
    assert(call.original_subcontext_va==0x0049cac8u);
    assert(call.original_tail_branch==(i==26));
    return i!=f.refuse_call;
}
}
int main(){
    static_assert(kOriginalRegistrationSequence0x27fd7c.size()==27);
    static_assert(kOriginalRegistrationSequence0x27fd7c[0].callsite_arm_va==0x0027fda8);
    static_assert(kOriginalRegistrationSequence0x27fd7c[26].callsite_arm_va==0x00280038);
    static_assert(kOriginalRegistrationSequence0x27fd7c[26].tail_call);
    Fixture f{};
    for(unsigned i=0;i<27;++i){
        put(f.got[i].data(),0x6000u+i*4u);
        put(f.values[i].data(),0x7000u+i);
    }
    OriginalRegistrationInputs in{0x0049cab0u,lookup,call,&f};
    const auto completed=original_register_stage_resources_0x27fd7c(in);
    assert(completed==OriginalRegistrationStatus::CompletedOriginalSequence);
    assert(f.calls==27&&f.lookups==27);
    f.calls=0;f.lookups=0;f.refuse_call=12;
    assert(original_register_stage_resources_0x27fd7c(in)==
           OriginalRegistrationStatus::OriginalRegistrationRefused);
    assert(f.calls==13&&f.lookups==13);
    f.calls=0;f.lookups=0;f.refuse_call=-1;
    put(f.got[0].data(),0);
    assert(original_register_stage_resources_0x27fd7c(in)==
           OriginalRegistrationStatus::InvalidOriginalGotPointer);
    assert(f.calls==0&&f.lookups==1);
    put(f.got[0].data(),0x6000);
    in.invoke_original_0x284178=nullptr;
    assert(original_register_stage_resources_0x27fd7c(in)==
           OriginalRegistrationStatus::MissingOriginalRegistrationHelper);
    in.invoke_original_0x284178=call;
    in.original_context_arm_va=0xfffffff0u;
    assert(original_register_stage_resources_0x27fd7c(in)==
           OriginalRegistrationStatus::InvalidOriginalContext);
    return 0;
}
