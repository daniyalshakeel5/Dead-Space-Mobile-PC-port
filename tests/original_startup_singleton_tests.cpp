#include "ds_port/original_startup_singleton.hpp"
#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace ds_port::original_runtime;
namespace {
int registrations=0;
void put(std::uint8_t* p,std::uint32_t v) {
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(v>>(8*i));
}
std::uint32_t word(const std::uint8_t* p){
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
bool registered(std::uint32_t obj,std::uint32_t destructor,
                std::uint32_t dso,void*) noexcept {
    assert(obj==kSingletonObjectVA&&destructor==0x8000&&dso==0x9000);
    ++registrations;return true;
}
}
int main(){
    std::array<std::uint8_t,0x18> object{};
    std::array<std::uint8_t,4> got_field{},got_table{},got_dtor{},got_dso{},src{};
    put(got_field.data(),0xa000);put(got_table.data(),0xb000);
    put(got_dtor.data(),0x8000);put(got_dso.data(),0x9000);
    put(src.data(),0xc000);
    OriginalSingletonMemory m{object.data(),object.size(),
        got_field.data(),got_table.data(),got_dtor.data(),got_dso.data(),
        0xa000,src.data(),registered,nullptr};
    std::uint32_t va=0;
    assert(original_initialize_singleton_0x27e560(m,&va)==
           OriginalSingletonStatus::ConstructedAndRegistered);
    assert(va==kSingletonObjectVA&&registrations==1);
    assert(word(object.data())==1&&word(object.data()+4)==0xb008);
    assert(word(object.data()+8)==0xc000);
    for(int i=0;i<4;++i){
        va=0;
        assert(original_singleton_base_provider_0x27e560(
            0x27e560,0x1000u+i,&m,&va));
        assert(va==kSingletonObjectVA);
    }
    assert(registrations==1);
    object.fill(0);put(got_field.data(),0xdead);
    assert(original_initialize_singleton_0x27e560(m,&va)==
           OriginalSingletonStatus::InvalidMemory);
    assert(word(object.data())==0);
    return 0;
}
