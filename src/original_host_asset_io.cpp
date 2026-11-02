#include "ds_port/original_host_asset_io.hpp"
#include "ds_port/deadspace_paths.hpp"
#include "ds_port/original_embedded_provider.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <memory>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace ds_port {
namespace {
void put32(std::uint8_t* at,std::uint32_t value) noexcept {
    for(unsigned i=0;i<4;++i)at[i]=static_cast<std::uint8_t>(value>>(i*8u));
}
std::FILE* open_native_binary(const std::filesystem::path& path) {
#ifdef _WIN32
    std::FILE* stream=nullptr;
    if(_wfopen_s(&stream,path.c_str(),L"rb")!=0)return nullptr;
    return stream;
#else
    return std::fopen(path.c_str(),"rb");
#endif
}
int native_descriptor(std::FILE* stream) noexcept {
#ifdef _WIN32
    return ::_fileno(stream);
#else
    return ::fileno(stream);
#endif
}
} // namespace
bool read_original_asset_file(
    const std::filesystem::path& root,std::string_view relative_path,
    std::size_t max_bytes,std::vector<std::uint8_t>& bytes,
    bool* file_found) {
    bytes.clear();
    if(file_found)*file_found=false;
    if(max_bytes==0 || max_bytes>64u*1024u*1024u)return false;
    const auto resolved=resolve_game_asset_path(root,relative_path,true);
    if(resolved.status!=GamePathStatus::Resolved)return false;
    if(file_found)*file_found=true;
    std::unique_ptr<std::FILE,decltype(&std::fclose)> stream(
        open_native_binary(resolved.path),&std::fclose);
    if(!stream)return false;
    const int fd=native_descriptor(stream.get());
    if(fd<0)return false;
    // The original file-backed provider has fd at +0x10 and mode
    // at +0xA4. This is an explicit native bridge: never interpret
    // an untrusted ARM pointer or original fd as a host descriptor.
    std::array<std::uint8_t,0xa8u> provider{};
    put32(provider.data()+0x10u,static_cast<std::uint32_t>(fd));
    put32(provider.data()+0xa4u,0u);
    using original_runtime::OriginalEmbeddedProviderStatus;
    constexpr auto ok=OriginalEmbeddedProviderStatus::Completed;
    const auto size=original_runtime::original_embedded_provider_size_file_0x2763ac(
        provider.data(),provider.size(),fd);
    if(size.status!=ok || size.original_r0==0xffffffffu ||
       size.original_r0==0 || size.original_r0>max_bytes)
        return false;
    const auto start=original_runtime::original_embedded_provider_seek_file_0x273034(
        provider.data(),provider.size(),fd,0,0);
    if(start.status!=ok || start.original_r0!=1)return false;
    bytes.resize(size.original_r0);
    std::size_t offset=0;
    while(offset<bytes.size()){
        const std::size_t n=std::min<std::size_t>(bytes.size()-offset,1024u*1024u);
        const auto result=original_runtime::original_embedded_provider_read_file_0x2716f8(
            provider.data(),provider.size(),fd,bytes.data()+offset,n);
        if(result.status!=ok || result.original_r0==0 ||
           result.original_r0>n) {
            bytes.clear();
            return false;
        }
        offset+=result.original_r0;
    }
    return true;
}
} // namespace ds_port
