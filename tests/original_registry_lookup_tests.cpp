#include "ds_port/original_registry_lookup.hpp"
#include <array>
#include <cassert>
#include <cstdint>
using namespace ds_port::original_runtime;
namespace {
void store(std::uint8_t* b, std::uint32_t x) {
    for(unsigned i=0;i<4;i++) b[i]=std::uint8_t(x>>(8*i));
}
struct Fixture { int calls=0; bool refuse=false; std::uint32_t result=0x87654321; };
bool invoke(const OriginalRegistryQuery& q,void* data,std::uint32_t* out) noexcept {
    auto& f=*static_cast<Fixture*>(data);
    ++f.calls;
    assert(q.original_registry_arm_address==0x40000000u);
    assert(q.original_method_arm_address==0x12345678u);
    assert(q.interface_id==0x10004u);
    assert(*out==0);
    if(f.refuse)return false;
    *out=f.result;
    return true;
}
}
int main() {
    std::array<std::uint8_t,4> slot{},obj{};
    std::array<std::uint8_t,0x1c> table{};
    store(slot.data(),0x40000000u);
    store(obj.data(),0x40001000u);
    store(table.data()+0x18,0x12345678u);
    Fixture f{};
    std::uint32_t out=0xdeadbeefu;
    auto run=[&](OriginalRegistryQueryDispatch fn) {
        return original_query_registry_0x27e930(
            slot.data(),slot.size(),0x40000000u,
            obj.data(),obj.size(),0x40001000u,table.data(),table.size(),
            fn,&f,&out);
    };
    assert(run(invoke)==OriginalRegistryStatus::Completed);
    assert(out==f.result && f.calls==1);
    out=0xdeadbeefu; f.refuse=true;
    assert(run(invoke)==OriginalRegistryStatus::CallRefused);
    assert(out==0xdeadbeefu && f.calls==2);
    f.refuse=false;
    assert(run(nullptr)==OriginalRegistryStatus::MissingImplementation && f.calls==2);
    store(slot.data(),0);
    assert(run(invoke)==OriginalRegistryStatus::InvalidMemory && f.calls==2);
    store(slot.data(),0x40000000u);
    store(table.data()+0x18,0);
    assert(run(invoke)==OriginalRegistryStatus::InvalidVtable && f.calls==2);
    store(table.data()+0x18,0x12345678u);
    out=0xdeadbeefu;f.result=0;
    assert(run(invoke)==OriginalRegistryStatus::Completed && out==0);
    assert(original_query_registry_0x27e930(
        slot.data(),slot.size(),0x40000000u,obj.data(),obj.size(),
        0x40001000u,table.data(),0x1b,invoke,&f,&out)
        ==OriginalRegistryStatus::InvalidMemory);
    return 0;
}
