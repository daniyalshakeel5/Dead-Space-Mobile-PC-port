#pragma once
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
// ELF 0x0027E560: guarded construction of a BSS-static object.
// Original PC-relative literal evidence: guard is 4 bytes before returned
// object. All ARM VAs below are opaque 32-bit tokens, not native pointers.
enum class OriginalSingletonStatus:std::uint8_t {
    InvalidMemory, MissingOriginalImports, GuardAcquireRefused,
    MissingOriginalVtable, GuardReleaseRefused, AtexitRefused, Completed
};
using OriginalGuardAcquire=bool (*)(void* mapped_guard,
    void* context,std::uint32_t* acquired) noexcept;
using OriginalGuardRelease=bool (*)(void* mapped_guard,
    void* context) noexcept;
using OriginalAtexit=bool (*)(std::uint32_t object_arm_va,
    void* context) noexcept;
struct OriginalSingletonInputs {
    std::uint32_t guard_arm_va;
    std::uint8_t* mapped_guard_and_object;
    std::size_t mapped_bytes;
    std::uint32_t original_object_vtable_arm_va;
    std::uint32_t original_component_vtable_arm_va;
    OriginalGuardAcquire guard_acquire;
    OriginalGuardRelease guard_release;
    OriginalAtexit register_atexit;
    void* context;
};
struct OriginalSingletonResult {
    OriginalSingletonStatus status;
    std::uint32_t original_object_arm_va;
    bool constructed;
};
// Original object begins at guard+4; accessed by other original callers.
// Verifies exact size, addresses, and callbacks before modifying memory.
OriginalSingletonResult original_initialize_static_0x27e560(
    const OriginalSingletonInputs&) noexcept;
}
