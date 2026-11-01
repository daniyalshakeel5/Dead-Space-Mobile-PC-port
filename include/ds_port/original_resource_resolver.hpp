#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>
namespace ds_port::original_runtime {
// Original APK ARM 0x28A718..0x28A864; real evidence run 41327755219.
// The original calls below are NOT ported by this interface: they must be
// backed by separately reconstructed native game services, never stubs.
using OriginalResolverPoolAlloc=bool (*)(std::uint32_t bytes,
    std::uint32_t owner,void*,std::uint32_t*) noexcept; // 0x276CB4
using OriginalResolverPoolInit=bool (*)(std::uint32_t pool,
    std::uint32_t bytes,std::uint32_t flag,void*,void**,std::size_t*) noexcept; // 0x275680
using OriginalResolverObjectAlloc=bool (*)(std::uint32_t bytes,
    void*,std::uint32_t*) noexcept; // 0x27E8E8
using OriginalResolverObjectInit=bool (*)(std::uint32_t object,
    std::uint32_t pool,std::uint32_t bytes,std::uint32_t flag,
    void*) noexcept; // 0x26D35C
using OriginalResolverObjectParam=bool (*)(std::uint32_t object,
    std::uint32_t mode,std::uint32_t float_bits,void*) noexcept; // 0x26D23C
using OriginalResolverNameCopy=bool (*)(std::string_view name,
    void*) noexcept; // 0x27B204
using OriginalResolverLookup=bool (*)(std::uint32_t arg1,
    std::uint32_t arg2,std::uint32_t arg3,std::uint32_t stack0,
    void*,std::uint32_t*) noexcept; // 0x272DDC
using OriginalResolverTransform=bool (*)(std::uint32_t size,
    void*,std::uint32_t* result,std::uint32_t* scratch_token) noexcept; // 0x2716F8
using OriginalResolverVirtual=bool (*)(std::uint32_t object,
    std::uint32_t slot,std::uint32_t first,std::uint32_t second,
    void*) noexcept; // vtable +0x3C, +0x2C
using OriginalResolverRelease=bool (*)(void*) noexcept; // 0x27744C

struct OriginalResolverInputs0x28a718 {
    OriginalResolverPoolAlloc pool_alloc=nullptr;
    OriginalResolverPoolInit pool_init=nullptr;
    OriginalResolverObjectAlloc object_alloc=nullptr;
    OriginalResolverObjectInit object_init=nullptr;
    OriginalResolverObjectParam object_param=nullptr;
    OriginalResolverNameCopy copy_name=nullptr;
    OriginalResolverLookup lookup=nullptr;
    OriginalResolverTransform transform=nullptr;
    OriginalResolverVirtual invoke_virtual=nullptr;
    OriginalResolverRelease release_temporary=nullptr;
    void* context=nullptr;
};
enum class OriginalResolverStatus0x28a718 : std::uint8_t {
    MissingOriginalService, OriginalPoolBlocked, InvalidOriginalBuffer,
    OriginalObjectBlocked, OriginalNameBlocked, OriginalLookupBlocked,
    OriginalTransformBlocked, OriginalVirtualBlocked,
    OriginalTemporaryCleanupBlocked, Completed
};
struct OriginalResolverResult0x28a718 {
    OriginalResolverStatus0x28a718 status;
    std::uint32_t original_resource_object_va; // Original ARM r0 on return
};
// This function translates the *verified call/branch order* in original
// 0x28A718, not the internal ARM service bodies. Zero lookup still returns
// the allocated 0x30-byte object after temporary cleanup.
OriginalResolverResult0x28a718 original_execute_resolver_0x28a718(
    const OriginalResolverInputs0x28a718&,
    std::uint32_t original_owner_va,
    std::string_view original_resource_name) noexcept;
} // namespace ds_port::original_runtime
