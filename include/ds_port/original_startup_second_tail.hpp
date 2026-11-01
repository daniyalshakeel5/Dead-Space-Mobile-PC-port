#pragma once
#include "ds_port/original_allocator_call.hpp"
#include "ds_port/original_secondary_pipeline.hpp"
#include "ds_port/original_startup_first.hpp"
#include "ds_port/original_startup_object34.hpp"
#include "ds_port/original_startup_object58.hpp"
#include "ds_port/original_static_singleton.hpp"
#include "ds_port/original_startup_registration.hpp"
#include "ds_port/original_startup_global_service.hpp"
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
// Verified original APK ELF descriptor constants (run 41317600806).
inline constexpr std::uint32_t kStartupResourcePrimary=0x0042fc10u;
inline constexpr std::uint32_t kStartupResourceFallback=0x0042fc20u;
inline constexpr std::uint32_t kStartupAllocate34Descriptor=0x0042fc30u;
inline constexpr std::uint32_t kStartupAllocate58Descriptor=0x0042fc50u;
// Original 0x2803E4 resource lookup: return 0 to try fallback.
using OriginalStartupResourceLookup=bool (*)(
    std::uint32_t original_callee_va,std::uint32_t context_arm_va,
    std::uint32_t original_descriptor_arm_va,void* context,
    std::uint32_t* original_result) noexcept;
// Direct ARM 0x27FD7C(context), 0x36FD38(no verified incoming args).
using OriginalStartupDirectCall=bool (*)(
    std::uint32_t original_callee_va,std::uint32_t context_arm_va,
    void* context) noexcept;
struct OriginalStartupSecondTailInputs {
    std::uint32_t original_context_arm_va;
    void* original_context;
    std::size_t context_bytes;
    OriginalStartupServiceResolver resolve_object;
    OriginalStartupVirtualDispatch invoke_virtual;
    OriginalStartupDirectCall invoke_original_direct;
    OriginalStartupResourceLookup original_resource_0x2803e4;
    OriginalAllocatorDispatch original_allocator_vtable_0x0c;
    OriginalStartupObjectBaseProvider original_base_0x27e560;
    const OriginalSecondaryPipeline* native_secondary_0x2849e0;
    void* callback_context;
    // Use real native 0x27E560 singleton in both startup constructors.
    // Existing callback retained for older tests and unported callers.
    const OriginalSingletonInputs* native_singleton_0x27e560=nullptr;
    // Verified native 27-call 0x27FD7C wrapper when available.
    const OriginalRegistrationInputs* native_registration_0x27fd7c=nullptr;
    // Reconstructed 0x36FD38 -> 0x37000C wrapper.
    const OriginalGlobalServiceInputs* native_global_service_0x36fd38=nullptr;
};
enum class OriginalStartupSecondTailStatus:std::uint8_t {
    InvalidOriginalContext,
    MissingOriginalDirect,
    OriginalDirectRefused,
    MissingOriginalResource,
    OriginalResourceRefused,
    MissingOriginalVirtualMethod,
    OriginalVirtualRefused,
    MissingOriginalAllocator,
    OriginalAllocatorRefused,
    OriginalAllocatorReturnedNull,
    MissingOriginalBaseConstructor,
    OriginalBaseConstructorRefused,
    SecondaryPipelineBlocked,
    CompletedOriginalSecondRoutine
};
// Starts immediately AFTER original 0x2833F4 at 0x280580 and runs
// through 0x2806A0. Does not fake any original method implementations.
OriginalStartupSecondTailStatus original_execute_stage_zero_tail_0x280580(
    const OriginalStartupSecondTailInputs&) noexcept;
}
