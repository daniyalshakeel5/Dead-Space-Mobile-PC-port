#include "ds_port/original_secondary_postprocess.hpp"
#include "ds_port/original_object_constructor.hpp"
namespace ds_port::original_runtime {
namespace {
void store32(std::uint8_t* p,std::uint32_t v) noexcept {
    for(unsigned i=0;i<4;++i)p[i]=static_cast<std::uint8_t>(v>>(i*8));
}
}
OriginalPostprocessStatus original_postprocess_0x288688_prefix(
    const OriginalPostprocessInputs& in) noexcept {
    if(!in.original_object_arm_address || !in.mapped_object_bytes ||
       in.accessible_object_bytes<0x24 || !in.initial_vtable_arm_address ||
       !in.first_setup_arg_arm_address || !in.second_setup_arg_arm_address ||
       !in.third_setup_arg_arm_address ||
       in.original_object_arm_address>0xffffffffu-0x14u)
        return OriginalPostprocessStatus::InvalidMemory;
    if(!in.call_original)return OriginalPostprocessStatus::MissingOriginalCallees;
    std::uint32_t result=0;
    if(!in.call_original(0x002828acu,in.original_object_arm_address,0u,0u,
                         in.callback_context,&result))
        return OriginalPostprocessStatus::CallRefused;
    auto* p=static_cast<std::uint8_t*>(in.mapped_object_bytes);
    store32(p,in.initial_vtable_arm_address);
    if(!in.call_original(0x0027fb28u,in.original_object_arm_address+0x14u,
                         in.first_setup_arg_arm_address,0u,
                         in.callback_context,&result))
        return OriginalPostprocessStatus::CallRefused;
    if(!in.call_original(0x0027fa74u,in.original_object_arm_address+0x14u,
                         in.second_setup_arg_arm_address,in.third_setup_arg_arm_address,
                         in.callback_context,&result))
        return OriginalPostprocessStatus::CallRefused;
    store32(p+0x20,result);
    return OriginalPostprocessStatus::Completed;
}
OriginalPostprocessStatus original_postprocess_0x288688_native_constructor(
    const OriginalPostprocessInputs& in) noexcept {
    if(!in.original_object_arm_address || !in.mapped_object_bytes ||
       in.accessible_object_bytes<0x24 ||
       in.initial_vtable_arm_address!=kOriginal288688OverrideVtableVA ||
       !in.first_setup_arg_arm_address || !in.second_setup_arg_arm_address ||
       !in.third_setup_arg_arm_address ||
       in.original_object_arm_address>0xffffffffu-0x14u)
        return OriginalPostprocessStatus::InvalidMemory;
    if(!in.call_original) return OriginalPostprocessStatus::MissingOriginalCallees;
    // 0x288690 BL 0x2828AC: complete original constructor, now native.
    if(!original_construct_0x2828ac(in.mapped_object_bytes,
        in.accessible_object_bytes,kOriginal2828acConstructorVtableVA))
        return OriginalPostprocessStatus::InvalidMemory;
    auto* p=static_cast<std::uint8_t*>(in.mapped_object_bytes);
    // 0x2886A8: original ARM overrides the initial constructor's vtable.
    store32(p,kOriginal288688OverrideVtableVA);
    std::uint32_t result=0;
    if(!in.call_original(0x0027fb28u,in.original_object_arm_address+0x14u,
        in.first_setup_arg_arm_address,0u,in.callback_context,&result))
        return OriginalPostprocessStatus::CallRefused;
    if(!in.call_original(0x0027fa74u,in.original_object_arm_address+0x14u,
        in.second_setup_arg_arm_address,in.third_setup_arg_arm_address,
        in.callback_context,&result))
        return OriginalPostprocessStatus::CallRefused;
    // 0x2886D0 STR r0,[r4,#0x20]
    store32(p+0x20,result);
    return OriginalPostprocessStatus::Completed;
}

}
