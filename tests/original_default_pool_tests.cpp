#include "ds_port/original_default_pool.hpp"
#include "ds_port/original_resource_owner.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdint>
using namespace ds_port::original_runtime;
namespace {
void put(std::uint8_t* p,std::uint32_t v) noexcept {
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(v>>(8*i));
}
std::uint32_t get(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
        (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
struct Fixture {
    std::array<std::uint8_t,4> global_slot{};
    std::array<std::uint8_t,0x18> guard_and_object{};
    unsigned acquired=0,released=0,registered=0;
    bool refuse=false;
};
bool acquire(void* ptr,void* context,std::uint32_t* acquired) noexcept {
    auto& f=*static_cast<Fixture*>(context);
    assert(ptr==f.guard_and_object.data());
    ++f.acquired;*acquired=f.refuse?0u:1u;
    return true;
}
bool release(void* ptr,void* context) noexcept {
    auto& f=*static_cast<Fixture*>(context);
    assert(ptr==f.guard_and_object.data());
    ++f.released;
    put(f.guard_and_object.data(),1);
    return true;
}
bool register_destructor(std::uint32_t va,void* context) noexcept {
    auto& f=*static_cast<Fixture*>(context);
    assert(va==kVerifiedSingletonObjectVA);
    ++f.registered;return true;
}
}
int main(){
    using S=OriginalDefaultPoolStatus0x271914;
    Fixture f{};
    OriginalSingletonInputs singleton{
        kVerifiedSingletonGuardVA,f.guard_and_object.data(),
        f.guard_and_object.size(),
        0x4000u,0x5000u,acquire,release,register_destructor,&f
    };
    OriginalDefaultPoolInputs0x271914 in{
        f.global_slot.data(),f.global_slot.size(),nullptr};
    auto r=original_default_pool_0x271914(in);
    assert(r.status==S::MissingOriginalSingleton&&r.original_r0==0u);
    assert(get(f.global_slot.data())==0u);
    // Original 0x271920 returns nonzero BSS pointer immediately,
    // before 0x27E560, and does not need its imported services.
    put(f.global_slot.data(),0x41000u);
    r=original_default_pool_0x271914(in);
    assert(r.status==S::CompletedExisting&&r.original_r0==0x41000u);
    assert(f.acquired==0&&f.released==0&&f.registered==0);
    // A zero original 0x49BCA8 slot takes original 0x27E560.
    put(f.global_slot.data(),0u);
    in.native_singleton_0x27e560=&singleton;
    r=original_default_pool_0x271914(in);
    assert(r.status==S::CompletedNewSingleton);
    assert(r.original_r0==kVerifiedSingletonObjectVA);
    assert(get(f.global_slot.data())==kVerifiedSingletonObjectVA);
    assert(get(f.guard_and_object.data()+4u)==0x4008u);
    assert(get(f.guard_and_object.data()+8u)==0x5000u);
    assert(f.acquired==1&&f.released==1&&f.registered==1);
    // Following lookups reuse exactly the original object address.
    r=original_default_pool_0x271914(in);
    assert(r.status==S::CompletedExisting);
    assert(f.acquired==1);
    // Original 0x27A768 reads the SAME global as 0x27A4F0:
    // no dummy default-pool callback is required for a live slot.
    std::array<std::uint8_t,kOriginalOwnerObjectBytes> owner{};
    OriginalOwnerConstructorInputs0x27a768 constructor{};
    constructor.owner_arm_va=0x2000u;
    constructor.mapped_owner=owner.data();
    constructor.owner_bytes=owner.size();
    constructor.mapped_allocator_global_word=f.global_slot.data();
    constructor.mapped_allocator_global_bytes=f.global_slot.size();
    constructor.native_default_pool=&in;
    auto created=original_construct_resource_owner_0x27a768(constructor);
    assert(created.status==OriginalOwnerConstructorStatus0x27a768::Completed);
    assert(created.original_pool_arm_va==kVerifiedSingletonObjectVA);
    assert(get(owner.data()+0x824u)==kVerifiedSingletonObjectVA);
    assert(get(owner.data()+0x8d4u)==kVerifiedSingletonObjectVA);
    assert(get(owner.data()+0x8f4u)==kVerifiedSingletonObjectVA);
    // Mismatched global mapping must not silently bind a different
    // singleton as the original allocator.
    std::array<std::uint8_t,4> unrelated{};
    constructor.mapped_allocator_global_word=unrelated.data();
    created=original_construct_resource_owner_0x27a768(constructor);
    assert(created.status==OriginalOwnerConstructorStatus0x27a768::InvalidMemory);
    // Missing callbacks must not publish a non-constructed object.
    Fixture failing{};
    OriginalSingletonInputs other{
        kVerifiedSingletonGuardVA,failing.guard_and_object.data(),
        failing.guard_and_object.size(),0x4000u,0x5000u,
        acquire,release,register_destructor,&failing
    };
    failing.refuse=true;
    OriginalDefaultPoolInputs0x271914 missing{
        failing.global_slot.data(),failing.global_slot.size(),&other};
    r=original_default_pool_0x271914(missing);
    assert(r.status==S::OriginalSingletonBlocked);
    assert(get(failing.global_slot.data())==0u);
    missing.accessible_slot_bytes=3u;
    r=original_default_pool_0x271914(missing);
    assert(r.status==S::InvalidGlobalPointer);
    return 0;
}
