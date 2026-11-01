#pragma once
#include "ds_port/original_resource_owner.hpp"
#include "ds_port/original_receiver_mode.hpp"
#include "ds_port/original_receiver_activation.hpp"
#include "ds_port/original_receiver_prefix.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ds_port::original_runtime {

// Owned original ARM32 resource-owner state for a native C++ engine.
// All fields of original 0x27A768 are stored in a properly owned
// host buffer and exposed to already reconstructed original services.
// Original VAs remain virtual address tokens; never dereference them.
class OriginalNativeResourceEngine {
public:
    explicit OriginalNativeResourceEngine(std::uint32_t arm_owner_va) noexcept;
    OriginalOwnerConstructorResult0x27a768 initialize(
        const OriginalOwnerConstructorInputs0x27a768& services) noexcept;
    OriginalReceiverModeResult0x26c410 set_mode(
        std::uint32_t opcode,std::uint32_t value,
        OriginalReceiverDisableReset original_reset=nullptr,
        void* context=nullptr) noexcept;
    OriginalReceiverActivationResult0x278e58 activate(
        OriginalActivationTreeClear original_tree_release=nullptr,
        void* context=nullptr) noexcept;
    OriginalReceiverPrefixResult0x278018 process_receiver_prefix(
        std::u16string_view record,
        OriginalRegistrationCharacterTable character_table,
        OriginalTreeNodeResolver node_resolver,
        OriginalReceiverWideResolver wide_resolver,
        void* node_context=nullptr,
        OriginalActivationTreeClear original_tree_release=nullptr) noexcept;
    const std::uint8_t* mapped_bytes() const noexcept {return bytes_.data();}
    std::size_t mapped_size() const noexcept {return bytes_.size();}
    std::uint32_t original_owner_va() const noexcept {return owner_va_;}
    bool constructed() const noexcept {return constructed_;}
private:
    std::array<std::uint8_t,kOriginalOwnerObjectBytes> bytes_{};
    std::uint32_t owner_va_;
    bool constructed_=false;
};
} // namespace ds_port::original_runtime
