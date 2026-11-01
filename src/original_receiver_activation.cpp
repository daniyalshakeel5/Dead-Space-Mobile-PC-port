#include "ds_port/original_receiver_activation.hpp"

namespace ds_port::original_runtime {
namespace {
std::uint32_t read32(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
void store32(std::uint8_t* p,std::uint32_t v) noexcept {
    for(unsigned k=0;k<4;++k)p[k]=std::uint8_t(v>>(k*8u));
}
}
OriginalReceiverActivationResult0x278e58
original_execute_receiver_activation_0x278e58(
    const OriginalReceiverActivationInputs0x278e58& in) noexcept {
    using S=OriginalReceiverActivationStatus0x278e58;
    auto fail=[](S s,std::uint32_t root=0,
                 std::uint32_t provider=0) noexcept {
        return OriginalReceiverActivationResult0x278e58{s,root,provider};
    };
    if(!in.mapped_owner||!in.owner_arm_va||
       in.owner_arm_va>0xfffff72bu||in.owner_bytes<0x8d4u)
        return fail(S::InvalidOwner);
    auto* owner=static_cast<std::uint8_t*>(in.mapped_owner);
    const std::uint32_t root=read32(owner+0x8c8u);
    const std::uint32_t provider=read32(owner+0x8acu);
    // ARM 0x278E74..0x278E7C: BL 0x273570(owner+0x8BC,root).
    // For an empty original tree the destructor has no node releases.
    if(root) {
        if(!in.release_existing_tree)
            return fail(S::MissingOriginalTreeDestructor,root,provider);
        if(!in.release_existing_tree(in.owner_arm_va+0x8bcu,
                                     root,in.context))
            return fail(S::OriginalTreeReleaseRefused,root,provider);
    }
    // ARM 0x278E80..0x278EA4 resets the original tree and two
    // bookkeeping words before provider dispatch. Preserve exact
    // original ARM32 pointer provenance, not host addresses.
    if(in.owner_arm_va>0xffffffffu-0x8c0u)
        return fail(S::InvalidOwner,root,provider);
    const auto sentinel=in.owner_arm_va+0x8c0u;
    store32(owner+0x8c4u,sentinel);
    owner[0x8b6u]=0u;
    store32(owner+0x8c0u,sentinel);
    store32(owner+0x8c8u,0u);
    owner[0x8ccu]=0u;
    store32(owner+0x8d0u,0u);
    // ARM 0x278EA8 BEQ 0x2790E0 for a null provider. The r0
    // return is not a demonstrated engine-ready result; only
    // the state stores and branch are considered reconstructed.
    if(!provider)
        return fail(S::CompletedEmptyProviderReset,root,provider);
    // The nonzero provider branch invokes original virtual +0x14,
    // the owner's +0x24, and +0x70, plus more allocations.
    return fail(S::UnreconstructedProviderActivation,root,provider);
}
} // namespace ds_port::original_runtime
