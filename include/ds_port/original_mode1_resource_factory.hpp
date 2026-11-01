#pragma once
#include "ds_port/original_resource_handle.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>
namespace ds_port::original_runtime {
// Original vtable proven from 0x28B3A8 PC-relative literal and
// 0x28B3B4/0x28B3B8 arithmetic on user-confirmed original APK.
// GitHub Actions run 41326833766.
inline constexpr std::uint32_t kOriginalMode1HandleVtable=0x00489b20u;
inline constexpr std::size_t kOriginalMode1HandleBytes=16u;
// Actual original 0x28A718 performs resource resolution, dynamically
// allocates memory and calls numerous engine services. Until translated,
// this callback MUST be a real implementation rather than fake success.
using OriginalMode1ResourceResolve=bool (*)(
    std::uint32_t original_owner_arm_va,
    std::string_view original_name,
    void* context,std::uint32_t* original_resource_arm_va) noexcept;
struct OriginalMode1FactoryInputs {
    void* mapped_handle=nullptr;
    std::size_t handle_bytes=0;
    OriginalMode1ResourceResolve original_0x28a718=nullptr;
    void* context=nullptr;
};
enum class OriginalMode1FactoryStatus:std::uint8_t {
    InvalidMemory,MissingOriginalResolver,
    OriginalResolverRefused,CompletedOriginalMode1Constructor
};
struct OriginalMode1FactoryResult {
    OriginalMode1FactoryStatus status;
    OriginalResourceHandleInput handle;
};
// Native constructor for the mode=1 branch of original 0x28B3A4.
// The original string copies/temporary objects use host std::string_view;
// original 0x28AAB4 parse and 0x28A718 resolution are still external.
// No provider vtable +0x50 is called for this mode.
OriginalMode1FactoryResult original_construct_mode1_handle_0x28b3a4(
    const OriginalMode1FactoryInputs&,
    std::uint32_t original_owner_arm_va,
    std::string_view original_name) noexcept;
// Original 0x28A4F8 destructor when +8 is already null. In this exact
// mode-1 case no virtual provider release is invoked; vtable resets to
// the same 0x00489B20 and resource pointer remains zero.
enum class OriginalUnreadyHandleStatus:std::uint8_t {
    InvalidMemory,WrongOriginalMode,UnreconstructedNonNullResource,
    CompletedNullResourceCleanup
};
OriginalUnreadyHandleStatus original_cleanup_unready_mode1_0x28a4f8(
    const OriginalResourceHandleInput&) noexcept;
} // namespace ds_port::original_runtime
