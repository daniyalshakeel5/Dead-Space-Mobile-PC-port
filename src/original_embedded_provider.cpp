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
OriginalEmbeddedProviderResult original_embedded_provider_error_0x26c3cc(
    const void* mapped,std::size_t size) noexcept {
    using S=OriginalEmbeddedProviderStatus;
    // Original 0x26C3CC: LDR r0,[r0,#0xA0]; BX lr.
    if(!mapped||size<0xa4u)return {S::InvalidMappedProvider,0};
    const auto* p=static_cast<const std::uint8_t*>(mapped);
    return {S::Completed,load32(p+0xa0u)};
}
OriginalEmbeddedProviderResult original_embedded_provider_size_0x2763ac(
    const void* mapped,std::size_t size) noexcept {
    using S=OriginalEmbeddedProviderStatus;
    if(!mapped||size<0xa8u)return {S::InvalidMappedProvider,0};
    const auto* p=static_cast<const std::uint8_t*>(mapped);
    if(load32(p+0xa4u)==1u){
        // ARM 0x27641C..0x276430: null provider stream and
        // unknown cached length both return -1 or enter codec.
        if(!load32(p+4u))return {S::Completed,0xffffffffu};
        const auto known=load32(p+8u);
        if(known!=0xffffffffu)return {S::Completed,known};
        // ARM 0x276434..0x276458 invokes original 0x26C26C,
        // 0x27627C and 0x26C2F4; still unported codec.
        return {S::NeedsOriginalMemoryService,0};
    }
    // The original file mode at 0x2763C4 returns -1 for fd=-1,
    // otherwise performs two 0x35350 seeks then restores position.
    if(load32(p+0x10u)==0xffffffffu)
        return {S::Completed,0xffffffffu};
    return {S::NeedsOriginalFileService,0};
}
OriginalEmbeddedProviderResult original_embedded_provider_seek_0x273034(
    const void* mapped,std::size_t size,std::uint32_t offset,
    std::uint32_t origin) noexcept {
    using S=OriginalEmbeddedProviderStatus;
    if(!mapped||size<0xa8u)return {S::InvalidMappedProvider,0};
    const auto* p=static_cast<const std::uint8_t*>(mapped);
    if(load32(p+0xa4u)==1u){
        // Original 0x273084..0x27314C: mode 0 absolute,
        // mode 1 current-relative, mode 2 cached-size-relative.
        std::uint32_t wanted=offset;
        if(origin==1u)wanted+=load32(p+0x0cu);
        if(origin==2u){
            const auto end=load32(p+8u);
            if(end==0xffffffffu)
                return {S::Completed,0};
            wanted+=end;
        }
        // ARM 0x27309C: compare target with owner+0xC;
        // if equal, return 1 without initializing stream state.
        if(wanted==load32(p+0x0cu))
            return {S::Completed,1};
        // Different cursor needs original transfer construction
        // 0x26C26C and 0x272BCC / 0x272CEC codec seeks.
        return {S::NeedsOriginalMemoryService,0};
    }
    // Original 0x27304C..0x273054: fd=-1 jumps directly to
    // error return 0; otherwise it executes native lseek wrapper.
    if(load32(p+0x10u)==0xffffffffu)
        return {S::Completed,0};
    return {S::NeedsOriginalFileService,0};
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
        // ARM 0x2717DC invokes this very provider's vtable +0x20.
        // When it returns a cached size (not -1), the function
        // invokes +0x20 a second time and subtracts it from +0xC.
        const auto length=original_embedded_provider_size_0x2763ac(
            mapped,size);
        if(length.status==S::Completed){
            if(length.original_r0==0xffffffffu)
                return {S::Completed,0};
            return {S::Completed,
                    load32(data+0x0cu)-length.original_r0};
        }
        return {length.status,0};
    }
    // Original 0x2717B0: unsupported selector (not 0 or 2)
    // on file-backed provider returns zero.
    if(selector!=0u&&selector!=2u)return {S::Completed,0};
    // For file-backed sources ARM calls original libc wrapper
    // 0x35350; do not supply an invented file position.
    return {S::NeedsOriginalFileService,0};
}
} // namespace ds_port::original_runtime
