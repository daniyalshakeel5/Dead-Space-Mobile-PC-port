#include "ds_port/original_embedded_setup.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <array>
#include <cstdint>
using namespace ds_port::original_runtime;
namespace {
void put(std::uint8_t* p,std::uint32_t v){
    for(unsigned i=0;i<4;++i)p[i]=static_cast<std::uint8_t>(v>>(8*i));
}
struct Counters {int queries=0;int methods=0;bool refuse_query=false;bool refuse_method=false;std::uint32_t interface=0x5000;};
bool query(const OriginalRegistryQuery& request,void* ctx,std::uint32_t* result) noexcept {
    auto& s=*static_cast<Counters*>(ctx);
    ++s.queries;
    assert(request.interface_id==0x10004u);
    assert(request.original_registry_arm_address==0x4000);
    assert(request.original_method_arm_address==0x4300);
    assert(*result==0);
    if(s.refuse_query)return false;
    *result=s.interface;return true;
}
bool method(const OriginalEmbeddedMethod84Call& request,void* ctx,std::uint32_t* result) noexcept {
    auto& s=*static_cast<Counters*>(ctx);
    ++s.methods;
    assert(request.original_method_arm_va==0x5200);
    assert(request.original_interface_arm_va==0x5000);
    assert(request.embedded_field_plus_four==0x9990);
    assert(request.caller_arg1==0x00430020);
    assert(request.caller_arg2==0x00430030);
    if(s.refuse_method)return false;
    *result=0xabcddcba;return true;
}
}
int main(){
    std::array<std::uint8_t,8> embedded{};
    std::array<std::uint8_t,4> registry_slot{},registry{},interface{};
    std::array<std::uint8_t,0x1c> reg_vtable{};
    std::array<std::uint8_t,0x88> interface_vtable{};
    put(embedded.data()+4,0x9990);
    put(registry_slot.data(),0x4000);put(registry.data(),0x4200);
    put(reg_vtable.data()+0x18,0x4300);
    put(interface.data(),0x5100);put(interface_vtable.data()+0x84,0x5200);
    Counters calls{};
    OriginalEmbeddedService svc{
        registry_slot.data(),registry_slot.size(),
        0x4000,registry.data(),registry.size(),
        0x4200,reg_vtable.data(),reg_vtable.size(),
        0x5000,interface.data(),interface.size(),
        0x5100,interface_vtable.data(),interface_vtable.size(),
        query,method,&calls
    };
    std::uint32_t out=0xdeadbeef;
    auto invoke=[&](){return original_execute_embedded_setup_0x27fa74(
        embedded.data(),embedded.size(),0x00430020,0x00430030,svc,&out);};
    assert(invoke()==OriginalEmbeddedSetupStatus::Completed);
    assert(out==0xabcddcba&&calls.queries==1&&calls.methods==1);
    out=0xdeadbeef;calls.refuse_query=true;
    assert(invoke()==OriginalEmbeddedSetupStatus::RegistryLookupRefused);
    assert(out==0xdeadbeef&&calls.methods==1);
    calls.refuse_query=false;calls.refuse_method=true;
    assert(invoke()==OriginalEmbeddedSetupStatus::OriginalMethod84Refused);
    assert(out==0xdeadbeef&&calls.methods==2);
    calls.refuse_method=false;calls.interface=0x7000;
    assert(invoke()==OriginalEmbeddedSetupStatus::UnexpectedInterface);
    assert(out==0xdeadbeef);
    calls.interface=0x5000;svc.call_method84=nullptr;
    assert(invoke()==OriginalEmbeddedSetupStatus::MissingOriginalMethod84);
    svc.call_method84=method;svc.interface_vtable_bytes=0x87;
    assert(invoke()==OriginalEmbeddedSetupStatus::InvalidMemory);
    svc.interface_vtable_bytes=0x88;put(interface_vtable.data()+0x84,0);
    assert(invoke()==OriginalEmbeddedSetupStatus::InvalidOriginalInterfaceVtable);
    return 0;
}
