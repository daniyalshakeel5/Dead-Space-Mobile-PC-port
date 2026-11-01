#pragma once
#include "ds_port/original_resource_owner.hpp"
#include "ds_port/original_owner_byte_line.hpp"
#include "ds_port/original_owner_line_utf8.hpp"
#include "ds_port/original_owner_file_line.hpp"
#include "ds_port/original_receiver_mode.hpp"
#include "ds_port/original_receiver_activation.hpp"
#include "ds_port/original_receiver_prefix.hpp"
#include "ds_port/original_resource_owner_bind.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ds_port::original_runtime {

// Owned original ARM32 resource-owner state for a native C++ engine.
// All fields of original 0x27A768 are stored in a properly owned
// host buffer and exposed to already reconstructed original services.
// Original VAs remain virtual address tokens; never dereference them.
// Native first two original loader calls at ARM 0x286918:
// 0x27A768(owner=stack local, provider=r1, pool=[loader+4]);
// 0x26C410(owner,opcode=1,value=1).
// Subsequent original 0x278778 binding is not yet reconstructed.
enum class OriginalLoaderPrefixStatus0x286918:std::uint8_t {
    InvalidLoaderContext,ConstructorBlocked,ModeBlocked,
    ReachedUnreconstructedOriginalBind,
    ReachedOriginalNullProviderExit,ReachedOriginalProviderRejectedExit,
    OriginalBindingBlocked
};
struct OriginalLoaderPrefixResult0x286918 {
    OriginalLoaderPrefixStatus0x286918 status;
    OriginalOwnerConstructorStatus0x27a768 constructor_status;
    OriginalReceiverModeStatus0x26c410 mode_status;
    OriginalOwnerBindStatus0x278778 binding_status=
        OriginalOwnerBindStatus0x278778::InvalidOriginalOwner;
};
class OriginalNativeResourceEngine {
public:
    explicit OriginalNativeResourceEngine(std::uint32_t arm_owner_va) noexcept;
    OriginalOwnerConstructorResult0x27a768 initialize(
        const OriginalOwnerConstructorInputs0x27a768& services) noexcept;
    OriginalLoaderPrefixResult0x286918 begin_loader_0x286918(
        const void* mapped_original_loader_context,
        std::size_t accessible_original_loader_bytes,
        std::uint32_t original_provider_arm_va,
        const OriginalOwnerConstructorInputs0x27a768& services) noexcept;
    OriginalOwnerBindResult0x278778 bind_original_owner_0x278778(
        OriginalActivationTreeClear release_tree=nullptr,
        void* context=nullptr,
        OriginalActivationProviderMode14 query_provider_mode=nullptr,
        const OriginalReceiverActivationScratch0x278e58* scratch=nullptr) noexcept;
    OriginalReceiverModeResult0x26c410 set_mode(
        std::uint32_t opcode,std::uint32_t value,
        OriginalReceiverDisableReset original_reset=nullptr,
        void* context=nullptr) noexcept;
    OriginalReceiverActivationResult0x278e58 activate(
        OriginalActivationTreeClear original_tree_release=nullptr,
        void* context=nullptr) noexcept;
    // Process caller-supplied original resource text after owner is
    // constructed. This only extracts a mode-8 line; actual owner
    // +0x70 postprocessing and provider virtuals remain incomplete.
    OriginalNativeOwnerStreamResult read_resource_text_line_0x276d18(
        OriginalNativeByteSource& source,
        OriginalResourceByteString& output) const noexcept;
    // Recover mode-8 resource text as owned UTF-16, using the game's
    // distinct 0x26D5FC byte decoder. Does not imply a loaded scene.
    OriginalOwnerWideLineResult read_resource_wide_line_0x276d18(
        OriginalNativeByteSource& source,
        OriginalNativeUtf16String& output) const noexcept;
    // Explicit native Windows/Linux HOST file adapter. The original
    // 0x27A4F0 embedded provider must be constructed and selected.
    // Callers own their fd. This is not the original game's file-open
    // subroutine, and cannot boot or register assets by itself.
    bool bind_user_asset_fd(int native_host_fd) noexcept;
    OriginalOwnerWideLineResult read_bound_asset_line_0x276d18(
        int native_host_fd,OriginalNativeUtf16String& output) noexcept;
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
