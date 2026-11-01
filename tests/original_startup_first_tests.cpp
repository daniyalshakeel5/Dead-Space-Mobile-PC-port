#include "ds_port/original_startup_first.hpp"
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
struct Context {
    std::array<std::uint8_t,4> registry_slot{},registry{},obj{};
    std::array<std::uint8_t,0x1c> registry_table{};
    std::array<std::uint8_t,0x23c> service_table{};
    int queries=0,calls=0,directs=0,fail_step=-1;
};
bool query(const OriginalRegistryQuery& q,void* c,std::uint32_t* result) noexcept {
    auto& s=*static_cast<Context*>(c);
    assert(q.original_registry_arm_address==0x4000);
    assert(q.original_method_arm_address==0x4300);
    assert(q.interface_id==0x10004u);
    ++s.queries;
    if(s.fail_step==20)return false;
    *result=0x5000;return true;
}
bool resolve(std::uint32_t va,void* c,OriginalStartupServiceMemory* result) noexcept {
    auto& s=*static_cast<Context*>(c);
    assert(va==0x5000);
    *result={0x5000,s.obj.data(),s.obj.size(),
            0x5100,s.service_table.data(),s.service_table.size()};
    return true;
}
bool call(const OriginalStartupVirtualCall& q,void* c,
          OriginalStartupVirtualResult* out) noexcept {
    auto& s=*static_cast<Context*>(c);
    const std::uint32_t slots[]={0x18,0x1c4,0x18,0x1c4,0x84,0x8c,0x238};
    const std::uint32_t values[]={0x8000,0x8100,0x8200,0x8300,0x8400,0x8500,0x8600};
    assert(s.calls<7);
    const auto i=s.calls++;
    assert(q.vtable_slot==slots[i]);
    assert(q.original_owner_arm_va==0x5000);
    assert(q.original_method_arm_va==0x6000+slots[i]);
    if(i==0)assert(q.r1==kStartupFirstDescriptor&&q.known_reg_mask==1);
    if(i==1)assert(q.r1==0x8000&&q.r2==kStartupFirstArg2&&q.r3==kStartupFirstArg3);
    if(i==2)assert(q.r1==kStartupNestedDescriptor);
    if(i==3)assert(q.r1==0x8200&&q.r2==kStartupNestedArg2&&q.r3==kStartupNestedArg3);
    if(i==4)assert(q.r1==0x8200&&q.r2==kStartupNextArg2&&q.r3==kStartupNextArg3);
    if(i==5)assert(q.r1==0x9000&&q.r2==0x8400&&q.saved_r3_host_word&&
                  *q.saved_r3_host_word==0x55ff);
    if(i==6)assert(q.r1==0x8000&&q.r2==0x8100&&q.saved_r3_host_word&&
                  *q.saved_r3_host_word==0x8500);
    if(s.fail_step==i)return false;
    *out={values[i],i==4?0x55ffu:0u};
    return true;
}
bool direct(std::uint32_t r0,std::uint32_t r1,std::uint32_t r2,
            void* c,std::uint32_t* result) noexcept {
    auto& s=*static_cast<Context*>(c);++s.directs;
    assert(r0==0x5000&&r1==0x8200&&r2==0x8300);
    if(s.fail_step==21)return false;
    *result=0x9000;return true;
}
}
int main(){
    Context c{};
    put(c.registry_slot.data(),0x4000);put(c.registry.data(),0x4200);
    put(c.registry_table.data()+0x18,0x4300);put(c.obj.data(),0x5100);
    for(const unsigned slot:{0x18u,0x1c4u,0x84u,0x8cu,0x238u})
        put(c.service_table.data()+slot,0x6000u+slot);
    OriginalStartupFirstInputs in{
        {c.registry_slot.data(),c.registry_slot.size(),0x4000,
         c.registry.data(),c.registry.size(),0x4200,
         c.registry_table.data(),c.registry_table.size(),query},
        resolve,call,direct,&c
    };
    assert(original_execute_startup_first_0x27f790(in)==
           OriginalStartupFirstStatus::CompletedOriginalFirstRoutine);
    assert(c.queries==2&&c.calls==7&&c.directs==1);
    c.calls=c.queries=c.directs=0;c.fail_step=4;
    assert(original_execute_startup_first_0x27f790(in)==
           OriginalStartupFirstStatus::OriginalVirtualRefused);
    assert(c.calls==5&&c.directs==1);
    c.calls=c.queries=c.directs=0;c.fail_step=21;
    assert(original_execute_startup_first_0x27f790(in)==
           OriginalStartupFirstStatus::OriginalDirectRefused);
    assert(c.calls==4&&c.directs==1);
    c.fail_step=-1;c.calls=c.queries=c.directs=0;
    auto missing=in;missing.direct_0x2068a8=nullptr;
    assert(original_execute_startup_first_0x27f790(missing)==
           OriginalStartupFirstStatus::MissingOriginalDirectCall);
    c.calls=c.queries=c.directs=0;
    put(c.service_table.data()+0x238,0);
    assert(original_execute_startup_first_0x27f790(in)==
           OriginalStartupFirstStatus::MissingOriginalVirtualMethod);
    return 0;
}
