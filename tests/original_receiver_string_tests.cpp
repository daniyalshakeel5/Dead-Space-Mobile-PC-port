#include "ds_port/original_receiver_string.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdint>
using namespace ds_port::original_runtime;
namespace {
void put(std::uint8_t* p,std::uint32_t v){
    for(unsigned n=0;n<4;++n)p[n]=std::uint8_t(v>>(8*n));
}
std::uint32_t get(const std::uint8_t* p){
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
}
int main(){
    using S=OriginalReceiverStringStatus0x27541c;
    std::array<std::uint8_t,4> slot{};
    std::array<std::uint8_t,kOriginalReceiverStringBytes> data{};
    data.fill(0xCC);
    put(slot.data(),0x49c544u);
    OriginalDefaultPoolInputs0x271914 pool{
        slot.data(),slot.size(),nullptr};
    OriginalReceiverStringInputs0x27541c in{
        0x2000u,data.data(),data.size(),&pool};
    auto result=original_construct_receiver_string_0x27541c(in);
    assert(result.status==S::Completed);
    assert(result.original_allocator_va==0x49c544u);
    assert(get(data.data())==0x2018u);
    assert(get(data.data()+4)==0x2018u);
    assert(get(data.data()+8)==0x2218u);
    assert(get(data.data()+0xc)==0x49c544u);
    assert(get(data.data()+0x10)==0);
    assert(get(data.data()+0x14)==0x2018u);
    assert(data[0x18]==0&&data[0x19]==0);
    assert(data[0x1a]==0xCC); // native original inline body untouched
    // Original static singleton dependency must not be fabricated.
    std::array<std::uint8_t,kOriginalReceiverStringBytes> missing{};
    in.mapped_string=missing.data();
    put(slot.data(),0);
    result=original_construct_receiver_string_0x27541c(in);
    assert(result.status==S::OriginalAllocatorBlocked);
    assert(get(missing.data())==0);
    put(slot.data(),0x49c544u);
    in.native_allocator_pool=nullptr;
    result=original_construct_receiver_string_0x27541c(in);
    assert(result.status==S::MissingAllocator);
    assert(get(missing.data())==0);
    in.native_allocator_pool=&pool;
    in.accessible_bytes=kOriginalReceiverStringBytes-1;
    result=original_construct_receiver_string_0x27541c(in);
    assert(result.status==S::InvalidMemory);
    in.accessible_bytes=missing.size();
    in.original_string_arm_va=0xfffffff0u;
    result=original_construct_receiver_string_0x27541c(in);
    assert(result.status==S::InvalidMemory);
    return 0;
}
