#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
// Complete original 0x0027FD7C disassembly + ELF PC-relative
// literals verified from the user's APK on GitHub Actions run 41320699473.
// Code is x86-64 C++17; no ARM executable, loader, or resource data included.
inline constexpr std::uint32_t kOriginalStartupRegistrationHelperVA=0x00284178u;
struct OriginalRegistrationSpec {
    std::uint32_t callsite_arm_va;
    std::uint32_t global_slot_arm_va;
    std::uint32_t descriptor_arm_va;
    bool tail_call;
};
inline constexpr std::array<OriginalRegistrationSpec,27>
    kOriginalRegistrationSequence0x27fd7c = {{
        {0x0027fda8u,0x00490908u,0x0044ca10u,false},
        {0x0027fdc4u,0x004907b4u,0x0044ca10u,false},
        {0x0027fde0u,0x00490a30u,0x0044ca10u,false},
        {0x0027fdf8u,0x00490814u,0x0044b008u,false},
        {0x0027fe10u,0x00490b04u,0x0044ca10u,false},
        {0x0027fe28u,0x0049080cu,0x0044b008u,false},
        {0x0027fe40u,0x004907ccu,0x0044b008u,false},
        {0x0027fe58u,0x00490a14u,0x0044b008u,false},
        {0x0027fe70u,0x00490738u,0x0044b008u,false},
        {0x0027fe88u,0x00490a18u,0x0044b008u,false},
        {0x0027fea4u,0x00490958u,0x0043ca18u,false},
        {0x0027febcu,0x00490a84u,0x0044ca10u,false},
        {0x0027fed8u,0x00490af8u,0x00426720u,false},
        {0x0027fef0u,0x0049091cu,0x0044ca10u,false},
        {0x0027ff08u,0x004909e8u,0x0044b008u,false},
        {0x0027ff20u,0x00490774u,0x0044b008u,false},
        {0x0027ff38u,0x00490918u,0x0044b008u,false},
        {0x0027ff50u,0x004909b4u,0x0044ca10u,false},
        {0x0027ff68u,0x00490820u,0x0044b008u,false},
        {0x0027ff84u,0x004909e0u,0x0044e4f8u,false},
        {0x0027ffa0u,0x004907f0u,0x0042fbe0u,false},
        {0x0027ffbcu,0x00490a10u,0x00431c20u,false},
        {0x0027ffd4u,0x0049076cu,0x0044ca10u,false},
        {0x0027ffecu,0x00490938u,0x0044ca10u,false},
        {0x00280004u,0x0049088cu,0x0044ca10u,false},
        {0x0028001cu,0x004907a4u,0x0044ca10u,false},
        {0x00280038u,0x00490aa8u,0x0044ca10u,true},
    }};
// Every original [GOT pointer slot] must be independently supplied and
// mapped by the host: the game stores the address of another ARM32
// global at [GOT]; 0x27FD7C then reads the value of that global.
struct OriginalRegistrationMappedGlobal {
    std::uint32_t global_arm_va;
    const void* got_slot_host_bytes;
    std::size_t got_slot_bytes;
    const void* global_host_bytes;
    std::size_t global_bytes;
};
using OriginalRegistrationGlobalResolver=bool (*)(
    std::uint32_t original_got_slot_va,void* context,
    OriginalRegistrationMappedGlobal* output) noexcept;
struct OriginalRegistrationCall {
    std::uint32_t helper_arm_va;
    std::uint32_t original_callsite_va;
    std::uint32_t original_subcontext_va;  // original r0=context+0x18
    std::uint32_t original_global_value;  // original r1=**GOT
    std::uint32_t descriptor_arm_va;       // original r2
    bool original_tail_branch;
};
using OriginalRegistrationInvoke=bool (*)(
    const OriginalRegistrationCall&,void* context) noexcept;
struct OriginalRegistrationInputs {
    std::uint32_t original_context_arm_va;
    OriginalRegistrationGlobalResolver resolve_original_global;
    OriginalRegistrationInvoke invoke_original_0x284178;
    void* callback_context;
};
enum class OriginalRegistrationStatus : std::uint8_t {
    InvalidOriginalContext,
    MissingOriginalGlobalResolver,
    MissingOriginalRegistrationHelper,
    UnmappedOriginalGlobal,
    InvalidOriginalGotPointer,
    OriginalRegistrationRefused,
    CompletedOriginalSequence
};
OriginalRegistrationStatus original_register_stage_resources_0x27fd7c(
    const OriginalRegistrationInputs&) noexcept;
} // namespace ds_port::original_runtime
