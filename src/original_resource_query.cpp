#include "ds_port/original_resource_query.hpp"
#include <algorithm>
namespace ds_port::original_runtime {
namespace {
std::uint32_t load32(const void* memory) noexcept {
    const auto* p=static_cast<const std::uint8_t*>(memory);
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
}
// Original 0x2803E4. Refer to the ARM trace in run 41321446955.
// Native std::string_view replaces private ARM string copy allocation
// 0x284D38 -> 0x1CA8B0; owned name memory remains caller-provided.
// No original game bytes or ARM execution are included.
OriginalResourceQueryResult original_execute_resource_query_0x2803e4(
    const OriginalResourceQueryInputs& in,
    std::uint32_t name_arm_va) noexcept {
    auto fail=[](OriginalResourceQueryStatus s) noexcept {
        return OriginalResourceQueryResult{s,0};
    };
    if(!in.mapped_context||in.context_bytes<0x1c||
       !in.original_context_arm_va||
       in.original_context_arm_va>0xffffffe7u||
       !name_arm_va)
        return fail(OriginalResourceQueryStatus::InvalidContext);
    if(!in.resolve_name)
        return fail(OriginalResourceQueryStatus::MissingOriginalService);
    OriginalResourceQueryString mapped{};
    if(!in.resolve_name(name_arm_va,in.context,&mapped)||
       !mapped.bytes||!mapped.accessible_bytes)
        return fail(OriginalResourceQueryStatus::UnmappedName);
    // Original code scans a signed-byte C string until zero. A bounded
    // source protects native code from an unmapped/unterminated ARM name.
    constexpr std::size_t kMax=16u*1024u;
    const auto size=std::min(kMax,mapped.accessible_bytes);
    const auto* end=std::find(mapped.bytes,mapped.bytes+size,0);
    if(end==mapped.bytes+size)
        return fail(OriginalResourceQueryStatus::UnterminatedName);
    const std::string_view name(
        reinterpret_cast<const char*>(mapped.bytes),
        static_cast<std::size_t>(end-mapped.bytes));
    const auto* ctx=static_cast<const std::uint8_t*>(in.mapped_context);
    const std::uint32_t owner=load32(ctx+4);
    if(!owner)
        return fail(OriginalResourceQueryStatus::InvalidContext);
    if(!in.original_0x28b3a4 && !in.native_factory_0x28b3a4)
        return fail(OriginalResourceQueryStatus::MissingOriginalService);
    OriginalResourceHandleInput handle{};
    if(in.native_factory_0x28b3a4) {
        const auto created=original_construct_mode1_handle_0x28b3a4(
            *in.native_factory_0x28b3a4,owner,name);
        if(created.status!=
           OriginalMode1FactoryStatus::CompletedOriginalMode1Constructor)
            return fail(OriginalResourceQueryStatus::HandleCreationRefused);
        handle=created.handle;
    } else if(!in.original_0x28b3a4(owner,name,in.context,&handle)) {
        return fail(OriginalResourceQueryStatus::HandleCreationRefused);
    }
    // This method is an actual reconstructed original 0x28A5F4.
    const auto ready=original_resource_ready_0x28a5f4(
        handle,in.provider_method_0x50,in.context);
    if(ready.status!=OriginalResourceHandleStatus::Completed)
        return fail(OriginalResourceQueryStatus::HandleReadinessBlocked);
    if(!ready.original_r0) {
        // Original 0x28045C calls 0x28A4F8. For the reconstructed
        // mode=1 null-resource handle, the virtual provider release is
        // skipped. Native string_view has no original ARM heap to free.
        if(in.native_factory_0x28b3a4) {
            if(original_cleanup_unready_mode1_0x28a4f8(handle)!=
               OriginalUnreadyHandleStatus::CompletedNullResourceCleanup)
                return fail(OriginalResourceQueryStatus::OriginalFailurePathBlocked);
        } else {
            // Legacy factory path may require original cleanup virtuals.
            if(!in.original_0x28a4f8_failure_path ||
               !in.original_0x28a4f8_failure_path(owner,name,handle,in.context))
                return fail(OriginalResourceQueryStatus::OriginalFailurePathBlocked);
        }
        return {OriginalResourceQueryStatus::Completed,0};
    }
    // Original 0x2804A8: 0x284DF8(owner) -> 0x283CB4 twice
    // -> 0x286918(scratch, handle, context+0x18, first, second)
    // -> 0x28A644(handle) -> 0x284F00(scratch); return 1.
    if(!in.original_0x284df8||!in.original_0x283cb4||
       !in.original_0x286918||!in.original_0x28a644||
       !in.original_0x284f00)
        return fail(OriginalResourceQueryStatus::MissingOriginalService);
    if(!in.original_0x284df8(owner,in.context))
        return fail(OriginalResourceQueryStatus::OriginalScratchBlocked);
    std::uint32_t first=0,second=0;
    if(!in.original_0x283cb4(in.original_context_arm_va+0x18u,
                              0u,in.context,&first) ||
       !in.original_0x283cb4(in.original_context_arm_va+0x18u,
                              1u,in.context,&second))
        return fail(OriginalResourceQueryStatus::OriginalBindingBlocked);
    if(!in.original_0x286918(in.original_context_arm_va+0x18u,
                             first,second,handle,in.context))
        return fail(OriginalResourceQueryStatus::OriginalLoaderBlocked);
    if(!in.original_0x28a644(in.context) ||
       !in.original_0x284f00(in.context))
        return fail(OriginalResourceQueryStatus::OriginalCleanupBlocked);
    return {OriginalResourceQueryStatus::Completed,1};
}
} // namespace ds_port::original_runtime
