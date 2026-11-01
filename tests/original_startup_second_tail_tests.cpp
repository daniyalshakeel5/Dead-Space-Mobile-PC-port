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
struct Test {
    std::array<std::uint8_t,0x70> context{};
    std::array<std::uint8_t,4> owner{},context_table_owner{};
    std::array<std::uint8_t,0x60> context_table{};
    std::array<std::uint8_t,0x10> owner_table{};
    int direct_count=0,resources=0;
};
bool resolve(std::uint32_t va,void* c,OriginalStartupServiceMemory* out) noexcept {
    auto& t=*static_cast<Test*>(c);
    if(va==0x1000) {
        *out={va,t.context.data(),t.context.size(),0x1100,
              t.context_table.data(),t.context_table.size()};
        return true;
    }
    if(va==0x2000) {
        *out={va,t.owner.data(),t.owner.size(),0x2100,
              t.owner_table.data(),t.owner_table.size()};
        return true;
    }
    return false;
}
bool direct(std::uint32_t va,std::uint32_t ctx,void* c) noexcept {
    auto& t=*static_cast<Test*>(c);++t.direct_count;
    assert(va==0x27fd7cu&&ctx==0x1000);
    return true;
}
bool virtual_call(const OriginalStartupVirtualCall& c,void*,
                  OriginalStartupVirtualResult* out) noexcept {
    assert(c.original_owner_arm_va==0x1000&&c.vtable_slot==0x54);
    *out={0,0};return true;
}
bool resource(std::uint32_t target,std::uint32_t ctx,std::uint32_t descriptor,
              void* c,std::uint32_t* result) noexcept {
    auto& t=*static_cast<Test*>(c);++t.resources;
    assert(target==0x2803e4u&&ctx==0x1000);
    assert(descriptor==kStartupResourcePrimary ||
           descriptor==kStartupResourceFallback);
    *result=0;return true;
}
}
int main(){
    Test t{};
    put(t.context.data(),0x1100);
    put(t.context.data()+4,0x2000);
    put(t.context_table.data()+0x54,0x7777);
    put(t.owner.data(),0x2100);
    put(t.owner_table.data()+0x0c,0x8888);
    OriginalStartupSecondTailInputs in{
        0x1000,t.context.data(),t.context.size(),resolve,virtual_call,
        direct,resource,nullptr,nullptr,nullptr,&t
    };
    auto r=original_execute_stage_zero_tail_0x280580(in);
    assert(r==OriginalStartupSecondTailStatus::MissingOriginalAllocator);
    assert(t.direct_count==1&&t.resources==2);
    for(int i=0;i<0x18;++i)
        if(i>=0x0c&&i<0x18)assert(t.context[i]==0);
    t.direct_count=t.resources=0;in.invoke_original_direct=nullptr;
    r=original_execute_stage_zero_tail_0x280580(in);
    assert(r==OriginalStartupSecondTailStatus::MissingOriginalDirect);
    assert(t.direct_count==0&&t.resources==0);
    return 0;
}
