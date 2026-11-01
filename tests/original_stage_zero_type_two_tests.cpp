#include "ds_port/original_stage_zero_type_two.hpp"
#include <array>
#include <cstdint>
#include <iostream>
using ds_port::original_runtime::original_initialize_type_two_0x28ca24;
#define CHECK(x) do {if(!(x)){std::cerr << "Original ARM type-2 mismatch " << __LINE__ << ": " << #x << "\n";return 1;}}while(0)
namespace {
std::uint32_t get32(const std::uint8_t* v) {
    return std::uint32_t(v[0]) |
        (std::uint32_t(v[1])<<8) |
        (std::uint32_t(v[2])<<16) |
        (std::uint32_t(v[3])<<24);
}
}
int main(){
    std::array<std::uint8_t,44> object{};
    object.fill(0xab);
    std::array<std::uint8_t,4> source{{0x00,0x00,0x80,0x3f}};
    const auto untouched=object;
    CHECK(!original_initialize_type_two_0x28ca24(
        nullptr,object.size(),source.data(),source.size()));
    CHECK(!original_initialize_type_two_0x28ca24(
        object.data(),0x17,source.data(),source.size()));
    CHECK(!original_initialize_type_two_0x28ca24(
        object.data(),object.size(),nullptr,source.size()));
    CHECK(!original_initialize_type_two_0x28ca24(
        object.data(),object.size(),source.data(),3));
    CHECK(object==untouched);
    CHECK(original_initialize_type_two_0x28ca24(
        object.data()+1,0x18,source.data(),4));
    CHECK(get32(object.data()+1+0x10)==2);
    CHECK(get32(object.data()+1+0x14)==0x3f800000u);
    for(std::size_t i=0;i<object.size();++i) {
        const bool touched=(i>=1+0x10 && i<1+0x18);
        if(!touched)CHECK(object[i]==untouched[i]);
    }
    // VFP load and store copy bits including negative zero / NaN; they
    // do not modify the byte pattern or perform float arithmetic.
    const std::array<std::uint32_t,4> patterns{{
        0x80000000u,0x7fc00001u,0x3eaaaaabu,0xffffffffu}};
    for(const auto bits:patterns) {
        source={static_cast<std::uint8_t>(bits),
                static_cast<std::uint8_t>(bits>>8),
                static_cast<std::uint8_t>(bits>>16),
                static_cast<std::uint8_t>(bits>>24)};
        CHECK(original_initialize_type_two_0x28ca24(
            object.data()+1,0x18,source.data(),4));
        CHECK(get32(object.data()+1+0x10)==2u);
        CHECK(get32(object.data()+1+0x14)==bits);
    }
    std::cout<<"Original ARM type-2 selected complete switch case verified\n";
    return 0;
}
