#include "ds_port/original_startup_global_service.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <array>
using namespace ds_port::original_runtime;
namespace {
void put(std::uint8_t* b,std::uint32_t v){
    for(unsigned i=0;i<4;++i)b[i]=std::uint8_t(v>>(i*8));
}
std::uint32_t get(const std::uint8_t* p){
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
struct Trace{
    std::array<std::uint8_t,4> global{};
    void* temp=nullptr;
    int calls=0;
    int refuse=-1;
};
bool construct(void* buffer,std::size_t length,std::uint32_t descriptor,
               std::uint32_t duration,std::uint32_t owner,
               std::uint32_t stack_arg,std::uint32_t mode,void* c) noexcept {
    auto& t=*static_cast<Trace*>(c);
    assert(t.calls++==0);
    assert(length==0x148u&&descriptor==0x00371608u);
    assert(duration==1000000u&&owner==0x1234u);
    assert(stack_arg==0x0043c9c0u&&mode==1u);
    t.temp=buffer;
    // Confirm original later reloads owner after this call, not from cache.
    put(t.global.data(),0x5678);
    return t.refuse!=0;
}
bool attach(std::uint32_t owner,void* buffer,std::size_t length,
            void* c) noexcept{
    auto& t=*static_cast<Trace*>(c);
    assert(t.calls++==1);
    assert(owner==0x5678u&&buffer==t.temp&&length==0x148u);
    return t.refuse!=1;
}
bool finish(void* buffer,std::size_t length,void* c,
            std::uint32_t* out) noexcept{
    auto& t=*static_cast<Trace*>(c);
    assert(t.calls++==2);
    assert(buffer==static_cast<std::uint8_t*>(t.temp)+0x10);
    assert(length==0x138u);
    assert(get(static_cast<std::uint8_t*>(t.temp))==
           kOriginalGlobalServiceTempVtableVA);
    *out=0xBEEFCAFE;return t.refuse!=2;
}
}
int main(){
    Trace t{};put(t.global.data(),0x1234);
    OriginalGlobalServiceInputs in{
        t.global.data(),t.global.size(),construct,attach,finish,&t};
    const auto ok=original_execute_global_service_0x36fd38(in);
    assert(ok.status==OriginalGlobalServiceStatus::CompletedOriginalWrapper);
    assert(ok.original_r0==0xBEEFCAFE&&t.calls==3);
    for(int i=0;i<3;++i){
        t.calls=0;t.refuse=i;put(t.global.data(),0x1234);
        const auto r=original_execute_global_service_0x36fd38(in);
        assert(r.original_r0==0&&t.calls==i+1);
        assert(r.status==(i==0?OriginalGlobalServiceStatus::OriginalConstructRefused:
                          i==1?OriginalGlobalServiceStatus::OriginalAttachRefused:
                               OriginalGlobalServiceStatus::OriginalFinishRefused));
    }
    in.original_0x282614=nullptr;
    assert(original_execute_global_service_0x36fd38(in).status==
           OriginalGlobalServiceStatus::MissingOriginalCallees);
    in.original_0x282614=attach;
    in.global_word_bytes=3;
    assert(original_execute_global_service_0x36fd38(in).status==
           OriginalGlobalServiceStatus::InvalidOriginalMemory);
    return 0;
}
