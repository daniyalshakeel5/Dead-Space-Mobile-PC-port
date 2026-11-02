#include "ds_port/original_embedded_provider.hpp"
#include <cerrno>
#include <climits>
#include <cstdio>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace ds_port::original_runtime {
namespace {
std::uint32_t load32(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
void store32(std::uint8_t* p,std::uint32_t value) noexcept {
    for(unsigned k=0;k<4;++k)p[k]=static_cast<std::uint8_t>(value>>(k*8u));
}
bool matches_explicit_host_fd(const std::uint8_t* p,int native_fd) noexcept {
    return native_fd>=0 &&
           load32(p+0x10u)==static_cast<std::uint32_t>(native_fd);
}
}
OriginalEmbeddedProviderResult original_embedded_provider_read_file_0x2716f8(
    const void* mapped,std::size_t size,int host_fd,
    void* destination,std::size_t length) noexcept {
    using S=OriginalEmbeddedProviderStatus;
    if(!mapped||size<0xa8u)return {S::InvalidMappedProvider,0};
    const auto* p=static_cast<const std::uint8_t*>(mapped);
    if(load32(p+0xa4u)==1u)
        return {S::NeedsOriginalMemoryService,0};
    if(length>static_cast<std::size_t>(INT_MAX) ||
       (length && !destination))
        return {S::InvalidHostBuffer,0};
    // Original 0x271718: LDR r0,[provider+0x10],
    // CMN r0,#1, skip read if fd==-1. The original result
    // is -1 in r0, represented as 0xFFFFFFFF here.
    if(load32(p+0x10u)==0xffffffffu)
        return {S::Completed,0xffffffffu};
    // Original fd tokens are not generally host descriptors;
    // demand the caller explicitly bind a live native file.
    if(!matches_explicit_host_fd(p,host_fd))
        return {S::InvalidHostFileBinding,0};
    // User-owned extracted asset file: native host I/O.
    // Original APK ELF PLT 0x35554 resolves to libc read.
#ifdef _WIN32
    const int returned=::_read(host_fd,destination,
                               static_cast<unsigned int>(length));
#else
    const auto returned=::read(host_fd,destination,length);
#endif
    if(returned<0)return {S::Completed,0xffffffffu};
    return {S::Completed,static_cast<std::uint32_t>(returned)};
}
OriginalEmbeddedProviderResult original_embedded_provider_seek_file_0x273034(
    void* mapped,std::size_t size,int host_fd,
    std::int32_t offset,std::uint32_t origin) noexcept {
    using S=OriginalEmbeddedProviderStatus;
    if(!mapped||size<0xa8u)return {S::InvalidMappedProvider,0};
    auto* p=static_cast<std::uint8_t*>(mapped);
    if(load32(p+0xa4u)==1u)
        return {S::NeedsOriginalMemoryService,0};
    // Original 0x27304C/0x27312C returns zero when fd==-1.
    if(load32(p+0x10u)==0xffffffffu)
        return {S::Completed,0};
    if(!matches_explicit_host_fd(p,host_fd))
        return {S::InvalidHostFileBinding,0};
    // Verified original APK 0x273034 ARM dispatch and its real
    // read-only table at 0x0042F0F4 (run 41426559665):
    // original origin 0 -> SEEK_SET, 1 -> SEEK_CUR, 2 -> SEEK_END.
    // Original SUB/CMP/MOVHI path maps every other origin to SEEK_SET.
    // This is the *original* behavior, not an Android compatibility shim.
    const int whence=origin==1u?SEEK_CUR:origin==2u?SEEK_END:SEEK_SET;
    errno=0;
#ifdef _WIN32
    const auto at=::_lseek(host_fd,static_cast<long>(offset),whence);
#else
    const auto at=::lseek(host_fd,static_cast<off_t>(offset),whence);
#endif
    if(at<0){
        // ARM 0x27310C..0x273118 obtains errno via original
        // 0x34FFC and stores it to provider+0xA0, then r0=0.
        store32(p+0xa0u,static_cast<std::uint32_t>(errno));
        return {S::Completed,0};
    }
    return {S::Completed,1};
}
OriginalEmbeddedProviderResult original_embedded_provider_size_file_0x2763ac(
    void* mapped,std::size_t size,int host_fd) noexcept {
    using S=OriginalEmbeddedProviderStatus;
    if(!mapped||size<0xa8u)return {S::InvalidMappedProvider,0};
    auto* p=static_cast<std::uint8_t*>(mapped);
    if(load32(p+0xa4u)==1u)
        return {S::NeedsOriginalMemoryService,0};
    if(load32(p+0x10u)==0xffffffffu)
        return {S::Completed,0xffffffffu};
    if(!matches_explicit_host_fd(p,host_fd))
        return {S::InvalidHostFileBinding,0};

    // Original ELF 0x2763D0..0x276470: libc lseek(fd,0,SEEK_CUR)
    // -> lseek(fd,0,SEEK_END) -> lseek(fd,prior,SEEK_SET).
    // Its return value is the END offset, not the restored cursor;
    // original ignores the last lseek return. Both original failure
    // branches retrieve errno and store it at provider +0xA0.
    errno=0;
#ifdef _WIN32
    const auto prior=::_lseek(host_fd,0L,SEEK_CUR);
#else
    const auto prior=::lseek(host_fd,static_cast<off_t>(0),SEEK_CUR);
#endif
    if(prior<0){
        store32(p+0xa0u,static_cast<std::uint32_t>(errno));
        return {S::Completed,0xffffffffu};
    }
    errno=0;
#ifdef _WIN32
    const auto end=::_lseek(host_fd,0L,SEEK_END);
#else
    const auto end=::lseek(host_fd,static_cast<off_t>(0),SEEK_END);
#endif
    if(end<0){
        store32(p+0xa0u,static_cast<std::uint32_t>(errno));
        return {S::Completed,0xffffffffu};
    }
#ifdef _WIN32
    (void)::_lseek(host_fd,prior,SEEK_SET);
#else
    (void)::lseek(host_fd,prior,SEEK_SET);
#endif
    // Native off_t can be wider than original ARM32 return register.
    // Refuse an unrepresentable successful file length.
    if(static_cast<std::uint64_t>(end)>0xffffffffull)
        return {S::NeedsOriginalFileService,0};
    return {S::Completed,static_cast<std::uint32_t>(end)};
}
OriginalEmbeddedProviderResult original_embedded_provider_position_file_0x271794(
    void* mapped,std::size_t size,
    int native_fd,std::uint32_t selector) noexcept {
    using S=OriginalEmbeddedProviderStatus;
    if(!mapped||size<0xa8u)return {S::InvalidMappedProvider,0};
    auto* p=static_cast<std::uint8_t*>(mapped);
    if(load32(p+0xa4u)==1u)
        return {S::NeedsOriginalMemoryService,0};
    // Verified 0x2717A8..0x2717B4: only selectors 0 and 2
    // dispatch the original file-backed seek path.
    if(selector!=0u&&selector!=2u)return {S::Completed,0};
    if(load32(p+0x10u)==0xffffffffu)
        return {S::Completed,0xffffffffu};
    if(!matches_explicit_host_fd(p,native_fd))
        return {S::InvalidHostFileBinding,0};

    // Original selector 0: lseek(fd,0,SEEK_CUR).
    // Original selector 2: lseek(fd,0,SEEK_CUR) followed by the
    // provider's +0x20 file-size method 0x2763AC, then
    // RSBNE r5,r0,r5 (r5 = size - prior position).
#ifdef _WIN32
    const auto at=::_lseek(native_fd,0L,SEEK_CUR);
#else
    const auto at=::lseek(native_fd,static_cast<off_t>(0),SEEK_CUR);
#endif
    if(at<0)return {S::Completed,0xffffffffu};
    if(static_cast<std::uint64_t>(at)>0xffffffffull)
        return {S::NeedsOriginalFileService,0};
    const auto position=static_cast<std::uint32_t>(at);
    if(selector==0u)return {S::Completed,position};

    const auto length=original_embedded_provider_size_file_0x2763ac(
        mapped,size,native_fd);
    if(length.status!=S::Completed)return length;
    // ARM CMN r0,#1 suppresses RSB when size query failed.
    if(length.original_r0==0xffffffffu)
        return {S::Completed,position};
    return {S::Completed,
            static_cast<std::uint32_t>(length.original_r0-position)};
}

} // namespace ds_port::original_runtime
