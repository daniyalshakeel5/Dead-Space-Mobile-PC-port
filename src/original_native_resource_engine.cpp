#include "ds_port/original_native_resource_engine.hpp"

namespace ds_port::original_runtime {
OriginalNativeResourceEngine::OriginalNativeResourceEngine(
    std::uint32_t arm_owner_va) noexcept : owner_va_(arm_owner_va) {}

OriginalOwnerConstructorResult0x27a768
OriginalNativeResourceEngine::initialize(
    const OriginalOwnerConstructorInputs0x27a768& input) noexcept {
    if(constructed_)
        return {OriginalOwnerConstructorStatus0x27a768::InvalidMemory,0,0};
    auto inputs=input;
    inputs.owner_arm_va=owner_va_;
    inputs.mapped_owner=bytes_.data();
    inputs.owner_bytes=bytes_.size();
    const auto result=original_construct_resource_owner_0x27a768(inputs);
    if(result.status==OriginalOwnerConstructorStatus0x27a768::Completed)
        constructed_=true;
    return result;
}
OriginalLoaderPrefixResult0x286918
OriginalNativeResourceEngine::begin_loader_0x286918(
    const void* original_context,std::size_t context_bytes,
    std::uint32_t provider,
    const OriginalOwnerConstructorInputs0x27a768& services) noexcept {
    using S=OriginalLoaderPrefixStatus0x286918;
    using CS=OriginalOwnerConstructorStatus0x27a768;
    using MS=OriginalReceiverModeStatus0x26c410;
    if(!original_context||context_bytes<8u||constructed_)
        return {S::InvalidLoaderContext,CS::InvalidMemory,
                MS::InvalidOriginalOwner};
    const auto* p=static_cast<const std::uint8_t*>(original_context);
    // Original 0x286934 LDR r2,[r5,#4], BL 0x27A768.
    const auto pool=std::uint32_t(p[4])|(std::uint32_t(p[5])<<8)|
        (std::uint32_t(p[6])<<16)|(std::uint32_t(p[7])<<24);
    auto bind=services;
    bind.original_pool_arm_va=pool;
    bind.original_provider_arm_va=provider;
    const auto constructed=initialize(bind);
    if(constructed.status!=CS::Completed)
        return {S::ConstructorBlocked,constructed.status,
                MS::InvalidOriginalOwner};
    // Original ARM 0x286944..0x286950 sets r1=1,r2=1 and
    // dispatches 0x26C410(owner,1,1) after constructor.
    const auto configured=set_mode(1u,1u);
    if(configured.status!=MS::Completed)
        return {S::ModeBlocked,constructed.status,configured.status};
    // Original next call is 0x278778(owner,
    //   PC-relative original literal, loader+8). Its provider /
    // resource binding is NOT implemented, so do not claim ready.
    return {S::ReachedUnreconstructedOriginalBind,
            constructed.status,configured.status};
}

OriginalReceiverModeResult0x26c410
OriginalNativeResourceEngine::set_mode(
    std::uint32_t opcode,std::uint32_t value,
    OriginalReceiverDisableReset reset,void* context) noexcept {
    if(!constructed_)
        return {OriginalReceiverModeStatus0x26c410::InvalidOriginalOwner,0,0};
    return original_set_receiver_mode_0x26c410({
        owner_va_,bytes_.data(),bytes_.size(),reset,context
    },opcode,value);
}
OriginalReceiverActivationResult0x278e58
OriginalNativeResourceEngine::activate(
    OriginalActivationTreeClear release,void* context) noexcept {
    if(!constructed_)
        return {OriginalReceiverActivationStatus0x278e58::InvalidOwner,0,0};
    return original_execute_receiver_activation_0x278e58({
        owner_va_,bytes_.data(),bytes_.size(),release,context
    });
}
OriginalReceiverPrefixResult0x278018
OriginalNativeResourceEngine::process_receiver_prefix(
    std::u16string_view record,
    OriginalRegistrationCharacterTable character_table,
    OriginalTreeNodeResolver node_resolver,
    OriginalReceiverWideResolver wide_resolver,
    void* node_context,
    OriginalActivationTreeClear original_tree_release) noexcept {
    if(!constructed_)
        return {OriginalReceiverPrefixStatus0x278018::InvalidOriginalOwner,0,0};
    // Native receiver activation can reset an empty original tree;
    // any nontrivial original provider remains a required service.
    OriginalReceiverActivationInputs0x278e58 activation{
        owner_va_,bytes_.data(),bytes_.size(),original_tree_release,
        node_context
    };
    OriginalReceiverPrefixInputs0x278018 receiver{
        owner_va_,bytes_.data(),bytes_.size(),
        character_table,node_resolver,wide_resolver,node_context,&activation
    };
    return original_execute_receiver_prefix_0x278018(receiver,record);
}
} // namespace ds_port::original_runtime
