#pragma once
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
// Original APK: guard 0x49C540, object 0x49C544 (2026-11-01 verified ELF).
inline constexpr std::uint32_t kSingletonGuardVA=0x0049c540u;
inline constexpr std::uint32_t kSingletonObjectVA=0x0049c544u;
inline constexpr std::uint32_t kSingletonFieldSourceGOT=0x0049096cu;
inline constexpr std::uint32_t kSingletonTableGOT=0x00490ae4u;
inline constexpr std::uint32_t kSingletonDestructorGOT=0x00490a54u;
inline constexpr std::uint32_t kSingletonDsoGOT=0x00490840u;
using OriginalAtexitRegister=bool (*)(std::uint32_t object,
    std::uint32_t destructor,std::uint32_t dso,void* ctx) noexcept;
struct OriginalSingletonMemory {
    void* guard_and_object;
    std::size_t bytes;
    const void* field_source_got;
    const void* table_got;
    const void* destructor_got;
    const void* dso_got;
    std::uint32_t source_arm_va;
    const void* source_word;
    OriginalAtexitRegister register_destructor;
    void* context;
};
enum class OriginalSingletonStatus:std::uint8_t {
    InvalidMemory,MissingRegistration,RegistrationRefused,AlreadyInitialized,
    ConstructedAndRegistered
};
OriginalSingletonStatus original_initialize_singleton_0x27e560(
    const OriginalSingletonMemory&,std::uint32_t* out) noexcept;
// Same callback signature as original constructor's BL 0x27E560.
// Original ignores its incoming r0 on every call.
bool original_singleton_base_provider_0x27e560(
    std::uint32_t target,std::uint32_t ignored_r0,
    void* singleton_context,std::uint32_t* out) noexcept;
}
