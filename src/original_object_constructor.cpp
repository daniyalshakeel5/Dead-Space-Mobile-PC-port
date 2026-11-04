#include "ds_port/original_object_constructor.hpp"
namespace ds_port::original_runtime {
bool original_construct_0x2828ac(void* object,std::size_t count,
                                 std::uint32_t vtable) noexcept {
    if(!object || count<kOriginalConstructorMinimumBytes || !vtable) return false;
    auto* b=static_cast<std::uint8_t*>(object);
    for(unsigned i=0;i<4;++i)b[i]=std::uint8_t(vtable>>(i*8));
    for(unsigned i=4;i<16;++i)b[i]=0;
    b[0x10]=0;b[0x11]=0;b[0x12]=0;
    return true;
}
}
