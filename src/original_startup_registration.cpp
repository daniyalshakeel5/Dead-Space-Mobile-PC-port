#include "ds_port/original_startup_registration.hpp"
namespace ds_port::original_runtime {
namespace {
std::uint32_t load_u32(const void* ptr) noexcept {
    const auto* b=static_cast<const std::uint8_t*>(ptr);
    return std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|
           (std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);
}
}
OriginalRegistrationStatus original_register_stage_resources_0x27fd7c(
    const OriginalRegistrationInputs& in) noexcept {
    if(!in.original_context_arm_va ||
       in.original_context_arm_va>0xffffffe7u)
        return OriginalRegistrationStatus::InvalidOriginalContext;
    if(!in.resolve_original_global)
        return OriginalRegistrationStatus::MissingOriginalGlobalResolver;
    if(!in.invoke_original_0x284178)
        return OriginalRegistrationStatus::MissingOriginalRegistrationHelper;
    // The real original function does not branch between registrations.
    // Each BL 0x284178 is followed by the next call; final entry is a
    // tail B 0x284178. We stop *only* when the unresolved original
    // callee rejects execution, never silently substitute its effects.
    for(const auto& item:kOriginalRegistrationSequence0x27fd7c) {
        OriginalRegistrationMappedGlobal global{};
        if(!in.resolve_original_global(
            item.global_slot_arm_va,in.callback_context,&global))
            return OriginalRegistrationStatus::UnmappedOriginalGlobal;
        if(!global.global_arm_va ||
           !global.got_slot_host_bytes||global.got_slot_bytes<4||
           !global.global_host_bytes||global.global_bytes<4||
           load_u32(global.got_slot_host_bytes)!=global.global_arm_va)
            return OriginalRegistrationStatus::InvalidOriginalGotPointer;
        const OriginalRegistrationCall args{
            kOriginalStartupRegistrationHelperVA,
            item.callsite_arm_va,
            in.original_context_arm_va+0x18u,
            load_u32(global.global_host_bytes),
            item.descriptor_arm_va,
            item.tail_call
        };
        if(!in.invoke_original_0x284178(args,in.callback_context))
            return OriginalRegistrationStatus::OriginalRegistrationRefused;
    }
    return OriginalRegistrationStatus::CompletedOriginalSequence;
}
} // namespace ds_port::original_runtime
