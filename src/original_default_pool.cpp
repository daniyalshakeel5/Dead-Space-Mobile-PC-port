#include "ds_port/original_default_pool.hpp"

namespace ds_port::original_runtime {
namespace {
std::uint32_t read32(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
void store32(std::uint8_t* p,std::uint32_t value) noexcept {
    for(unsigned n=0;n<4;++n)p[n]=std::uint8_t(value>>(n*8u));
}
}
OriginalDefaultPoolResult0x271914 original_default_pool_0x271914(
    const OriginalDefaultPoolInputs0x271914& in) noexcept {
    using S=OriginalDefaultPoolStatus0x271914;
    if(!in.mapped_global_pointer_slot || in.accessible_slot_bytes<4u)
        return {S::InvalidGlobalPointer,0u};
    auto* slot=static_cast<std::uint8_t*>(in.mapped_global_pointer_slot);
    const auto already=read32(slot);
    // Original 0x271920 LDR r0,[global]; CMP #0; BNE return.
    if(already)
        return {S::CompletedExisting,already};
    // Original 0x271930 calls 0x27E560 only on the empty slot.
    if(!in.native_singleton_0x27e560)
        return {S::MissingOriginalSingleton,0u};
    const auto constructed=original_initialize_static_0x27e560(
        *in.native_singleton_0x27e560);
    if(constructed.status!=OriginalSingletonStatus::Completed ||
       !constructed.original_object_arm_va ||
       !constructed.constructed)
        return {S::OriginalSingletonBlocked,0u};
    // Original 0x271934 STR r0,[r4]. Preserve a 32-bit original VA
    // token, never place a host pointer in the mapped ARM slot.
    store32(slot,constructed.original_object_arm_va);
    return {S::CompletedNewSingleton,constructed.original_object_arm_va};
}
} // namespace ds_port::original_runtime
