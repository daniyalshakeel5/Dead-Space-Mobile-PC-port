#include "ds_port/original_static_singleton.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <array>
using namespace ds_port::original_runtime;
namespace {
struct Test {int acquires=0,releases=0,registrations=0;unsigned acquire_result=1;};
std::uint32_t word(const std::uint8_t* p){
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
        (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
bool acquire(void*,void* ctx,std::uint32_t* result) noexcept {
    auto& t=*static_cast<Test*>(ctx);++t.acquires;*result=t.acquire_result;return true;
}
bool release(void* guard,void* ctx) noexcept {
    auto& t=*static_cast<Test*>(ctx);++t.releases;
    auto* p=static_cast<std::uint8_t*>(guard);
    p[0]|=1;return true;
}
bool atexit(std::uint32_t value,void* ctx) noexcept {
    auto& t=*static_cast<Test*>(ctx);++t.registrations;
    assert(value==0x9004);return true;
}
}
int main(){
    std::array<std::uint8_t,0x18> bytes{};
    Test trace{};
    OriginalSingletonInputs in{0x9000,bytes.data(),bytes.size(),
        0x4000,0x5000,acquire,release,atexit,&trace};
    auto r=original_initialize_static_0x27e560(in);
    assert(r.status==OriginalSingletonStatus::Completed&&r.constructed);
    assert(r.original_object_arm_va==0x9004);
    assert(word(bytes.data()+4)==0x4008);
    assert(word(bytes.data()+8)==0x5000);
    assert(bytes[0x0c]==0 && word(bytes.data()+0x10)==0 &&
           word(bytes.data()+0x14)==0);
    assert(trace.acquires==1&&trace.releases==1&&trace.registrations==1);
    r=original_initialize_static_0x27e560(in);
    assert(r.status==OriginalSingletonStatus::Completed&&!r.constructed);
    assert(trace.acquires==1&&trace.releases==1);
    bytes.fill(0);trace.acquire_result=0;
    r=original_initialize_static_0x27e560(in);
    assert(r.status==OriginalSingletonStatus::Completed&&!r.constructed);
    assert(trace.acquires==2&&trace.releases==1);
    // Both original startup object constructors call 0x27E560.
    // First call initializes the singleton; second returns same ARM token.
    bytes.fill(0);trace={};
    std::uint32_t from34=0,from58=0;
    assert(original_static_singleton_base_provider(
        0x0027e560u,0xa000u,&in,&from34));
    assert(original_static_singleton_base_provider(
        0x0027e560u,0xb000u,&in,&from58));
    assert(from34==0x9004u&&from58==from34);
    assert(trace.acquires==1&&trace.releases==1&&trace.registrations==1);
    assert(!original_static_singleton_base_provider(
        0x0027e564u,0,&in,&from58));
    // Guard and GOT source addresses come from actual game ELF, not
    // arbitrary ARM VAs supplied by a test factory.
    std::array<std::uint8_t,4> field_got{},table_got{},dtor_got{},dso_got{},source{};
    const auto put=[](std::uint8_t* p,std::uint32_t v){
        for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(v>>(8*i));
    };
    put(field_got.data(),0x8800);put(table_got.data(),0x4000);
    put(dtor_got.data(),0x8900);put(dso_got.data(),0x8a00);
    put(source.data(),0x5000);
    auto mapped=in;mapped.guard_arm_va=kVerifiedSingletonGuardVA;
    mapped.original_object_vtable_arm_va=0;
    mapped.original_component_vtable_arm_va=0;
    OriginalSingletonOriginalGOT got{
        field_got.data(),table_got.data(),dtor_got.data(),dso_got.data(),
        0x8800,source.data()
    };
    OriginalSingletonInputs verified{};
    assert(original_bind_static_singleton_from_got(mapped,got,&verified));
    assert(verified.guard_arm_va==kVerifiedSingletonGuardVA);
    assert(verified.original_object_vtable_arm_va==0x4000);
    assert(verified.original_component_vtable_arm_va==0x5000);
    put(table_got.data(),0);
    assert(!original_bind_static_singleton_from_got(mapped,got,&verified));
    mapped.guard_arm_va=0x9999;
    assert(!original_bind_static_singleton_from_got(mapped,got,&verified));
    in.mapped_bytes=0x17;
    assert(original_initialize_static_0x27e560(in).status==
           OriginalSingletonStatus::InvalidMemory);
    return 0;
}
