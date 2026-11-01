#include "ds_port/original_startup_second_tail.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
using namespace ds_port::original_runtime;
namespace {
void put(std::uint8_t* p,std::uint32_t v) {
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(v>>(8*i));
}
std::uint32_t get(const std::uint8_t* p) {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
struct Harness {
    std::array<std::uint8_t,0x70> ctx{};
    std::array<std::uint8_t,0x60> context_table{};
    std::array<std::uint8_t,4> owner{};
    std::array<std::uint8_t,0x10> owner_table{};
    std::array<std::uint8_t,0x34> object34{};
    std::array<std::uint8_t,0x10> table34{};
    std::array<std::uint8_t,0x24> secondary{};
    std::array<std::uint8_t,0x10> secondary_table{};
    std::array<std::uint8_t,0x58> object58{};
    std::array<std::uint8_t,0x28> table58{};
    std::array<std::uint8_t,4> registry_slot{},registry{},interface{};
    std::array<std::uint8_t,0x1c> registry_table{};
    std::array<std::uint8_t,0x88> interface_table{};
    int allocs=0,virtuals=0,directs=0,resources=0,bases=0;
    int embedded_virtuals=0,service_84=0,queries=0,secondary_allocs=0;
    bool refuse_last=false;
    bool use_native_registration=false;
    int global_service_calls=0;
    std::array<std::uint8_t,4> global_service_word{};
    int registrations=0,registration_lookups=0;
    std::array<std::array<std::uint8_t,4>,27> registration_got{};
    std::array<std::array<std::uint8_t,4>,27> registration_values{};
};
bool map(std::uint32_t va,void* c,OriginalStartupServiceMemory* out) noexcept {
    auto& h=*static_cast<Harness*>(c);
    if(va==0x1000)*out={va,h.ctx.data(),h.ctx.size(),0x1100,
        h.context_table.data(),h.context_table.size()};
    else if(va==0x2000)*out={va,h.owner.data(),h.owner.size(),0x2100,
        h.owner_table.data(),h.owner_table.size()};
    else if(va==0x3000)*out={va,h.object34.data(),h.object34.size(),
        kOriginal287a94Table0,h.table34.data(),h.table34.size()};
    else if(va==0x4000)*out={va,h.secondary.data(),h.secondary.size(),
        kOriginal288688OverrideVtableVA,h.secondary_table.data(),
        h.secondary_table.size()};
    else if(va==0x6000)*out={va,h.object58.data(),h.object58.size(),
        kOriginal280a34Table0,h.table58.data(),h.table58.size()};
    else return false;
    return true;
}
bool direct(std::uint32_t callee,std::uint32_t ctx,void* c) noexcept {
    auto& h=*static_cast<Harness*>(c);++h.directs;
    assert(callee==(h.use_native_registration?0x36fd38u:
        (h.directs==1?0x27fd7cu:0x36fd38u)));
    assert(ctx==(h.use_native_registration?0u:
        (h.directs==1?0x1000u:0u)));
    return true;
}
bool virt(const OriginalStartupVirtualCall& call,void* c,
          OriginalStartupVirtualResult* out) noexcept {
    auto& h=*static_cast<Harness*>(c);
    const std::uint32_t owners[]={0x1000,0x3000,0x4000,0x6000};
    const std::uint32_t slots[]={0x54,0x0c,0x0c,0x24};
    const std::uint32_t targets[]={0x1120,0x3010,0x4010,0x6010};
    assert(h.virtuals<4);
    const int step=h.virtuals++;
    assert(call.original_owner_arm_va==owners[step]);
    assert(call.vtable_slot==slots[step]);
    assert(call.original_method_arm_va==targets[step]);
    if(step==1){assert(call.r1==0x1000&&call.r2==0x2000);}
    if(step==2){assert(call.r1==0x1000&&call.r2==0x2000);}
    if(step==3){assert(call.r1==0x2000&&call.r2==0x1000);}
    if(step==3&&h.refuse_last)return false;
    *out={0x7000,0};return true;
}
bool resource(std::uint32_t callee,std::uint32_t ctx,std::uint32_t descriptor,
              void* c,std::uint32_t* out) noexcept {
    auto& h=*static_cast<Harness*>(c);++h.resources;
    assert(callee==0x2803e4u&&ctx==0x1000);
    assert(descriptor==(h.resources==1?
        kStartupResourcePrimary:kStartupResourceFallback));
    *out=h.resources==1?0:1;return true;
}
bool allocate(const OriginalAllocatorCall& q,void* c,
              OriginalAllocatorReturn* result) noexcept {
    auto& h=*static_cast<Harness*>(c);
    assert(q.original_owner_arm_address==0x2000);
    assert(q.original_method_arm_address==0x2200);
    assert(q.flag==1&&q.stack_arg0==4&&q.stack_arg1==0);
    ++h.allocs;
    if(h.allocs==1){
        assert(q.requested_size==0x34&&
               q.original_type_descriptor_arm_address==kStartupAllocate34Descriptor);
        *result={0x3000,h.object34.data(),h.object34.size()};
    } else {
        assert(h.allocs==2&&q.requested_size==0x58);
        assert(q.original_type_descriptor_arm_address==kStartupAllocate58Descriptor);
        *result={0x6000,h.object58.data(),h.object58.size()};
    }
    return true;
}
bool base(std::uint32_t method,std::uint32_t owner,
          void* c,std::uint32_t* result) noexcept {
    auto& h=*static_cast<Harness*>(c);++h.bases;
    assert(method==0x27e560u);
    const std::uint32_t args[]={0x3000,0x6000,0x8002,0x8003};
    assert(h.bases<=4&&owner==args[h.bases-1]);
    *result=0x8000u+h.bases;return true;
}
bool secondary_alloc(const OriginalSecondaryAllocation& a,void* c,
                     std::uint32_t* result) noexcept {
    auto& h=*static_cast<Harness*>(c);++h.secondary_allocs;
    assert(a.owner_arm_address==0x2000&&a.method_arm_address==0x2200);
    assert(a.requested_bytes==36&&a.descriptor_arm_address==0x42ffb0u);
    *result=0x4000;return true;
}
bool registry(const OriginalRegistryQuery& query,void* c,
              std::uint32_t* result) noexcept {
    auto& h=*static_cast<Harness*>(c);++h.queries;
    assert(query.interface_id==0x10004u&&
           query.original_method_arm_address==0x5018);
    *result=0x5200;return true;
}
bool embedded_virtual(const OriginalEmbeddedVirtualCall& call,void* c,
                      OriginalEmbeddedVirtualResult* out) noexcept {
    auto& h=*static_cast<Harness*>(c);
    const unsigned slots[]={0x18,0x84,0x74,0x54,0x54,0x5c};
    assert(h.embedded_virtuals<6&&call.slot_offset==slots[h.embedded_virtuals]);
    ++h.embedded_virtuals;
    const unsigned values[]={0x9000,0xa000,0xb000,0xc000,0x9990,0};
    *out={values[h.embedded_virtuals-1],0};return true;
}
bool embedded84(const OriginalEmbeddedMethod84Call& q,void* c,
                std::uint32_t* result) noexcept {
    auto& h=*static_cast<Harness*>(c);++h.service_84;
    assert(q.embedded_field_plus_four==0x9990u);
    assert(q.caller_arg1==kOriginal27fa74FirstArgVA);
    assert(q.caller_arg2==kOriginal27fa74SecondArgVA);
    *result=0x5200;return true;
}
bool registration_global(std::uint32_t original_slot,void* c,
                         OriginalRegistrationMappedGlobal* out) noexcept {
    auto& h=*static_cast<Harness*>(c);
    assert(h.registration_lookups<27);
    const auto i=static_cast<unsigned>(h.registration_lookups++);
    assert(original_slot==kOriginalRegistrationSequence0x27fd7c[i].global_slot_arm_va);
    *out={0x8000u+4*i,h.registration_got[i].data(),
          h.registration_got[i].size(),h.registration_values[i].data(),
          h.registration_values[i].size()};
    return true;
}
bool register_resource(const OriginalRegistrationCall& q,void* c) noexcept {
    auto& h=*static_cast<Harness*>(c);
    assert(h.registrations<27);
    const auto i=static_cast<unsigned>(h.registrations++);
    assert(q.original_callsite_va==kOriginalRegistrationSequence0x27fd7c[i].callsite_arm_va);
    assert(q.descriptor_arm_va==kOriginalRegistrationSequence0x27fd7c[i].descriptor_arm_va);
    assert(q.original_subcontext_va==0x1018u);
    assert(q.original_global_value==0x9000u+i);
    assert(q.original_tail_branch==(i==26));
    return true;
}
bool global_construct(void*,std::size_t bytes,std::uint32_t desc,
                      std::uint32_t duration,std::uint32_t owner,
                      std::uint32_t literal,std::uint32_t enable,
                      void* ctx) noexcept {
    auto& h=*static_cast<Harness*>(ctx);
    assert(h.global_service_calls++==0);
    assert(bytes==0x148u&&desc==kOriginalGlobalServiceDescriptorVA);
    assert(duration==1000000u&&owner==0xAAAAu);
    assert(literal==kOriginalGlobalServiceArgumentVA&&enable==1u);
    put(h.global_service_word.data(),0xBBBBu);
    return true;
}
bool global_attach(std::uint32_t owner,void*,std::size_t len,
                   void* ctx) noexcept {
    auto& h=*static_cast<Harness*>(ctx);
    assert(h.global_service_calls++==1);
    assert(owner==0xBBBBu&&len==0x148u);
    return true;
}
bool global_finish(void*,std::size_t len,void* ctx,
                   std::uint32_t* result) noexcept {
    auto& h=*static_cast<Harness*>(ctx);
    assert(h.global_service_calls++==2);
    assert(len==0x138u);
    *result=0;return true;
}
void prepare(Harness& h) {
    put(h.ctx.data(),0x1100);put(h.ctx.data()+4,0x2000);
    put(h.context_table.data()+0x54,0x1120);
    put(h.owner.data(),0x2100);put(h.owner_table.data()+0xc,0x2200);
    put(h.table34.data()+0xc,0x3010);
    put(h.secondary_table.data()+0xc,0x4010);
    put(h.table58.data()+0x24,0x6010);
    put(h.registry_slot.data(),0x5000);put(h.registry.data(),0x5100);
    put(h.registry_table.data()+0x18,0x5018);
    put(h.interface.data(),0x5300);
    for(unsigned slot:{0x18u,0x84u,0x74u,0x54u,0x5cu})
        put(h.interface_table.data()+slot,0x5500u+slot);
}
}
int main(){
    Harness h{};prepare(h);
    OriginalEmbeddedService embedded{
        h.registry_slot.data(),h.registry_slot.size(),0x5000,
        h.registry.data(),h.registry.size(),0x5100,
        h.registry_table.data(),h.registry_table.size(),
        0x5200,h.interface.data(),h.interface.size(),0x5300,
        h.interface_table.data(),h.interface_table.size(),
        registry,embedded84,&h,embedded_virtual
    };
    OriginalSecondaryPipeline secondary{
        0x2000,h.owner.data(),h.owner.size(),
        h.owner_table.data(),h.owner_table.size(),
        0x4000,h.secondary.data(),h.secondary.size(),
        secondary_alloc,&h,&embedded
    };
    OriginalStartupSecondTailInputs in{
        0x1000,h.ctx.data(),h.ctx.size(),map,virt,direct,resource,
        allocate,base,&secondary,&h
    };
    const auto r=original_execute_stage_zero_tail_0x280580(in);
    assert(r==OriginalStartupSecondTailStatus::CompletedOriginalSecondRoutine);
    assert(h.directs==2&&h.resources==2&&h.allocs==2&&h.bases==4);
    assert(h.virtuals==4&&h.secondary_allocs==1);
    assert(h.queries==2&&h.embedded_virtuals==6&&h.service_84==1);
    assert(get(h.ctx.data()+0x0c)==0x3000);
    assert(get(h.ctx.data()+0x14)==0x4000);
    assert(get(h.ctx.data()+0x10)==0x6000);
    assert(get(h.object34.data())==kOriginal287a94Table0);
    assert(get(h.object58.data())==kOriginal280a34Table0);
    assert(get(h.secondary.data())==kOriginal288688OverrideVtableVA);
    h.refuse_last=true;
    h.directs=h.resources=h.allocs=h.bases=h.virtuals=0;
    h.secondary_allocs=h.embedded_virtuals=h.queries=h.service_84=0;
    assert(original_execute_stage_zero_tail_0x280580(in)==
           OriginalStartupSecondTailStatus::OriginalVirtualRefused);

    // Exercise the 27-call native 0x27FD7C wrapper through the FULL
    // stage-zero tail, not just the isolated registration unit test.
    h.refuse_last=false;h.use_native_registration=true;
    h.directs=h.resources=h.allocs=h.bases=h.virtuals=0;
    h.secondary_allocs=h.embedded_virtuals=h.queries=h.service_84=0;
    h.registrations=h.registration_lookups=0;
    for(unsigned i=0;i<27;++i) {
        put(h.registration_got[i].data(),0x8000u+i*4u);
        put(h.registration_values[i].data(),0x9000u+i);
    }
    OriginalRegistrationInputs registered{0x1000u,registration_global,
                                          register_resource,&h};
    in.native_registration_0x27fd7c=&registered;
    assert(original_execute_stage_zero_tail_0x280580(in)==
           OriginalStartupSecondTailStatus::CompletedOriginalSecondRoutine);
    assert(h.registrations==27&&h.registration_lookups==27);
    // The ONLY remaining direct call in this fixture is 0x36FD38.
    assert(h.directs==1&&h.resources==2&&h.allocs==2);
    assert(h.virtuals==4&&h.embedded_virtuals==6);
    registered.original_context_arm_va=0xDEADBEEFu;
    assert(original_execute_stage_zero_tail_0x280580(in)==
           OriginalStartupSecondTailStatus::InvalidOriginalContext);

    // Integration: native wrappers for BOTH original direct calls.
    // No invented direct-call results, but the remaining engine leaf
    // methods are controlled test callbacks and not gameplay code.
    registered.original_context_arm_va=0x1000u;
    h.directs=h.resources=h.allocs=h.bases=h.virtuals=0;
    h.secondary_allocs=h.embedded_virtuals=h.queries=h.service_84=0;
    h.registrations=h.registration_lookups=h.global_service_calls=0;
    put(h.global_service_word.data(),0xAAAAu);
    OriginalGlobalServiceInputs global_service{
        h.global_service_word.data(),h.global_service_word.size(),
        global_construct,global_attach,global_finish,&h};
    in.native_global_service_0x36fd38=&global_service;
    in.invoke_original_direct=nullptr;
    assert(original_execute_stage_zero_tail_0x280580(in)==
           OriginalStartupSecondTailStatus::CompletedOriginalSecondRoutine);
    assert(h.directs==0&&h.registrations==27&&h.registration_lookups==27);
    assert(h.global_service_calls==3&&h.resources==2&&h.allocs==2);
    assert(h.virtuals==4&&h.secondary_allocs==1);
    return 0;
}
