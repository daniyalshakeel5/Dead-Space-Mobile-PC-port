#pragma once
#include "ds_port/original_startup_registration.hpp"
#include "ds_port/original_native_resource_map.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>
namespace ds_port::original_runtime {
// Native x86-64 reconstruction of the *verified* original 0x284178
// registration path: precheck 0x2839C8, optional 0x283F5C action,
// copied first/second strings, original mapped table conversion,
// paired string insertion at 0x2837B4. No game strings shipped.
// The original 0x281DF4 allocator is represented by std::string
// instead of preserving its ARM32 memory-allocation ABI.
inline constexpr std::uint32_t kOriginalRegistrationCharacterGotVA=0x004909f4u;
inline constexpr std::size_t kOriginalRegistrationCharMapMinBytes=514;
struct OriginalRegistrationStringBytes {
    const std::uint8_t* bytes=nullptr;
    std::size_t accessible_bytes=0;
};
// Original r1 and r2 point to C strings. No source is guessed;
// callers must map original game virtual addresses to host bytes.
using OriginalRegistrationStringResolver=bool (*)(
    std::uint32_t original_arm_va,void* context,
    OriginalRegistrationStringBytes* mapped) noexcept;
struct OriginalRegistrationCharacterTable {
    // Original GOT [0x004909F4] -> pointer to 16-bit conversion table.
    const void* mapped_got_word=nullptr;
    std::size_t got_word_bytes=0;
    std::uint32_t original_table_arm_va=0;
    const std::uint8_t* mapped_table_bytes=nullptr;
    std::size_t mapped_table_size=0;
};
// Original 0x2839C8(r0=context+0x18, r1=source, r2=descriptor).
using OriginalRegistrationPrecheck=bool (*)(
    const OriginalRegistrationCall&,void* context,
    std::uint32_t* result) noexcept;
// Original 0x283F5C(r0=context+0x18,r1=original source) if precheck != 0.
using OriginalRegistrationMatchedAction=bool (*)(
    const OriginalRegistrationCall&,void* context) noexcept;
// Original 0x2837B4 receives r1=context+0x1C, paired copies of
// converted first string and unchanged second string, r3=0.
// Implementing the actual resource container remains further work.
using OriginalRegistrationPairInsert=bool (*)(
    const OriginalRegistrationCall&,std::uint32_t original_container_arm_va,
    std::string_view converted_source,std::string_view descriptor,
    void* context) noexcept;
struct OriginalNativeRegistrationServices {
    OriginalRegistrationStringResolver resolve_string=nullptr;
    OriginalRegistrationCharacterTable character_table{};
    OriginalRegistrationPrecheck original_0x2839c8=nullptr;
    OriginalRegistrationMatchedAction original_0x283f5c=nullptr;
    OriginalRegistrationPairInsert original_0x2837b4=nullptr;
    void* context=nullptr;
    // Native tree replaces the three unported ARM lookup/remove/insert
    // virtual implementations with their recovered semantic operation.
    // If present, this takes priority over the original callback chain.
    OriginalNativeResourceMap* native_resource_map=nullptr;
};
enum class OriginalNativeRegistrationStatus : std::uint8_t {
    InvalidOriginalArguments,
    MissingOriginalService,
    UnmappedOriginalString,
    UnterminatedOriginalString,
    InvalidOriginalCharacterTable,
    OriginalPrecheckRefused,
    OriginalMatchedActionRefused,
    OriginalInsertRefused,
    CompletedOriginalPath
};
OriginalNativeRegistrationStatus original_execute_resource_registration_0x284178(
    const OriginalRegistrationCall&,const OriginalNativeRegistrationServices&) noexcept;
} // namespace ds_port::original_runtime
