#include "ds_port/original_resource_owner_bind.hpp"

namespace ds_port::original_runtime {
namespace {
std::uint32_t load32(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
}
OriginalOwnerBindResult0x278778 original_bind_resource_owner_0x278778(
    const OriginalOwnerBindInputs0x278778& in) noexcept {
    using S=OriginalOwnerBindStatus0x278778;
    using A=OriginalReceiverActivationStatus0x278e58;
    auto result=[](S status,A activation,std::uint32_t provider) noexcept {
        return OriginalOwnerBindResult0x278778{
            status,activation,provider};
    };
    if(!in.owner_arm_va || in.owner_arm_va>0xfffff72bu ||
       !in.mapped_owner || in.owner_bytes<0x8d4u)
        return result(S::InvalidOriginalOwner,A::InvalidOwner,0);
    auto* owner=static_cast<std::uint8_t*>(in.mapped_owner);
    const auto provider=load32(owner+0x8acu);
    // Original 0x278784 LDRB +0x8B6 / BEQ 0x278AAC.
    // When already active, it builds a list of provider-backed
    // resource entries and invokes callback r1. No native substitute
    // for those original object/allocator methods is provided.
    if(owner[0x8b6u]!=0)
        return result(S::UnreconstructedActiveTreeBinding,
                      A::InvalidOwner,provider);
    // Original 0x278AAC..0x278AB8 loads [owner.vtable+0x64]
    // and dispatches owner activation with r1=1. We call the
    // independently reconstructed 0x278E58 native code.
    const auto activated=original_execute_receiver_activation_0x278e58({
        in.owner_arm_va,owner,in.owner_bytes,
        in.release_original_tree,in.context,
        in.query_provider_mode_0x14,1u,in.native_scratch
    });
    if(activated.status==A::CompletedEmptyProviderReset) {
        // Original 0x2790E0: activation returns r0=0.
        // 0x278ABC tests r0 and branches directly to the
        // bind routine cleanup/return at 0x278A54.
        // This is NO resource and NO engine-ready signal.
        return result(S::ReachedOriginalNullProviderExit,
                      activated.status,0);
    }
    if(activated.status==A::CompletedProviderRejected)
        // ARM 0x278EDC returns r0=0 after owner+0x24 rejects
        // the provider, so 0x278AC4 skips binding and exits.
        return result(S::ReachedOriginalProviderRejectedExit,
                      activated.status,provider);
    if(activated.status==A::UnreconstructedProviderActivation)
        return result(S::UnreconstructedProviderBinding,
                      activated.status,provider);
    return result(S::OriginalTreeActivationBlocked,
                  activated.status,provider);
}
} // namespace ds_port::original_runtime
