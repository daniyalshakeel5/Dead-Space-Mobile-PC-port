#include "ds_port/original_secondary_pipeline.hpp"
#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace ds_port::original_runtime;
namespace {
struct State {
    int allocations=0,embedded_init=0,registry_queries=0,method_calls=0;
    std::uint32_t token=0x5000;
    bool fail_registry=false,fail_method=false;
    std::uint8_t* object=nullptr;
};
void put(std::uint8_t* p,std::uint32_t v){
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(v>>(8*i));
}
bool allocate(const OriginalSecondaryAllocation& a,void* ctx,std::uint32_t* out) noexcept {
    auto& s=*static_cast<State*>(ctx);++s.allocations;
    assert(a.descriptor_arm_address==0x0042ffb0u);
    assert(a.requested_bytes==0x24u&&a.stack0==4&&a.stack1==0);
    *out=s.token;return true;
}
bool embedded_init(std::uint32_t target,std::uint32_t addr,
                   std::uint32_t arg1,std::uint32_t arg2,void* ctx,
                   std::uint32_t* result) noexcept {
    auto& s=*static_cast<State*>(ctx);
    assert(target==0x0027fb28u&&addr==0x5014u);
    assert(arg1==kOriginal27fb28FirstArgVA&&arg2==0);
    ++s.embedded_init;
    put(s.object+0x18,0x9990); // synthetic fixture for unrecovered 0x27FB28
    *result=0;return true;
}
bool registry_query(const OriginalRegistryQuery& call,void* ctx,
                    std::uint32_t* result) noexcept {
    auto& s=*static_cast<State*>(ctx);++s.registry_queries;
    assert(call.original_registry_arm_address==0x4000);
    assert(call.original_method_arm_address==0x4300);
    assert(call.interface_id==0x10004u);
    if(s.fail_registry)return false;
    *result=0x6000;return true;
}
bool slot84(const OriginalEmbeddedMethod84Call& call,void* ctx,
            std::uint32_t* result) noexcept {
    auto& s=*static_cast<State*>(ctx);++s.method_calls;
    assert(call.original_method_arm_va==0x6200u);
    assert(call.original_interface_arm_va==0x6000u);
    assert(call.embedded_field_plus_four==0x9990u);
    assert(call.caller_arg1==kOriginal27fa74FirstArgVA);
    assert(call.caller_arg2==kOriginal27fa74SecondArgVA);
    if(s.fail_method)return false;
    *result=0x6000u;return true;
}
}
int main(){
    std::array<std::uint8_t,4> owner{},registry_slot{},registry{},interface{};
    std::array<std::uint8_t,16> allocator_vtable{};
    std::array<std::uint8_t,0x1c> registry_vtable{};
    std::array<std::uint8_t,0x88> interface_vtable{};
    std::array<std::uint8_t,0x24> object{};
    put(owner.data(),0x1000);put(allocator_vtable.data()+12,0x2000);
    put(registry_slot.data(),0x4000);put(registry.data(),0x4200);
    put(registry_vtable.data()+0x18,0x4300);
    put(interface.data(),0x6100);put(interface_vtable.data()+0x84,0x6200);
    State s{};s.object=object.data();
    OriginalEmbeddedService service{
        registry_slot.data(),registry_slot.size(),
        0x4000,registry.data(),registry.size(),
        0x4200,registry_vtable.data(),registry_vtable.size(),
        0x6000,interface.data(),interface.size(),
        0x6100,interface_vtable.data(),interface_vtable.size(),
        registry_query,slot84,&s
    };
    OriginalSecondaryPipeline inputs{
        0x3000,owner.data(),owner.size(),allocator_vtable.data(),allocator_vtable.size(),
        0x5000,object.data(),object.size(),
        allocate,embedded_init,&s,&service
    };
    auto r=original_run_secondary_pipeline(inputs);
    assert(r.allocation_status==OriginalSecondaryStatus::Completed);
    assert(r.postprocess_status==OriginalPostprocessStatus::Completed);
    assert(r.original_result_arm_address==0x5000u);
    assert(s.allocations==1&&s.embedded_init==1&&s.registry_queries==1&&s.method_calls==1);
    assert(object[0]==0x98&&object[1]==0x9a&&object[2]==0x48);
    for(unsigned i=4;i<=0x12;++i)assert(object[i]==0);
    assert(object[0x20]==0&&object[0x21]==0x60);
    s={};s.object=object.data();s.token=0x9999;object.fill(0);
    r=original_run_secondary_pipeline(inputs);
    assert(r.allocation_status==OriginalSecondaryStatus::PostprocessRefused);
    assert(r.postprocess_status==OriginalPostprocessStatus::InvalidMemory);
    assert(r.original_result_arm_address==0&&s.embedded_init==0&&s.registry_queries==0);
    s={};s.object=object.data();s.token=0;object.fill(0);
    r=original_run_secondary_pipeline(inputs);
    assert(r.allocation_status==OriginalSecondaryStatus::Completed);
    assert(r.original_result_arm_address==0&&s.allocations==1&&s.embedded_init==0);
    for(auto byte:object)assert(byte==0);
    s={};s.object=object.data();s.fail_method=true;object.fill(0);
    r=original_run_secondary_pipeline(inputs);
    assert(r.allocation_status==OriginalSecondaryStatus::PostprocessRefused);
    assert(r.postprocess_status==OriginalPostprocessStatus::CallRefused);
    assert(s.embedded_init==1&&s.registry_queries==1&&s.method_calls==1);
    s={};s.object=object.data();inputs.embedded_service=nullptr;
    r=original_run_secondary_pipeline(inputs);
    assert(r.allocation_status==OriginalSecondaryStatus::PostprocessRefused);
    assert(r.postprocess_status==OriginalPostprocessStatus::MissingOriginalCallees);
    return 0;
}
