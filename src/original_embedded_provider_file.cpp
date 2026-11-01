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
    // ARM 0x273058..0x273068 maps original origin=0 to
    // whence 0, then calls the PLT at 0x35350 -> libc lseek.
    // The original mapping of origins 1 and 2 uses an embedded
    // table and must be recovered before enabling them.
    if(origin!=0u)return {S::NeedsOriginalFileService,0};
    errno=0;
#ifdef _WIN32
    const auto at=::_lseek(host_fd,static_cast<long>(offset),SEEK_SET);
#else
    const auto at=::lseek(host_fd,static_cast<off_t>(offset),SEEK_SET);
#endif
    if(at<0){
        // ARM 0x27310C..0x273118 obtains errno via original
        // 0x34FFC and stores it to provider+0xA0, then r0=0.
        store32(p+0xa0u,static_cast<std::uint32_t>(errno));
        return {S::Completed,0};
    }
    return {S::Completed,1};
}
} // namespace ds_port::original_runtime
