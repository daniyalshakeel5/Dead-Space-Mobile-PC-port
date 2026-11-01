#include "ds_port/original_embedded_provider.hpp"
namespace ds_port::original_runtime {
namespace {
std::uint32_t load32(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
}
OriginalEmbeddedProviderResult original_embedded_provider_mode_0x26c3c4(
    const void* mapped,std::size_t size) noexcept {
    using S=OriginalEmbeddedProviderStatus;
    if(!mapped||size<0x94u)return {S::InvalidMappedProvider,0};
    const auto* data=static_cast<const std::uint8_t*>(mapped);
    return {S::Completed,load32(data+0x90u)};
}
OriginalEmbeddedProviderResult original_embedded_provider_position_0x271794(
    const void* mapped,std::size_t size,
    std::uint32_t selector) noexcept {
    using S=OriginalEmbeddedProviderStatus;
    if(!mapped||size<0xa8u)return {S::InvalidMappedProvider,0};
    const auto* data=static_cast<const std::uint8_t*>(mapped);
    // Original 0x271798 LDR +0xA4; CMP #1.
    if(load32(data+0xa4u)==1u){
        // Original 0x2717C4: if [+4] is null, r0=0.
        if(load32(data+4u)==0u)
            return {S::Completed,0};
        // Original 0x2717D0: when selector=0, LDR +0xC.
        if(selector==0u)
            return {S::Completed,load32(data+0x0cu)};
        return {S::NeedsOriginalMemoryService,0};
    }
    // Original 0x2717B0: unsupported selector (not 0 or 2)
    // on file-backed provider returns zero.
    if(selector!=0u&&selector!=2u)return {S::Completed,0};
    // For file-backed sources ARM calls original libc wrapper
    // 0x35350; do not supply an invented file position.
    return {S::NeedsOriginalFileService,0};
}
} // namespace ds_port::original_runtime
