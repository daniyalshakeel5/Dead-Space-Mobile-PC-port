#pragma once
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
// Verified from the user's APK on GitHub Actions run 41321446955.
// 0x36FD38: MOV r0,#1; tail branch 0x37000C.
// 0x37000C: three original calls, stack workspace and final vtable store.
inline constexpr std::uint32_t kOriginalGlobalServiceWordVA=0x00498838u;
inline constexpr std::uint32_t kOriginalGlobalServiceArgumentVA=0x0043c9c0u;
inline constexpr std::uint32_t kOriginalGlobalServiceDescriptorVA=0x00371608u;
inline constexpr std::uint32_t kOriginalGlobalServiceTempVtableVA=0x004898d8u;
inline constexpr std::uint32_t kOriginalGlobalServiceDuration=1000000u;
// Original ARM r0=temp+0x0C, r1=descriptor, r2=1M, r3=global,
// stack[0]=argument token and stack[1]=initial r0=1.
// This callback must execute the original function 0x282000.
using OriginalGlobalServiceConstruct=bool (*)(
    void* mapped_temp,std::size_t mapped_bytes,
    std::uint32_t descriptor_va,std::uint32_t duration,
    std::uint32_t original_global_word,
    std::uint32_t original_stack_argument_va,
    std::uint32_t original_enable_flag,void* context) noexcept;
// Original 0x282614(r0=*global reloaded,r1=mapped temp).
using OriginalGlobalServiceAttach=bool (*)(
    std::uint32_t original_global_word_after_construct,
    void* mapped_temp,std::size_t mapped_bytes,void* context) noexcept;
// Original 0x288408(r0=temp+0x10), returning r0 to the caller.
using OriginalGlobalServiceFinish=bool (*)(
    void* mapped_inner_temp,std::size_t mapped_bytes,
    void* context,std::uint32_t* original_return) noexcept;
struct OriginalGlobalServiceInputs {
    // Host memory backing the real original 0x00498838 word.
    void* mapped_original_global_word;
    std::size_t global_word_bytes;
    OriginalGlobalServiceConstruct original_0x282000;
    OriginalGlobalServiceAttach original_0x282614;
    OriginalGlobalServiceFinish original_0x288408;
    void* callback_context;
};
enum class OriginalGlobalServiceStatus:std::uint8_t {
    InvalidOriginalMemory,MissingOriginalCallees,
    OriginalConstructRefused,OriginalAttachRefused,
    OriginalFinishRefused,CompletedOriginalWrapper
};
struct OriginalGlobalServiceResult {
    OriginalGlobalServiceStatus status;
    std::uint32_t original_r0;
};
OriginalGlobalServiceResult original_execute_global_service_0x36fd38(
    const OriginalGlobalServiceInputs&) noexcept;
}
