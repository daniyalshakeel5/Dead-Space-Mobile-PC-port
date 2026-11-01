#include "ds_port/original_startup_global_service.hpp"
#include <array>
namespace ds_port::original_runtime {
namespace {
std::uint32_t get32(const void* p) noexcept {
    const auto* b=static_cast<const std::uint8_t*>(p);
    return std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|
           (std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);
}
void put32(std::uint8_t* b,std::uint32_t v) noexcept {
    for(unsigned i=0;i<4;++i)b[i]=std::uint8_t(v>>(8*i));
}
}
OriginalGlobalServiceResult original_execute_global_service_0x36fd38(
    const OriginalGlobalServiceInputs& in) noexcept {
    const auto fail=[](OriginalGlobalServiceStatus s) noexcept {
        return OriginalGlobalServiceResult{s,0};
    };
    if(!in.mapped_original_global_word || in.global_word_bytes<4)
        return fail(OriginalGlobalServiceStatus::InvalidOriginalMemory);
    if(!in.original_0x282000||!in.original_0x282614||
       !in.original_0x288408)
        return fail(OriginalGlobalServiceStatus::MissingOriginalCallees);
    // Original SUB sp,sp,#0x154 and r5=sp+0x0C. Native scratch
    // is intentionally zero-initialized to avoid exposing host
    // uninitialized data; callback implementation owns game semantics.
    alignas(8) std::array<std::uint8_t,0x154> local{};
    auto* temp=local.data()+0x0c;
    constexpr std::size_t temp_bytes=0x154u-0x0cu;
    // Original 0x370048 calls 0x282000. It must be invoked before
    // reloading the global word for the following 0x282614 call.
    if(!in.original_0x282000(
         temp,temp_bytes,kOriginalGlobalServiceDescriptorVA,
         kOriginalGlobalServiceDuration,
         get32(in.mapped_original_global_word),
         kOriginalGlobalServiceArgumentVA,1u,in.callback_context))
        return fail(OriginalGlobalServiceStatus::OriginalConstructRefused);
    if(!in.original_0x282614(
         get32(in.mapped_original_global_word),temp,temp_bytes,
         in.callback_context))
        return fail(OriginalGlobalServiceStatus::OriginalAttachRefused);
    // Original 0x370068: STR r3,[sp,#0x0C] with r3=literal +8,
    // followed by BL 0x288408(r0=sp+0x1C).
    put32(temp,kOriginalGlobalServiceTempVtableVA);
    std::uint32_t result=0;
    if(!in.original_0x288408(temp+0x10,temp_bytes-0x10,
          in.callback_context,&result))
        return fail(OriginalGlobalServiceStatus::OriginalFinishRefused);
    return {OriginalGlobalServiceStatus::CompletedOriginalWrapper,result};
}
}
