#include "ds_port/original_record_postalloc.hpp"
#include <array>
#include <cstdint>
#include <iostream>
using namespace ds_port::original_runtime;
#define CHECK(x) do{if(!(x)){std::cerr<<"Original ARM 0x281d54 postalloc mismatch "<<__LINE__<<" :"<<#x<<"\n";return 1;}}while(0)
namespace {
std::uint32_t load32(const std::uint8_t* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
}
int main(){
    std::array<std::uint8_t,40> record{};
    std::array<std::uint8_t,8> global{};
    record.fill(0xb6);global.fill(0x7d);
    const auto initial_record=record;
    const auto initial_global=global;
    CHECK(original_complete_allocator_postreturn_0x281d54(
        record.data(),record.size(),0xdeadbeefu,0x10203040u,
        nullptr,global.size())==OriginalPostAllocateResult::InvalidNativeAddressOrBounds);
    CHECK(original_complete_allocator_postreturn_0x281d54(
        record.data(),record.size(),0xdeadbeefu,0x10203040u,
        global.data(),3)==OriginalPostAllocateResult::InvalidNativeAddressOrBounds);
    CHECK(original_complete_allocator_postreturn_0x281d54(
        nullptr,record.size(),0xdeadbeefu,0x10203040u,
        global.data(),global.size())==OriginalPostAllocateResult::InvalidNativeAddressOrBounds);
    CHECK(original_complete_allocator_postreturn_0x281d54(
        record.data(),0x1f,0xdeadbeefu,0x10203040u,
        global.data(),global.size())==OriginalPostAllocateResult::InvalidNativeAddressOrBounds);
    CHECK(record==initial_record && global==initial_global);

    CHECK(original_complete_allocator_postreturn_0x281d54(
        nullptr,0,0xdeadbeefu,0u,global.data(),4)
        ==OriginalPostAllocateResult::OriginalNullResultPublished);
    CHECK(record==initial_record);
    CHECK(load32(global.data())==0);
    for(std::size_t i=4;i<global.size();++i)CHECK(global[i]==initial_global[i]);

    global.fill(0x7d);
    CHECK(original_complete_allocator_postreturn_0x281d54(
        record.data()+1,0x20,0xdeadbeefu,0x10203040u,
        global.data()+1,4)
        ==OriginalPostAllocateResult::OriginalRecordInitializedAndPublished);
    const auto* item=record.data()+1;
    CHECK(load32(item+4)==0x10203044u);
    CHECK(load32(item+8)==0x10203044u);
    CHECK(load32(item+0x0c)==0);
    CHECK(item[0x10]==0);
    CHECK(item[0x11]==0xb6 && item[0x12]==0xb6 && item[0x13]==0xb6);
    CHECK(load32(item+0x14)==0u);
    CHECK(load32(item+0x18)==0xdeadbeefu);
    CHECK(load32(item+0x1c)==0);
    CHECK(load32(global.data()+1)==0x10203040u);
    CHECK(global[0]==0x7d && global[5]==0x7d);
    // All untouched record bytes must preserve allocator-provided memory.
    for(std::size_t i=0;i<record.size();++i) {
        const auto off=std::ptrdiff_t(i)-1;
        bool touched=(off>=4 && off<16) || off==16 ||
                     (off>=20 && off<32);
        if(!touched)CHECK(record[i]==initial_record[i]);
    }

    record.fill(0xb6);global.fill(0x7d);
    CHECK(original_complete_allocator_postreturn_0x281d54(
        record.data(),record.size(),0,0xfffffffcu,global.data(),global.size())
        ==OriginalPostAllocateResult::OriginalRecordInitializedAndPublished);
    CHECK(load32(record.data()+4)==0u); // original ARM uint32 wrap
    CHECK(load32(record.data()+8)==0u);
    CHECK(load32(global.data())==0xfffffffcu);
    std::cout<<"Original ARM allocator postreturn memory layout and global publication verified\n";
    return 0;
}
