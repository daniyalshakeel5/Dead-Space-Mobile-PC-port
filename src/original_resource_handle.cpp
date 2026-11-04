#include "ds_port/original_resource_handle.hpp"
namespace ds_port::original_runtime {
namespace {
std::uint32_t read32(const void* memory) noexcept {
    const auto* p=static_cast<const std::uint8_t*>(memory);
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
}
OriginalResourceHandleResult original_resource_ready_0x28a5f4(
    const OriginalResourceHandleInput& in,
    OriginalResourceHandleVirtual original_virtual,
    void* context) noexcept {
    const auto fail=[](OriginalResourceHandleStatus s) noexcept {
        return OriginalResourceHandleResult{s,0};
    };
    if(!in.mapped_handle || in.handle_bytes<12)
        return fail(OriginalResourceHandleStatus::InvalidOriginalMemory);
    const auto* p=static_cast<const std::uint8_t*>(in.mapped_handle);
    const auto mode=read32(p+4);
    const auto provider=read32(p+8);
    // Original CMP [handle+4],#0 / BNE:
    // for nonzero mode, it returns bool(handle+8) without virtual calls.
    if(mode!=0)
        return {OriginalResourceHandleStatus::Completed,provider?1u:0u};
    // Original mode zero invokes a provider at handle+8:
    // r1=1, r2=6, r3=1, stack_arg0=0; vtable method +0x50.
    if(!provider || provider!=in.original_provider_arm_va||
       !in.mapped_provider||in.provider_bytes<4||
       !in.original_provider_vtable_arm_va||
       read32(in.mapped_provider)!=in.original_provider_vtable_arm_va||
       !in.mapped_provider_vtable||in.provider_vtable_bytes<0x54)
        return fail(OriginalResourceHandleStatus::UnmappedOriginalProvider);
    const auto* vt=static_cast<const std::uint8_t*>(in.mapped_provider_vtable);
    const auto method=read32(vt+0x50);
    if(!method || !original_virtual)
        return fail(OriginalResourceHandleStatus::MissingOriginalVirtualMethod);
    std::uint32_t r0=0;
    const OriginalResourceHandleVirtualCall call{
        provider,method,1u,6u,1u,0u};
    if(!original_virtual(call,context,&r0))
        return fail(OriginalResourceHandleStatus::OriginalVirtualRefused);
    return {OriginalResourceHandleStatus::Completed,r0};
}
}
