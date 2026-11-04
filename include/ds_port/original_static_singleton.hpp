#pragma once
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
// Original 0x27E560 ELF values established from real APK instruction
// literals and 4 GOT slot offsets on GitHub Actions run 41319202254.
inline constexpr std::uint32_t kVerifiedSingletonGuardVA=0x0049c540u;
inline constexpr std::uint32_t kVerifiedSingletonObjectVA=0x0049c544u;
inline constexpr std::uint32_t kVerifiedSingletonSourceGOT=0x0049096cu;
inline constexpr std::uint32_t kVerifiedSingletonTableGOT=0x00490ae4u;
inline constexpr std::uint32_t kVerifiedSingletonDtorGOT=0x00490a54u;
inline constexpr std::uint32_t kVerifiedSingletonDsoGOT=0x00490840u;
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
// Map actual original ARM32 GOT entries before constructing the C++
// singleton. An application must supply these as REAL mapped APK data,
// never fabricate vtable/field/destructor pointers.
struct OriginalSingletonOriginalGOT {
    const void* field_source_pointer_slot; // ELF 0x0049096C
    const void* object_table_pointer_slot; // ELF 0x00490AE4
    const void* destructor_pointer_slot; // ELF 0x00490A54
    const void* dso_pointer_slot; // ELF 0x00490840
    std::uint32_t original_field_source_arm_va;
    const void* mapped_field_source_word;
};
bool original_bind_static_singleton_from_got(
    const OriginalSingletonInputs& unresolved,
    const OriginalSingletonOriginalGOT& got,
    OriginalSingletonInputs* verified) noexcept;

struct OriginalSingletonResult {
    OriginalSingletonStatus status;
    std::uint32_t original_object_arm_va;
    bool constructed;
};
// Original object begins at guard+4; accessed by other original callers.
// Verifies exact size, addresses, and callbacks before modifying memory.
OriginalSingletonResult original_initialize_static_0x27e560(
    const OriginalSingletonInputs&) noexcept;
// Original 0x27E560 ignores incoming r0 and returns static object+4;
// matches the existing startup constructor base-provider signature.
bool original_static_singleton_base_provider(
    std::uint32_t original_callee_va,std::uint32_t ignored_arm_argument,
    void* context,std::uint32_t* original_return) noexcept;
}
