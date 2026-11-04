#include "ds_port/original_receiver_mode.hpp"
namespace ds_port::original_runtime {
namespace {
std::uint32_t load32(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
void store32(std::uint8_t* p,std::uint32_t x) noexcept {
    for(unsigned n=0;n<4;++n)p[n]=std::uint8_t(x>>(8u*n));
}
}
OriginalReceiverModeResult0x26c410 original_set_receiver_mode_0x26c410(
    const OriginalReceiverModeInputs0x26c410& in,
    std::uint32_t opcode,std::uint32_t value) noexcept {
    using S=OriginalReceiverModeStatus0x26c410;
    auto ret=[](S s,std::uint32_t old=0,std::uint32_t now=0) noexcept {
        return OriginalReceiverModeResult0x26c410{s,old,now};
    };
    if(!in.owner_arm_va||!in.mapped_owner||in.owner_bytes<0x8bcu)
        return ret(S::InvalidOriginalOwner);
    auto* obj=static_cast<std::uint8_t*>(in.mapped_owner);
    if(opcode==2u) {
        const auto old=load32(obj+0x8b8u);
        store32(obj+0x8b8u,value);
        return ret(S::Completed,old,value);
    }
    if(opcode!=1u)
        return ret(S::Completed);
    const auto old=obj[0x8b5u];
    const std::uint32_t normalized=value?1u:0u;
    if(old==normalized)
        return ret(S::Completed,old,normalized);
    // Original 0x26C430: boolean reduction; 0x26C43C STRB,
    // followed by vtable+0x28 if the NEW value is zero. Require
    // the missing native reset service before modifying state.
    if(normalized==0u&&!in.native_reset_0x273a0c)
        return ret(S::MissingOriginalDisableReset,old,old);
    obj[0x8b5u]=static_cast<std::uint8_t>(normalized);
    if(normalized==0u &&
       !in.native_reset_0x273a0c(in.owner_arm_va,in.context)) {
        // A failing host service cannot advance the native receiver.
        // Restore mode; external reset-service side effects must be
        // transactional independently.
        obj[0x8b5u]=static_cast<std::uint8_t>(old);
        return ret(S::OriginalDisableResetRefused,old,old);
    }
    return ret(S::Completed,old,normalized);
}
} // namespace ds_port::original_runtime
