#pragma once
#include "ds_port/original_registry_lookup.hpp"
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
// All descriptor addresses from original APK ARM literal pools.
// Independently verified by GitHub Actions run 41316887945.
inline constexpr std::uint32_t kStartupFirstDescriptor=0x0042f620u;
inline constexpr std::uint32_t kStartupFirstArg2=0x0042f938u;
inline constexpr std::uint32_t kStartupFirstArg3=0x0042f940u;
inline constexpr std::uint32_t kStartupNextArg2=0x0042f698u;
inline constexpr std::uint32_t kStartupNextArg3=0x0042f6a8u;
inline constexpr std::uint32_t kStartupNestedDescriptor=0x0042f648u;
inline constexpr std::uint32_t kStartupNestedArg2=0x0042f668u;
inline constexpr std::uint32_t kStartupNestedArg3=0x0042f678u;

struct OriginalStartupServiceMemory {
    std::uint32_t original_service_arm_address;
    const void* host_object;
    std::size_t object_bytes;
    std::uint32_t original_vtable_arm_address;
    const void* host_vtable;
    std::size_t vtable_bytes;
};
// Resolve original ARM address -> already constructed host-mapped service.
// Never maps an ARM32 token to a host pointer by reinterpret_cast.
using OriginalStartupServiceResolver=bool (*)(
    std::uint32_t service_arm_va,void* context,
    OriginalStartupServiceMemory* mapped) noexcept;
struct OriginalStartupVirtualCall {
    std::uint32_t original_owner_arm_va;
    std::uint32_t original_method_arm_va;
    std::uint32_t vtable_slot;
    std::uint32_t r1,r2,r3;
    std::uint8_t known_reg_mask; // bit 0 r1, bit 1 r2, bit 2 r3
    // ARM wrapper 0x2713B8/0xF895C passes a pointer to saved r3;
    // represent it with a host-backed word instead of inventing an ARM VA.
    std::uint32_t* saved_r3_host_word;
};
struct OriginalStartupVirtualResult {
    std::uint32_t r0=0;
    std::uint32_t r3_after=0;
};
using OriginalStartupVirtualDispatch=bool (*)(
    const OriginalStartupVirtualCall&,void* context,
    OriginalStartupVirtualResult*) noexcept;
// Direct original function 0x2068A8 is not yet reconstructed.
// It is invoked by the nested 0x27F124 initializer, in exact order.
using OriginalStartupDirect2068a8=bool (*)(
    std::uint32_t r0,std::uint32_t r1,std::uint32_t r2,
    void* context,std::uint32_t* original_r0) noexcept;
struct OriginalStartupRegistry {
    const void* bss_slot;
    std::size_t bss_bytes;
    std::uint32_t registry_arm_va;
    const void* registry_object;
    std::size_t registry_object_bytes;
    std::uint32_t registry_vtable_arm_va;
    const void* registry_vtable;
    std::size_t registry_vtable_bytes;
    OriginalRegistryQueryDispatch registry_method;
};
struct OriginalStartupFirstInputs {
    OriginalStartupRegistry registry;
    OriginalStartupServiceResolver resolve_service;
    OriginalStartupVirtualDispatch call_virtual;
    OriginalStartupDirect2068a8 direct_0x2068a8;
    void* context;
};
enum class OriginalStartupFirstStatus:std::uint8_t {
    MissingOriginalRegistry,OriginalRegistryRefused,InvalidOriginalObject,
    MissingOriginalVirtualMethod,OriginalVirtualRefused,
    MissingOriginalDirectCall,OriginalDirectRefused,
    CompletedOriginalFirstRoutine
};
// Entire observed 0x27F790 -> 0x27F124 -> 0x2713B8 -> 0xF895C
// control-flow including nested registry and seven virtual call sites.
// This does not implement the underlying original virtual methods.
OriginalStartupFirstStatus original_execute_startup_first_0x27f790(
    const OriginalStartupFirstInputs&) noexcept;
} // namespace ds_port::original_runtime
