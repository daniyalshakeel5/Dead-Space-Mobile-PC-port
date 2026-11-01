#pragma once
#include "ds_port/original_resource_handle.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>
namespace ds_port::original_runtime {
// Original ELF 0x2803E4, observed in original-APK run 41321446955.
// This is the original control-flow shell, not a complete resource provider.
// Every unresolved game service must be bound to its independently
// reconstructed native implementation; a missing binding blocks the query.
struct OriginalResourceQueryString {
    const std::uint8_t* bytes=nullptr;
    std::size_t accessible_bytes=0;
};
using OriginalResourceQueryResolveName=bool (*)(
    std::uint32_t resource_name_arm_va,void* context,
    OriginalResourceQueryString* mapped) noexcept;
using OriginalResourceQueryMakeHandle=bool (*)(
    std::uint32_t owner_arm_va,std::string_view name,void* context,
    OriginalResourceHandleInput* mapped_handle) noexcept;
using OriginalResourceQueryReadyFailure=bool (*)(
    std::uint32_t owner_arm_va,std::string_view name,
    const OriginalResourceHandleInput& handle,void* context) noexcept;
using OriginalResourceQueryScratchCreate=bool (*)(
    std::uint32_t owner_arm_va,void* context) noexcept;
using OriginalResourceQueryBinding=bool (*)(
    std::uint32_t original_context_plus_0x18_va,unsigned index,
    void* context,std::uint32_t* result) noexcept;
using OriginalResourceQueryLoader=bool (*)(
    std::uint32_t original_context_plus_0x18_va,
    std::uint32_t first_binding,std::uint32_t second_binding,
    const OriginalResourceHandleInput& handle,void* context) noexcept;
using OriginalResourceQueryCleanup=bool (*)(void* context) noexcept;
struct OriginalResourceQueryInputs {
    std::uint32_t original_context_arm_va=0;
    const void* mapped_context=nullptr;
    std::size_t context_bytes=0;
    OriginalResourceQueryResolveName resolve_name=nullptr;
    OriginalResourceQueryMakeHandle original_0x28b3a4=nullptr;
    OriginalResourceHandleVirtual provider_method_0x50=nullptr;
    OriginalResourceQueryReadyFailure original_0x28a4f8_failure_path=nullptr;
    OriginalResourceQueryScratchCreate original_0x284df8=nullptr;
    OriginalResourceQueryBinding original_0x283cb4=nullptr;
    OriginalResourceQueryLoader original_0x286918=nullptr;
    OriginalResourceQueryCleanup original_0x28a644=nullptr;
    OriginalResourceQueryCleanup original_0x284f00=nullptr;
    void* context=nullptr;
};
enum class OriginalResourceQueryStatus:std::uint8_t {
    InvalidContext,UnmappedName,UnterminatedName,MissingOriginalService,
    HandleCreationRefused,HandleReadinessBlocked,OriginalFailurePathBlocked,
    OriginalScratchBlocked,OriginalBindingBlocked,OriginalLoaderBlocked,
    OriginalCleanupBlocked,Completed
};
struct OriginalResourceQueryResult {
    OriginalResourceQueryStatus status;
    std::uint32_t original_r0;
};
OriginalResourceQueryResult original_execute_resource_query_0x2803e4(
    const OriginalResourceQueryInputs&,std::uint32_t name_arm_va) noexcept;
} // namespace ds_port::original_runtime
