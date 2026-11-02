#include "ds_port/original_embedded_provider.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

using namespace ds_port::original_runtime;
namespace {
void put(std::uint8_t* p,std::uint32_t value) {
    for(unsigned n=0;n<4;++n)p[n]=std::uint8_t(value>>(8*n));
}
std::uint32_t get(const std::uint8_t* p) {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
int fileno_of(std::FILE* f){
#ifdef _WIN32
    return ::_fileno(f);
#else
    return ::fileno(f);
#endif
}
}
int main(){
    using S=OriginalEmbeddedProviderStatus;
    // User-supplied data in a native local file; no copyrighted
    // original asset or ARM code is bundled with this test.
    std::FILE* f=std::tmpfile();
    assert(f!=nullptr);
    constexpr char payload[]="ONE\r\nTWO\nTHREE";
    constexpr std::size_t n=sizeof(payload)-1u;
    assert(std::fwrite(payload,1,n,f)==n);
    assert(std::fflush(f)==0);
    const int fd=fileno_of(f);
    assert(fd>=0);
    std::array<std::uint8_t,0xa8> provider{};
    put(provider.data()+0xa4u,0u); // original file-backed mode
    put(provider.data()+0x10u,static_cast<std::uint32_t>(fd));

    // A bare ARM32 provider descriptor is NOT trusted as a PC fd.
    auto read=original_embedded_provider_read_file_0x2716f8(
        provider.data(),provider.size(),fd+1,nullptr,0);
    assert(read.status==S::InvalidHostFileBinding);
    auto seek=original_embedded_provider_seek_file_0x273034(
        provider.data(),provider.size(),fd+1,0,0);
    assert(seek.status==S::InvalidHostFileBinding);

    seek=original_embedded_provider_seek_file_0x273034(
        provider.data(),provider.size(),fd,0,0);
    assert(seek.status==S::Completed&&seek.original_r0==1u);
    std::array<char,32> out{};
    read=original_embedded_provider_read_file_0x2716f8(
        provider.data(),provider.size(),fd,out.data(),3u);
    assert(read.status==S::Completed&&read.original_r0==3u);
    assert(std::memcmp(out.data(),"ONE",3)==0);
    // Original file-backed size query restores the original read cursor.
    auto length=original_embedded_provider_size_file_0x2763ac(
        provider.data(),provider.size(),fd);
    assert(length.status==S::Completed&&length.original_r0==n);
    length=original_embedded_provider_size_file_0x2763ac(
        provider.data(),provider.size(),fd+1);
    assert(length.status==S::InvalidHostFileBinding);
    // Reading after the size call must continue where the previous
    // read ended, not restart or unexpectedly jump to EOF.
    // Original file position virtual +0x28 is a native
    // lseek(fd,0,SEEK_CUR) when selector=0.
    auto pos=original_embedded_provider_position_file_0x271794(
        provider.data(),provider.size(),fd,0u);
    assert(pos.status==S::Completed&&pos.original_r0==3u);
    pos=original_embedded_provider_position_file_0x271794(
        provider.data(),provider.size(),fd+1,0u);
    assert(pos.status==S::InvalidHostFileBinding);
    read=original_embedded_provider_read_file_0x2716f8(
        provider.data(),provider.size(),fd,out.data(),4u);
    assert(read.status==S::Completed&&read.original_r0==4u);
    assert(std::memcmp(out.data(),"\r\nTW",4)==0);
    // Original file seek uses native lseek(fd,offset,SEEK_SET)
    // in its verified origin-0 branch, and returns boolean success.
    seek=original_embedded_provider_seek_file_0x273034(
        provider.data(),provider.size(),fd,5,0);
    assert(seek.status==S::Completed&&seek.original_r0==1u);
    read=original_embedded_provider_read_file_0x2716f8(
        provider.data(),provider.size(),fd,out.data(),3u);
    assert(read.status==S::Completed&&read.original_r0==3u);
    assert(std::memcmp(out.data(),"TWO",3)==0);
    // Original APK read-only whence table: origin 1 relative to the
    // current cursor, origin 2 relative to end, exactly as ARM lseek.
    seek=original_embedded_provider_seek_file_0x273034(
        provider.data(),provider.size(),fd,-3,1);
    assert(seek.status==S::Completed&&seek.original_r0==1u);
    pos=original_embedded_provider_position_file_0x271794(
        provider.data(),provider.size(),fd,0);
    assert(pos.status==S::Completed&&pos.original_r0==5u);
    seek=original_embedded_provider_seek_file_0x273034(
        provider.data(),provider.size(),fd,-5,2);
    assert(seek.status==S::Completed&&seek.original_r0==1u);
    read=original_embedded_provider_read_file_0x2716f8(
        provider.data(),provider.size(),fd,out.data(),5u);
    assert(read.status==S::Completed&&read.original_r0==5u);
    assert(std::memcmp(out.data(),"THREE",5)==0);
    // Original ARM falls back to SEEK_SET when origin is outside 0..2.
    seek=original_embedded_provider_seek_file_0x273034(
        provider.data(),provider.size(),fd,5,99);
    assert(seek.status==S::Completed&&seek.original_r0==1u);
    pos=original_embedded_provider_position_file_0x271794(
        provider.data(),provider.size(),fd,0);
    assert(pos.status==S::Completed&&pos.original_r0==5u);
    // Original file-mode read returns zero at EOF.
    seek=original_embedded_provider_seek_file_0x273034(
        provider.data(),provider.size(),fd,static_cast<std::int32_t>(n),0);
    assert(seek.status==S::Completed&&seek.original_r0==1u);
    read=original_embedded_provider_read_file_0x2716f8(
        provider.data(),provider.size(),fd,out.data(),3u);
    assert(read.status==S::Completed&&read.original_r0==0u);
    seek=original_embedded_provider_seek_file_0x273034(
        provider.data(),provider.size(),fd,-1,0);
    assert(seek.status==S::Completed&&seek.original_r0==0u);
    assert(get(provider.data()+0xa0u)!=0u); // original errno storage
    seek=original_embedded_provider_seek_file_0x273034(
        provider.data(),provider.size(),fd,0,1);
    assert(seek.status==S::Completed&&seek.original_r0==1u);
    seek=original_embedded_provider_seek_file_0x273034(
        provider.data(),provider.size(),fd,0,2);
    assert(seek.status==S::Completed&&seek.original_r0==1u);
    pos=original_embedded_provider_position_file_0x271794(
        provider.data(),provider.size(),fd,0u);
    assert(pos.status==S::Completed&&pos.original_r0==n);
    // Unsupported original memory-backed mode needs codec.
    put(provider.data()+0xa4u,1u);
    read=original_embedded_provider_read_file_0x2716f8(
        provider.data(),provider.size(),fd,out.data(),4u);
    assert(read.status==S::NeedsOriginalMemoryService);
    put(provider.data()+0xa4u,0u);
    read=original_embedded_provider_read_file_0x2716f8(
        provider.data(),provider.size(),fd,nullptr,4u);
    assert(read.status==S::InvalidHostBuffer);
    put(provider.data()+0x10u,0xffffffffu);
    length=original_embedded_provider_size_file_0x2763ac(
        provider.data(),provider.size(),fd);
    assert(length.status==S::Completed&&length.original_r0==0xffffffffu);
    pos=original_embedded_provider_position_file_0x271794(
        provider.data(),provider.size(),fd,0u);
    assert(pos.status==S::Completed&&pos.original_r0==0xffffffffu);
    read=original_embedded_provider_read_file_0x2716f8(
        provider.data(),provider.size(),fd,out.data(),4u);
    assert(read.status==S::Completed&&read.original_r0==0xffffffffu);
    seek=original_embedded_provider_seek_file_0x273034(
        provider.data(),provider.size(),fd,0,0);
    assert(seek.status==S::Completed&&seek.original_r0==0u);
    read=original_embedded_provider_read_file_0x2716f8(
        provider.data(),0xa7u,fd,out.data(),4u);
    assert(read.status==S::InvalidMappedProvider);
    assert(std::fclose(f)==0);
    return 0;
}
