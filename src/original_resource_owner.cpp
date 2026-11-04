#include "ds_port/original_resource_owner.hpp"
#include <cstdint>

namespace ds_port::original_runtime {
namespace {
std::uint32_t load32(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
        (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
void store32(std::uint8_t* p,std::uint32_t x) noexcept {
    for(unsigned k=0;k<4;++k)p[k]=std::uint8_t(x>>(8u*k));
}
}
OriginalOwnerConstructorResult0x27a768
original_construct_resource_owner_0x27a768(
    const OriginalOwnerConstructorInputs0x27a768& in) noexcept {
    using S=OriginalOwnerConstructorStatus0x27a768;
    auto blocked=[](S s,std::uint32_t pool=0,
                    std::uint32_t provider=0) noexcept {
        return OriginalOwnerConstructorResult0x27a768{s,pool,provider};
    };
    // Native host mapping cannot wrap 32-bit ARM pointers. The last
    // original word written is at owner+0x8F8.
    if(!in.owner_arm_va||in.owner_arm_va>0xfffff707u||
       !in.mapped_owner||in.owner_bytes<kOriginalOwnerObjectBytes||
       !in.mapped_allocator_global_word||
       in.mapped_allocator_global_bytes<4u)
        return blocked(S::InvalidMemory);
    // Embedded original 0x27A4F0 obtains an already-present BSS
    // singleton through original GOT 0x49BCA8, or calls 0x27E560.
    // We require the real singleton pointer here and NEVER fabricate it.
    const auto* native_pool=in.native_default_pool;
    if(native_pool &&
       (static_cast<const void*>(native_pool->mapped_global_pointer_slot)!=
            in.mapped_allocator_global_word ||
        native_pool->accessible_slot_bytes<4u))
        return blocked(S::InvalidMemory);
    auto allocator=load32(
        static_cast<const std::uint8_t*>(
            in.mapped_allocator_global_word));
    // Original 0x27A4F0 resolves the same original 0x49BCA8 slot
    // used by 0x271914. When absent, both call 0x27E560.
    // Never supply a stand-in pointer: native_singleton must contain
    // the independently recovered guard/object/GOT state.
    if(!allocator && native_pool) {
        const auto acquired=original_default_pool_0x271914(*native_pool);
        if(acquired.status==OriginalDefaultPoolStatus0x271914::CompletedNewSingleton ||
           acquired.status==OriginalDefaultPoolStatus0x271914::CompletedExisting)
            allocator=acquired.original_r0;
    }
    if(!allocator)return blocked(S::MissingOriginalAllocator);
    auto pool=in.original_pool_arm_va;
    if(!pool) {
        if(native_pool) {
            const auto resolved=original_default_pool_0x271914(*native_pool);
            if(resolved.status!=OriginalDefaultPoolStatus0x271914::CompletedExisting &&
               resolved.status!=OriginalDefaultPoolStatus0x271914::CompletedNewSingleton)
                return blocked(S::OriginalDefaultPoolRefused);
            pool=resolved.original_r0;
        } else {
            if(!in.default_pool_0x271914)
                return blocked(S::MissingDefaultPool);
            if(!in.default_pool_0x271914(
                0x00271914u,in.context,&pool)||!pool)
                return blocked(S::OriginalDefaultPoolRefused);
        }
    }
    std::uint32_t provider_r0=0;
    if(in.original_provider_arm_va) {
        // If the provider IS the owner's embedded +0x804 object,
        // original 0x27A4F0 constructs its +0x90 mode word as zero.
        // Its verified +0x14 target 0x26C3C4 simply loads that
        // word. No external callback is needed for this exact
        // native owner/provider combination.
        const bool embedded=
            in.original_provider_arm_va==in.owner_arm_va+0x804u;
        if(!embedded) {
            if(!in.provider_flags_0x14)
                return blocked(S::MissingOriginalProviderMethod,pool,
                               in.original_provider_arm_va);
            if(!in.provider_flags_0x14(
                in.original_provider_arm_va,0x14u,in.context,&provider_r0))
                return blocked(S::OriginalProviderRefused,pool,
                               in.original_provider_arm_va);
        }
    }
    // All potentially failing external calls have succeeded. The
    // following writes translate literal stores in original ARM
    // 0x27A768 and 0x27A4F0, not guessed C++ engine behavior.
    auto* owner=static_cast<std::uint8_t*>(in.mapped_owner);
    const auto embedded_arm_va=in.owner_arm_va+0x804u;
    auto* embedded=owner+0x804u;
    store32(owner,kOriginalOwnerVtable0x27a768);
    // Embedded original 0x27A4F0 receives r1=0. In that branch it
    // sets three nested native string/tree sentinels without copying
    // an input name; no original 0x2797D8 call is reached.
    store32(embedded,kOriginalEmbeddedVtable0x27a4f0);
    store32(embedded+0x04u,0);
    store32(embedded+0x10u,0xffffffffu);
    store32(embedded+0x1cu,embedded_arm_va+0x8cu);
    store32(embedded+0x20u,allocator);
    store32(embedded+0x14u,embedded_arm_va+0x2cu);
    store32(embedded+0x98u,0);
    store32(embedded+0xa0u,0xfffffffeu);
    store32(embedded+0x24u,0);
    store32(embedded+0x28u,embedded_arm_va+0x2cu);
    store32(embedded+0x18u,embedded_arm_va+0x2cu);
    embedded[0x2cu]=0;
    store32(embedded+0x8cu,0);
    store32(embedded+0x90u,0);
    store32(embedded+0x94u,0);

    // Native ARM 0x27A794..0x27A7BC.
    store32(owner+0x8b0u,8u);
    owner[0x8b5u]=1u;
    store32(owner+0x8b8u,0);
    store32(owner+0x8acu,in.original_provider_arm_va);
    owner[0x8b4u]=static_cast<std::uint8_t>(
        (provider_r0>>1u)&1u);
    owner[0x8b6u]=0;

    // ARM 0x27A7D0..0x27A81C sets up two tree headers,
    // the original allocator/pool fields, and a blank string.
    store32(owner+0x8d4u,pool);
    store32(owner+0x8d8u,0);
    const auto first=in.owner_arm_va+0x8c0u;
    store32(owner+0x8c4u,first);
    store32(owner+0x8c8u,0);
    owner[0x8ccu]=0;
    store32(owner+0x8d0u,0);
    store32(owner+0x8c0u,first);
    store32(owner+0x8f4u,pool);
    const auto second=in.owner_arm_va+0x8e0u;
    store32(owner+0x8e4u,second);
    owner[4]=0;owner[5]=0;
    store32(owner+0x8e8u,0);
    owner[0x8ecu]=0;
    store32(owner+0x8f0u,0);
    store32(owner+0x8f8u,0);
    store32(owner+0x8e0u,second);
    return {S::Completed,pool,in.original_provider_arm_va};
}
} // namespace ds_port::original_runtime
