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
