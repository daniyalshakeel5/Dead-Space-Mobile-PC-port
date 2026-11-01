#include "ds_port/original_object_constructor.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <array>
using namespace ds_port::original_runtime;
int main(){
    std::array<std::uint8_t,24> bytes{};
    bytes.fill(0xcc);
    assert(original_construct_0x2828ac(bytes.data(),bytes.size(),0x44332211));
    assert(bytes[0]==0x11&&bytes[1]==0x22&&bytes[2]==0x33&&bytes[3]==0x44);
    for(unsigned i=4;i<=0x12;++i)assert(bytes[i]==0);
    for(unsigned i=0x13;i<bytes.size();++i)assert(bytes[i]==0xcc);
    bytes.fill(0xcc);
    assert(!original_construct_0x2828ac(bytes.data(),0x12,0x44332211));
    for(auto x:bytes)assert(x==0xcc);
    assert(!original_construct_0x2828ac(bytes.data(),bytes.size(),0));
    assert(!original_construct_0x2828ac(nullptr,bytes.size(),123));
    return 0;
}
