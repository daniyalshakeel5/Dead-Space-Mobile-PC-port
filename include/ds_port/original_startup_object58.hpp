#pragma once
#include "ds_port/original_startup_object34.hpp"
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
// Native original 0x280A34, complete through return.
// Verified APK ELF table tokens (GitHub Actions run 41317933905).
inline constexpr std::uint32_t kOriginal280a34Table0=0x00489880u;
inline constexpr std::uint32_t kOriginal280a34Table4=0x004898c4u;
inline constexpr std::uint32_t kOriginal280a34Table14=0x00489858u;
bool original_construct_object58_0x280a34(
    std::uint32_t original_object_arm_address,
    void* mapped_object,std::size_t accessible_bytes,
    OriginalStartupObjectBaseProvider original_0x27e560,
    void* callback_context) noexcept;
}
