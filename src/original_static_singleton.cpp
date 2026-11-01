#include "ds_port/original_static_singleton.hpp"
namespace ds_port::original_runtime {
namespace {
std::uint32_t read32(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
void put32(std::uint8_t* p,std::uint32_t v) noexcept {
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(v>>(8*i));
}
}
OriginalSingletonResult original_initialize_static_0x27e560(
    const OriginalSingletonInputs& in) noexcept {
    const auto fail=[](OriginalSingletonStatus status) noexcept {
        return OriginalSingletonResult{status,0,false};
    };
    if(!in.guard_arm_va || in.guard_arm_va>0xffffffffu-4 ||
       !in.mapped_guard_and_object || in.mapped_bytes<0x18u)
        return fail(OriginalSingletonStatus::InvalidMemory);
    // ARM LDR r6,[guard]; ANDS r6,#1; BNE return object.
    const auto token=in.guard_arm_va+4u;
    auto* p=in.mapped_guard_and_object;
    if(read32(p)&1u)
        return {OriginalSingletonStatus::Completed,token,false};
    if(!in.guard_acquire||!in.guard_release||!in.register_atexit)
        return fail(OriginalSingletonStatus::MissingOriginalImports);
    if(!in.original_object_vtable_arm_va||!in.original_component_vtable_arm_va)
        return fail(OriginalSingletonStatus::MissingOriginalVtable);
    std::uint32_t acquired=0;
    if(!in.guard_acquire(p,in.context,&acquired))
        return fail(OriginalSingletonStatus::GuardAcquireRefused);
    // Original ARM CMP r0,#0; BEQ return object. Another thread may
    // already be constructing it; do not touch the object on zero.
    if(!acquired)
        return {OriginalSingletonStatus::Completed,token,false};
    // Original ARM 0x27E5B4..0x27E5CC:
    // STR r6,[guard+0x14] (r6=0),
    // STR(vtable+8),[guard+4],
    // STR(component vtable),[guard+8],
    // STRB 0,[guard+0xc], STR 0,[guard+0x10].
    put32(p+0x14,0u);
    put32(p+4,in.original_object_vtable_arm_va+8u);
    put32(p+8,in.original_component_vtable_arm_va);
    p[0x0c]=0;
    put32(p+0x10,0u);
    // Imported PLT at ARM 0x35A28 is guard release in the original
    // native static-initialization sequence.
    if(!in.guard_release(p,in.context))
        return fail(OriginalSingletonStatus::GuardReleaseRefused);
    // Imported PLT 0x3541C registers static object lifetime cleanup.
    if(!in.register_atexit(token,in.context))
        return fail(OriginalSingletonStatus::AtexitRefused);
    return {OriginalSingletonStatus::Completed,token,true};
}
bool original_static_singleton_base_provider(
    std::uint32_t callee,std::uint32_t,
    void* context,std::uint32_t* result) noexcept {
    if(callee!=0x0027e560u || !context || !result)return false;
    const auto* in=static_cast<const OriginalSingletonInputs*>(context);
    const auto r=original_initialize_static_0x27e560(*in);
    if(r.status!=OriginalSingletonStatus::Completed)return false;
    *result=r.original_object_arm_va;
    return true;
}

}
