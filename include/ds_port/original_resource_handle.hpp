#pragma once
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
// ARM 0x0028A5F4..0x0028A640 proven from user's original APK.
// The function returns whether an existing resource/provider is ready,
// or executes one original virtual +0x50 call for provider mode 0.
struct OriginalResourceHandleInput {
    const void* mapped_handle;
    std::size_t handle_bytes;
    std::uint32_t original_provider_arm_va;
    const void* mapped_provider;
    std::size_t provider_bytes;
    std::uint32_t original_provider_vtable_arm_va;
    const void* mapped_provider_vtable;
    std::size_t provider_vtable_bytes;
};
// ARM virtual call: r0=provider, r1=1, r2=6, r3=1, stack0=0.
// The concrete +0x50 method is still a separate engine dependency.
struct OriginalResourceHandleVirtualCall {
    std::uint32_t original_provider_va;
    std::uint32_t original_method_va;
    std::uint32_t r1,r2,r3,stack0;
};
using OriginalResourceHandleVirtual=bool (*)(
    const OriginalResourceHandleVirtualCall&,void* context,
    std::uint32_t* original_r0) noexcept;
enum class OriginalResourceHandleStatus : std::uint8_t {
    InvalidOriginalMemory,UnmappedOriginalProvider,
    MissingOriginalVirtualMethod,OriginalVirtualRefused,Completed
};
struct OriginalResourceHandleResult {
    OriginalResourceHandleStatus status;
    std::uint32_t original_r0;
};
OriginalResourceHandleResult original_resource_ready_0x28a5f4(
    const OriginalResourceHandleInput&,
    OriginalResourceHandleVirtual virtual_call,
    void* callback_context) noexcept;
} // namespace ds_port::original_runtime
