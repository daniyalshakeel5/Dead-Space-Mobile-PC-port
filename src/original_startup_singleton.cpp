#include "ds_port/original_startup_singleton.hpp"
#include <mutex>
namespace ds_port::original_runtime {
namespace {
std::mutex guard_mutex;
std::uint32_t get(const void* p) noexcept {
    const auto* b=static_cast<const std::uint8_t*>(p);
    return std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|
           (std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);
}
void set(std::uint8_t* p,std::uint32_t v) noexcept {
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(v>>(8*i));
}
}
OriginalSingletonStatus original_initialize_singleton_0x27e560(
    const OriginalSingletonMemory& s,std::uint32_t* out) noexcept {
    if(!out||!s.guard_and_object||s.bytes<0x18)
        return OriginalSingletonStatus::InvalidMemory;
    std::lock_guard<std::mutex> lock(guard_mutex);
    auto* p=static_cast<std::uint8_t*>(s.guard_and_object);
    // __cxa_guard_acquire/release from actual ARM ELF 0x27E560.
    if(get(p)&1u) {
        *out=kSingletonObjectVA;
        return OriginalSingletonStatus::AlreadyInitialized;
    }
    if(!s.field_source_got||!s.table_got||!s.destructor_got||
       !s.dso_got||!s.source_word||!s.source_arm_va||
       get(s.field_source_got)!=s.source_arm_va||
       !get(s.table_got)||!get(s.destructor_got)||!get(s.dso_got)||
       get(s.table_got)>0xfffffff7u)
        return OriginalSingletonStatus::InvalidMemory;
    if(!s.register_destructor)
        return OriginalSingletonStatus::MissingRegistration;
    // ARM STR at guard offsets +4/+8, zero at +C/+10/+14.
    set(p+0x14,0);
    set(p+4,get(s.table_got)+8u);
    set(p+8,get(s.source_word));
    p[0xc]=0;set(p+0x10,0);
    set(p,get(p)|1u);
    // PLT 0x3541C is __aeabi_atexit, not __cxa_atexit.
    if(!s.register_destructor(kSingletonObjectVA,get(s.destructor_got),
                              get(s.dso_got),s.context))
        return OriginalSingletonStatus::RegistrationRefused;
    *out=kSingletonObjectVA;
    return OriginalSingletonStatus::ConstructedAndRegistered;
}
bool original_singleton_base_provider_0x27e560(
    std::uint32_t target,std::uint32_t,void* context,
    std::uint32_t* out) noexcept {
    if(target!=0x0027e560u||!context)return false;
    const auto result=original_initialize_singleton_0x27e560(
        *static_cast<const OriginalSingletonMemory*>(context),out);
    return result==OriginalSingletonStatus::AlreadyInitialized||
           result==OriginalSingletonStatus::ConstructedAndRegistered;
}
}
