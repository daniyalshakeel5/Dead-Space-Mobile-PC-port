#include "ds_port/original_resource_registration.hpp"
#include <algorithm>
#include <string>
namespace ds_port::original_runtime {
namespace {
std::uint32_t load32(const void* data) noexcept {
    const auto* p=static_cast<const std::uint8_t*>(data);
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
bool original_string(std::uint32_t va,
                     const OriginalNativeRegistrationServices& svc,
                     std::string* output,
                     OriginalNativeRegistrationStatus* failure) {
    OriginalRegistrationStringBytes mapped{};
    if(!va||!svc.resolve_string(va,svc.context,&mapped)||
       !mapped.bytes||!mapped.accessible_bytes) {
        *failure=OriginalNativeRegistrationStatus::UnmappedOriginalString;
        return false;
    }
    // Fail closed rather than scanning arbitrary pointers or huge streams.
    constexpr std::size_t kMax=16*1024;
    const auto count=std::min(kMax,mapped.accessible_bytes);
    const auto* end=std::find(mapped.bytes,mapped.bytes+count,0);
    if(end==mapped.bytes+count) {
        *failure=OriginalNativeRegistrationStatus::UnterminatedOriginalString;
        return false;
    }
    output->assign(reinterpret_cast<const char*>(mapped.bytes),
                   static_cast<std::size_t>(end-mapped.bytes));
    return true;
}
}
// The original 0x284178 function first prechecks r0=context+0x18,
// r1=source, r2=descriptor. On nonzero it also calls 0x283F5C,
// then constructs first/second strings and lowercases ONLY the first
// using bytes from original map: LDRSH [table + byte*2 + 2],
// STRB result. Five original 0x281DF4 temporary string copies
// become RAII-managed native strings here.
OriginalNativeRegistrationStatus original_execute_resource_registration_0x284178(
    const OriginalRegistrationCall& call,
    const OriginalNativeRegistrationServices& svc) noexcept {
    if(call.helper_arm_va!=kOriginalStartupRegistrationHelperVA||
       !call.original_subcontext_va||
       call.original_subcontext_va>0xfffffffbu||
       !call.original_global_value||!call.descriptor_arm_va)
        return OriginalNativeRegistrationStatus::InvalidOriginalArguments;
    if(!svc.resolve_string || !svc.original_0x2839c8 ||
       !svc.original_0x283f5c || !svc.original_0x2837b4)
        return OriginalNativeRegistrationStatus::MissingOriginalService;
    const auto& map=svc.character_table;
    if(!map.mapped_got_word||map.got_word_bytes<4||
       !map.original_table_arm_va||!map.mapped_table_bytes||
       map.mapped_table_size<kOriginalRegistrationCharMapMinBytes||
       load32(map.mapped_got_word)!=map.original_table_arm_va)
        return OriginalNativeRegistrationStatus::InvalidOriginalCharacterTable;
    std::string source;
    std::string descriptor;
    auto error=OriginalNativeRegistrationStatus::UnmappedOriginalString;
    if(!original_string(call.original_global_value,svc,&source,&error))
        return error;
    if(!original_string(call.descriptor_arm_va,svc,&descriptor,&error))
        return error;
    std::uint32_t found=0;
    if(!svc.original_0x2839c8(call,svc.context,&found))
        return OriginalNativeRegistrationStatus::OriginalPrecheckRefused;
    if(found && !svc.original_0x283f5c(call,svc.context))
        return OriginalNativeRegistrationStatus::OriginalMatchedActionRefused;

    // The original code loads each byte unsigned, indexes an int16_t
    // locale table at 2*(byte)+2, and truncates to byte on STRB.
    // The source is private mapped game data, not ASCII/locale guesses.
    for(auto& c:source){
        const auto byte=static_cast<unsigned char>(c);
        c=static_cast<char>(map.mapped_table_bytes[
            static_cast<std::size_t>(byte)*2u+2u]);
    }
    // The original 0x2837B4 takes context+4 (already offset+0x18
    // from original parent), plus copies of BOTH strings and r3=0.
    // The original string owner/allocation ABI and container method
    // are intentionally not fabricated here.
    if(!svc.original_0x2837b4(
        call,call.original_subcontext_va+4u,
        std::string_view(source),std::string_view(descriptor),svc.context))
        return OriginalNativeRegistrationStatus::OriginalInsertRefused;
    return OriginalNativeRegistrationStatus::CompletedOriginalPath;
}
} // namespace ds_port::original_runtime
