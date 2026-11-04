#include "ds_port/original_startup_object34.hpp"
#include <initializer_list>
namespace ds_port::original_runtime {
namespace {
void put(std::uint8_t* p,std::uint32_t value) noexcept {
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(value>>(i*8));
}
}
bool original_construct_object34_0x287a94(
    std::uint32_t address,void* memory,std::size_t bytes,
    OriginalStartupObjectBaseProvider provider,void* ctx) noexcept {
    if(!address||!memory||bytes<0x34||!provider)return false;
    auto* obj=static_cast<std::uint8_t*>(memory);
    // Original ARM 0x287AB8..0x287ADC exactly:
    put(obj,kOriginal287a94Table0);put(obj+4,kOriginal287a94Table4);
    put(obj+0x14,kOriginal287a94Table14);
    for(unsigned offset:{0x8u,0xcu,0x10u,0x18u,0x1cu,0x20u})
        put(obj+offset,0);
    // 0x287AE0 BL 0x27E560(r0=original object).
    std::uint32_t returned=0;
    if(!provider(0x0027e560u,address,ctx,&returned))return false;
    // ARM stores the returned object field at +0x24, and zeros the rest.
    put(obj+0x30,0);
    put(obj+0x24,returned);
    put(obj+0x28,0);
    put(obj+0x2c,0);
    return true;
}
}
