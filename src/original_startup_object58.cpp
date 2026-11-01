#include "ds_port/original_startup_object58.hpp"
#include <initializer_list>
namespace ds_port::original_runtime {
namespace {
void put(std::uint8_t* p,std::uint32_t value) noexcept {
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(value>>(i*8));
}
}
bool original_construct_object58_0x280a34(
    std::uint32_t owner,void* mem,std::size_t bytes,
    OriginalStartupObjectBaseProvider provider,void* context) noexcept {
    if(!owner || !mem || bytes<0x58 || !provider)return false;
    auto* p=static_cast<std::uint8_t*>(mem);
    // Original 0x280A58..0x280A78:
    put(p,kOriginal280a34Table0);
    put(p+4,kOriginal280a34Table4);
    put(p+0x14,kOriginal280a34Table14);
    for(unsigned at:{0x8u,0xcu,0x10u,0x18u,0x1cu,0x20u})put(p+at,0);
    std::uint32_t value=0;
    // 0x280A80 BL 0x27E560, +0x24, zero +0x28..+0x38.
    if(!provider(0x0027e560u,owner,context,&value))return false;
    put(p+0x24,value);
    for(unsigned at:{0x28u,0x2cu,0x30u,0x34u,0x38u})put(p+at,0);
    // 0x280A9C: same exact original callee again.
    if(!provider(0x0027e560u,owner,context,&value))return false;
    put(p+0x3c,value);
    for(unsigned at:{0x40u,0x44u,0x48u,0x4cu})put(p+at,0);
    // 0x280AB4: third call and final pointer at +0x50.
    if(!provider(0x0027e560u,owner,context,&value))return false;
    put(p+0x50,value);put(p+0x54,0);
    return true;
}
}
