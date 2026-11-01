#include "ds_port/original_startup_object34.hpp"
#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace ds_port::original_runtime;
namespace {
int calls=0;
bool provider(std::uint32_t target,std::uint32_t owner,
              void*,std::uint32_t* result) noexcept {
    assert(target==0x27e560u&&owner==0x5000);
    ++calls;*result=0x66778899;return true;
}
std::uint32_t get(const std::uint8_t* p) {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
        (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
}
int main(){
    std::array<std::uint8_t,0x38> obj{};
    obj.fill(0x91);
    assert(original_construct_object34_0x287a94(
        0x5000,obj.data(),obj.size(),provider,nullptr));
    assert(calls==1);
    assert(get(obj.data())==kOriginal287a94Table0);
    assert(get(obj.data()+4)==kOriginal287a94Table4);
    assert(get(obj.data()+0x14)==kOriginal287a94Table14);
    assert(get(obj.data()+0x24)==0x66778899u);
    for(unsigned offset:{0x8u,0xcu,0x10u,0x18u,0x1cu,0x20u,
                        0x28u,0x2cu,0x30u})assert(get(obj.data()+offset)==0);
    assert(get(obj.data()+0x34)==0x91919191);
    calls=0;obj.fill(0x91);
    assert(!original_construct_object34_0x287a94(
        0x5000,obj.data(),0x33,provider,nullptr));
    assert(calls==0&&get(obj.data())==0x91919191u);
    assert(!original_construct_object34_0x287a94(
        0x5000,obj.data(),obj.size(),nullptr,nullptr));
    return 0;
}
