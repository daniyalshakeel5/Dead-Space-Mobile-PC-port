#include "ds_port/original_embedded_provider.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdint>

using namespace ds_port::original_runtime;
namespace {
void put(std::uint8_t* p,std::uint32_t x) {
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(x>>(8u*i));
}
}
int main() {
    using S=OriginalEmbeddedProviderStatus;
    std::array<std::uint8_t,0xa8> buffer{};
    auto status=original_embedded_provider_error_0x26c3cc(
        buffer.data(),buffer.size());
    assert(status.status==S::Completed&&status.original_r0==0);
    put(buffer.data()+0xa0u,0xdeadbeefu);
    status=original_embedded_provider_error_0x26c3cc(
        buffer.data(),buffer.size());
    assert(status.status==S::Completed&&status.original_r0==0xdeadbeefu);
    status=original_embedded_provider_error_0x26c3cc(
        buffer.data(),0xa3u);
    assert(status.status==S::InvalidMappedProvider);

    // Memory-backed original provider: +0xA4==1, +4 carries a
    // non-null stream, +8 is cached size, +0xC is current cursor.
    put(buffer.data()+0xa4u,1u);
    put(buffer.data()+4u,0x2000u);
    put(buffer.data()+8u,150u);
    put(buffer.data()+0x0cu,25u);
    auto size=original_embedded_provider_size_0x2763ac(
        buffer.data(),buffer.size());
    assert(size.status==S::Completed&&size.original_r0==150u);
    auto seek=original_embedded_provider_seek_0x273034(
        buffer.data(),buffer.size(),25u,0u);
    assert(seek.status==S::Completed&&seek.original_r0==1u);
    assert(original_embedded_provider_position_0x271794(
        buffer.data(),buffer.size(),0u).original_r0==25u);
    // Original r2==1 adds the current position, then compares.
    seek=original_embedded_provider_seek_0x273034(
        buffer.data(),buffer.size(),0u,1u);
    assert(seek.status==S::Completed&&seek.original_r0==1u);
    // Original r2==2 adds provider cached size [+8].
    seek=original_embedded_provider_seek_0x273034(
        buffer.data(),buffer.size(),
        static_cast<std::uint32_t>(-125),2u);
    assert(seek.status==S::Completed&&seek.original_r0==1u);
    // The nontrivial seek calls original state allocation+codec and
    // cannot be approximated as success or change its cursor.
    seek=original_embedded_provider_seek_0x273034(
        buffer.data(),buffer.size(),26u,0u);
    assert(seek.status==S::NeedsOriginalMemoryService);
    assert(original_embedded_provider_position_0x271794(
        buffer.data(),buffer.size(),0u).original_r0==25u);
    // Unknown original in-memory size invokes 0x26C26C/0x27627C.
    put(buffer.data()+8u,0xffffffffu);
    size=original_embedded_provider_size_0x2763ac(
        buffer.data(),buffer.size());
    assert(size.status==S::NeedsOriginalMemoryService);
    seek=original_embedded_provider_seek_0x273034(
        buffer.data(),buffer.size(),0u,2u);
    assert(seek.status==S::Completed&&seek.original_r0==0u);
    // Null memory stream reports original length -1.
    put(buffer.data()+4u,0u);
    size=original_embedded_provider_size_0x2763ac(
        buffer.data(),buffer.size());
    assert(size.status==S::Completed&&size.original_r0==0xffffffffu);
    // A file-backed provider with invalid original descriptor
    // returns failure on seek and -1 on size.
    put(buffer.data()+0xa4u,0u);
    put(buffer.data()+0x10u,0xffffffffu);
    size=original_embedded_provider_size_0x2763ac(
        buffer.data(),buffer.size());
    assert(size.status==S::Completed&&size.original_r0==0xffffffffu);
    seek=original_embedded_provider_seek_0x273034(
        buffer.data(),buffer.size(),0u,0u);
    assert(seek.status==S::Completed&&seek.original_r0==0u);
    put(buffer.data()+0x10u,4u);
    size=original_embedded_provider_size_0x2763ac(
        buffer.data(),buffer.size());
    assert(size.status==S::NeedsOriginalFileService);
    seek=original_embedded_provider_seek_0x273034(
        buffer.data(),buffer.size(),15u,0u);
    assert(seek.status==S::NeedsOriginalFileService);
    seek=original_embedded_provider_seek_0x273034(
        buffer.data(),0xa7u,0u,0u);
    assert(seek.status==S::InvalidMappedProvider);
    return 0;
}
