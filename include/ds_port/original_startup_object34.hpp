#pragma once
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
// Original ARM ELF 0x287A94: 0x34-byte object constructor.
// All three addresses independently verified on original APK runner.
inline constexpr std::uint32_t kOriginal287a94Table0=0x00489a10u;
inline constexpr std::uint32_t kOriginal287a94Table4=0x00489a48u;
inline constexpr std::uint32_t kOriginal287a94Table14=0x00489a00u;
using OriginalStartupObjectBaseProvider=bool (*)(
    std::uint32_t original_callee_va,std::uint32_t object_arm_va,
    void* context,std::uint32_t* returned_arm_va) noexcept;
// The original BL 0x27E560 body is not recovered. Native code performs
// all original local stores and calls the required provider exactly once.
bool original_construct_object34_0x287a94(
    std::uint32_t object_arm_va,void* mapped_object,
    std::size_t accessible_bytes,
    OriginalStartupObjectBaseProvider original_0x27e560,
    void* provider_context) noexcept;
}
