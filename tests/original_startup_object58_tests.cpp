#include "ds_port/original_startup_object58.hpp"
#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace ds_port::original_runtime;
namespace {
int calls=0;
bool create(std::uint32_t method,std::uint32_t owner,
            void*,std::uint32_t* out) noexcept {
    assert(method==0x27e560u&&owner==0x5000);
    ++calls;*out=0x8000u+static_cast<unsigned>(calls);return true;
}
std::uint32_t get(const std::uint8_t* p) {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
        (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
}
int main() {
    std::array<std::uint8_t,0x60> obj{};obj.fill(0xb9);
    assert(original_construct_object58_0x280a34(
        0x5000,obj.data(),obj.size(),create,nullptr));
    assert(calls==3);
    assert(get(obj.data())==kOriginal280a34Table0);
    assert(get(obj.data()+4)==kOriginal280a34Table4);
    assert(get(obj.data()+0x14)==kOriginal280a34Table14);
    assert(get(obj.data()+0x24)==0x8001u);
    assert(get(obj.data()+0x3c)==0x8002u);
    assert(get(obj.data()+0x50)==0x8003u);
    for(unsigned off:{0x8u,0xcu,0x10u,0x18u,0x1cu,0x20u,
                      0x28u,0x2cu,0x30u,0x34u,0x38u,
                      0x40u,0x44u,0x48u,0x4cu,0x54u})
        assert(get(obj.data()+off)==0);
    assert(get(obj.data()+0x58)==0xb9b9b9b9u);
    obj.fill(0xb9);calls=0;
    assert(!original_construct_object58_0x280a34(
        0x5000,obj.data(),0x57,create,nullptr));
    assert(calls==0&&get(obj.data())==0xb9b9b9b9u);
    return 0;
}
