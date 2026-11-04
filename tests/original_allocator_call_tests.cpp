#include "ds_port/original_allocator_call.hpp"
#include <array>
#include <cstdint>
#include <iostream>
using namespace ds_port::original_runtime;
#define CHECK(x) do { if(!(x)) {std::cerr<<"Original allocator ABI mismatch at "<<__LINE__<<": "<<#x<<"\n";return 1;} } while(0)
namespace {
std::array<std::uint8_t,8> owner{};
std::array<std::uint8_t,24> vtable{};
std::array<std::uint8_t,32> allocated{};
std::array<std::uint8_t,8> global{};
unsigned calls=0;
OriginalAllocatorCall forwarded{};
bool original_returns_null=false;
bool original_returns_failure=false;
bool original_returns_undersized=false;
std::uint32_t le32(const std::uint8_t* a) {
    return std::uint32_t(a[0]) | (std::uint32_t(a[1])<<8) |
        (std::uint32_t(a[2])<<16) | (std::uint32_t(a[3])<<24);
}
void store32(std::uint8_t* a,std::uint32_t value) {
    for(int i=0;i<4;i++)a[i]=static_cast<std::uint8_t>(value>>(8*i));
}
bool original_vtable_method_test_only(
    const OriginalAllocatorCall& args,void*,OriginalAllocatorReturn* result) noexcept {
    ++calls;forwarded=args;
    if(original_returns_failure)return false;
    *result={
        original_returns_null?0u:0x34567890u,
        original_returns_null?nullptr:allocated.data(),
        original_returns_null?0u:(original_returns_undersized?31u:32u)
    };
    return true;
}
}
int main() {
    owner.fill(0xa5);vtable.fill(0xcd);
    allocated.fill(0x6d);global.fill(0x77);
    store32(owner.data(),0x10203040u);
    store32(vtable.data()+0x0c,0x002aabbcu);
    auto run=[&]() {
        return original_execute_allocator_call_0x281d54(
            0x12340000u,owner.data(),owner.size(),
            0x10203040u,vtable.data(),vtable.size(),
            global.data(),global.size(),
            original_vtable_method_test_only,nullptr);
    };
    CHECK(original_execute_allocator_call_0x281d54(
        0,owner.data(),owner.size(),0x10203040u,vtable.data(),vtable.size(),
        global.data(),global.size(),original_vtable_method_test_only,nullptr)
        ==OriginalAllocatorCallStatus::InvalidOriginalMemory);
    CHECK(original_execute_allocator_call_0x281d54(
        0x12340000u,owner.data(),3,0x10203040u,vtable.data(),vtable.size(),
        global.data(),global.size(),original_vtable_method_test_only,nullptr)
        ==OriginalAllocatorCallStatus::InvalidOriginalMemory);
    CHECK(original_execute_allocator_call_0x281d54(
        0x12340000u,owner.data(),owner.size(),0x10203041u,vtable.data(),vtable.size(),
        global.data(),global.size(),original_vtable_method_test_only,nullptr)
        ==OriginalAllocatorCallStatus::InvalidOriginalVtable);
    CHECK(original_execute_allocator_call_0x281d54(
        0x12340000u,owner.data(),owner.size(),0x10203040u,vtable.data(),15,
        global.data(),global.size(),original_vtable_method_test_only,nullptr)
        ==OriginalAllocatorCallStatus::InvalidOriginalVtable);
    CHECK(original_execute_allocator_call_0x281d54(
        0x12340000u,owner.data(),owner.size(),0x10203040u,vtable.data(),vtable.size(),
        global.data(),global.size(),nullptr,nullptr)
        ==OriginalAllocatorCallStatus::MissingOriginalAllocator);
    CHECK(calls==0);
    CHECK(le32(global.data())==0x77777777u);
    CHECK(run()==OriginalAllocatorCallStatus::OriginalRecordInitializedAndPublished);
    CHECK(calls==1);
    CHECK(forwarded.original_method_arm_address==0x002aabbcu);
    CHECK(forwarded.original_owner_arm_address==0x12340000u);
    CHECK(forwarded.requested_size==32u);
    CHECK(forwarded.original_type_descriptor_arm_address==0x0042fc70u);
    CHECK(forwarded.flag==1u);
    CHECK(forwarded.stack_arg0==4u && forwarded.stack_arg1==0u);
    CHECK(le32(global.data())==0x34567890u);
    CHECK(le32(allocated.data()+4)==0x34567894u);
    CHECK(le32(allocated.data()+8)==0x34567894u);
    CHECK(le32(allocated.data()+0x18)==0x12340000u);
    CHECK(le32(allocated.data()+0x1c)==0u);
    CHECK(allocated[0x10]==0);
    CHECK(allocated[0x11]==0x6d);
    CHECK(global[4]==0x77);
    // Owner vtable method can return zero; only publication occurs.
    original_returns_null=true;allocated.fill(0x6d);
    CHECK(run()==OriginalAllocatorCallStatus::OriginalAllocatorReturnedNull);
    CHECK(le32(global.data())==0u);
    for(const auto b:allocated)CHECK(b==0x6d);
    original_returns_null=false;
    original_returns_failure=true;
    global.fill(0x77);allocated.fill(0x6d);
    CHECK(run()==OriginalAllocatorCallStatus::OriginalAllocatorRefused);
    CHECK(le32(global.data())==0x77777777u);
    for(const auto b:allocated)CHECK(b==0x6d);
    original_returns_failure=false;
    original_returns_undersized=true;
    CHECK(run()==OriginalAllocatorCallStatus::InvalidOriginalMemory);
    CHECK(le32(global.data())==0x77777777u);
    original_returns_undersized=false;
    std::cout<<"Original ARM allocator vtable ABI, null branch and record writes passed\n";
    return 0;
}
