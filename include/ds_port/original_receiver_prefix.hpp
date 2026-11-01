#pragma once
#include "ds_port/original_receiver_tree.hpp"
#include "ds_port/original_receiver_activation.hpp"
#include "ds_port/original_resource_registration.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ds_port::original_runtime {
// Verified first part of original 0x278018 receiver.
// The original owner's byte flags are +0x8b5 (mode) and +0x8b6
// (branch to UTF-16 path). Its original tree starts at owner+0x8bc.
// Only the real no-match return path is complete here. The matched
// branch invokes original owner/provider methods and remains blocked.
struct OriginalReceiverPrefixInputs0x278018 {
    std::uint32_t original_owner_arm_va=0;
    const void* mapped_owner=nullptr;
    std::size_t owner_accessible_bytes=0;
    OriginalRegistrationCharacterTable original_character_table{};
    OriginalTreeNodeResolver resolve_node=nullptr;
    OriginalReceiverWideResolver resolve_utf16=nullptr;
    void* context=nullptr;
    // Optional native 0x278E58 activation path for the original
    // owner+0x8B6==0 branch. Still gated by missing provider services.
    const OriginalReceiverActivationInputs0x278e58* native_activation=nullptr;
};
enum class OriginalReceiverPrefixStatus0x278018 : std::uint8_t {
    CompletedNoMatch,
    InvalidOriginalOwner, UnreconstructedAlternateMode,
    OriginalActivationBlocked,
    InvalidOriginalCharacterTable, InvalidOriginalText,
    LookupBlocked, UnreconstructedOwnerReset,
    UnreconstructedMatchedReceiver, AllocationBlocked
};
struct OriginalReceiverPrefixResult0x278018 {
    OriginalReceiverPrefixStatus0x278018 status;
    std::uint32_t original_tree_match_va;
    std::uint32_t original_r0;
};
OriginalReceiverPrefixResult0x278018
original_execute_receiver_prefix_0x278018(
    const OriginalReceiverPrefixInputs0x278018&,
    std::u16string_view record_text) noexcept;
} // namespace ds_port::original_runtime
