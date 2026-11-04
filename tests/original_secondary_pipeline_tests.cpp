#include "ds_port/original_secondary_pipeline.hpp"
#include "ds_port/original_embedded_initializer.hpp"
#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace ds_port::original_runtime;
namespace {
struct State {
    int allocations=0,registry_queries=0,virtual_calls=0,setup_calls=0;
    std::uint32_t token=0x5000;
    bool refuse_registry=false,refuse_setup=false;
};
void put(std::uint8_t* p,std::uint32_t v){
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(v>>(8*i));
}
bool allocator(const OriginalSecondaryAllocation& a,void* ctx,std::uint32_t* out) noexcept{
    auto& s=*static_cast<State*>(ctx);++s.allocations;
    assert(a.descriptor_arm_address==kSecondaryAllocatorOriginalDescriptorVA);
    assert(a.requested_bytes==36&&a.stack0==4&&a.stack1==0);
    *out=s.token;return true;
}
bool registry(const OriginalRegistryQuery& a,void* ctx,std::uint32_t* out) noexcept{
    auto& s=*static_cast<State*>(ctx);++s.registry_queries;
    assert(a.original_registry_arm_address==0x4000);
    assert(a.original_method_arm_address==0x4300&&a.interface_id==0x10004);
    if(s.refuse_registry)return false;
    *out=0x6000;return true;
}
bool extended_virtual(const OriginalEmbeddedVirtualCall& a,void* ctx,
                      OriginalEmbeddedVirtualResult* out) noexcept {
    auto& s=*static_cast<State*>(ctx);
    const unsigned slots[]={0x18,0x84,0x74,0x54,0x54,0x5c};
    const std::uint32_t methods[]={0x6018,0x6084,0x6074,0x6054,0x6054,0x605c};
    assert(s.virtual_calls<6);
    const auto step=s.virtual_calls++;
    assert(a.slot_offset==slots[step]&&a.original_method_arm_va==methods[step]);
    assert(a.original_interface_arm_va==0x6000);
    if(step==0){assert(a.r1==kOriginal27fb28FirstArgVA&&a.known_registers_mask==1);
        out->r0=0x9000;}
    if(step==1){assert(a.r1==0x9000&&a.r2==kOriginal27fb28Method84Arg2VA);
        assert(a.r3==kOriginal27fb28Method84Arg3VA&&a.known_registers_mask==7);
        out->r0=0xa000;out->r3_after=0xbeef;}
    if(step==2){assert(a.r1==0x9000&&a.r2==0xa000&&a.known_registers_mask==3);
        assert(a.saved_r3_stack_word&&*a.saved_r3_stack_word==0xbeef);
        out->r0=0xb000;}
    if(step==3){assert(a.r1==0xb000);out->r0=0xc000;}
    if(step==4){assert(a.r1==0x9000);out->r0=0x9990;}
    if(step==5){assert(a.r1==0x9000);out->r0=0;}
    return true;
}
bool setup84(const OriginalEmbeddedMethod84Call& a,void* ctx,
             std::uint32_t* out) noexcept {
    auto& s=*static_cast<State*>(ctx);++s.setup_calls;
    assert(a.original_method_arm_va==0x6084);
    assert(a.original_interface_arm_va==0x6000);
    assert(a.embedded_field_plus_four==0x9990);
    assert(a.caller_arg1==kOriginal27fa74FirstArgVA);
    assert(a.caller_arg2==kOriginal27fa74SecondArgVA);
    if(s.refuse_setup)return false;
    *out=0x6000;return true;
}
}
int main() {
    std::array<std::uint8_t,4> owner{},registry_slot{},registry_object{},interface_object{};
    std::array<std::uint8_t,16> owner_vtable{};
    std::array<std::uint8_t,0x1c> registry_vtable{};
    std::array<std::uint8_t,0x88> interface_vtable{};
    std::array<std::uint8_t,0x24> object{};
    put(owner.data(),0x1000);put(owner_vtable.data()+12,0x2000);
    put(registry_slot.data(),0x4000);put(registry_object.data(),0x4200);
    put(registry_vtable.data()+0x18,0x4300);
    put(interface_object.data(),0x6100);
    for(auto slot:{0x18u,0x84u,0x74u,0x54u,0x5cu})
        put(interface_vtable.data()+slot,0x6000u+slot);
    State state{};
    OriginalEmbeddedService service{
        registry_slot.data(),registry_slot.size(),0x4000,
        registry_object.data(),registry_object.size(),0x4200,
        registry_vtable.data(),registry_vtable.size(),0x6000,
        interface_object.data(),interface_object.size(),0x6100,
        interface_vtable.data(),interface_vtable.size(),
        registry,setup84,&state,extended_virtual
    };
    OriginalSecondaryPipeline args{
        0x3000,owner.data(),owner.size(),owner_vtable.data(),owner_vtable.size(),
        0x5000,object.data(),object.size(),allocator,&state,&service
    };
    auto result=original_run_secondary_pipeline(args);
    assert(result.allocation_status==OriginalSecondaryStatus::Completed);
    assert(result.postprocess_status==OriginalPostprocessStatus::Completed);
    assert(result.original_result_arm_address==0x5000);
    assert(state.allocations==1&&state.registry_queries==2);
    assert(state.virtual_calls==6&&state.setup_calls==1);
    assert(object[0]==0x98&&object[1]==0x9a&&object[2]==0x48);
    assert(object[0x14]==0xb8&&object[0x15]==0x97&&object[0x16]==0x48);
    assert(object[0x18]==0x90&&object[0x19]==0x99);
    assert(object[0x1c]==0x00&&object[0x1d]==0xc0);
    assert(object[0x20]==0&&object[0x21]==0x60);

    state={};state.token=0;object.fill(0);
    result=original_run_secondary_pipeline(args);
    assert(result.allocation_status==OriginalSecondaryStatus::Completed);
    assert(result.original_result_arm_address==0&&state.allocations==1);
    assert(state.registry_queries==0&&state.virtual_calls==0);
    for(auto byte:object)assert(byte==0);

    state={};state.token=0x9999;object.fill(0);
    result=original_run_secondary_pipeline(args);
    assert(result.allocation_status==OriginalSecondaryStatus::PostprocessRefused);
    assert(result.postprocess_status==OriginalPostprocessStatus::InvalidMemory);
    assert(state.virtual_calls==0&&result.original_result_arm_address==0);

    state={};state.refuse_setup=true;object.fill(0);
    result=original_run_secondary_pipeline(args);
    assert(result.allocation_status==OriginalSecondaryStatus::PostprocessRefused);
    assert(result.postprocess_status==OriginalPostprocessStatus::CallRefused);
    assert(state.virtual_calls==6&&state.setup_calls==1);

    state={};state.refuse_registry=true;object.fill(0);
    result=original_run_secondary_pipeline(args);
    assert(result.allocation_status==OriginalSecondaryStatus::PostprocessRefused);
    assert(result.postprocess_status==OriginalPostprocessStatus::CallRefused);
    assert(state.virtual_calls==0&&state.setup_calls==0);

    state={};args.embedded_service=nullptr;
    result=original_run_secondary_pipeline(args);
    assert(result.allocation_status==OriginalSecondaryStatus::MissingPostprocess);
    args.embedded_service=&service;
    args.allocator=nullptr;
    result=original_run_secondary_pipeline(args);
    assert(result.allocation_status==OriginalSecondaryStatus::MissingAllocator);
    return 0;
}
