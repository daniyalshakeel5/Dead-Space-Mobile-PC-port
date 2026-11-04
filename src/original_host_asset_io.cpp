#include "ds_port/original_host_asset_io.hpp"
#include "ds_port/deadspace_paths.hpp"
#include <algorithm>
#include <cstdint>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace ds_port {
namespace {
void put32(std::uint8_t* at,std::uint32_t value) noexcept {
    for(unsigned i=0;i<4u;++i)at[i]=static_cast<std::uint8_t>(value>>(i*8u));
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
constexpr auto ok=original_runtime::OriginalEmbeddedProviderStatus::Completed;
constexpr auto invalid=original_runtime::OriginalEmbeddedProviderStatus::InvalidHostFileBinding;
}

OriginalHostAssetStream::~OriginalHostAssetStream() noexcept {close();}
void OriginalHostAssetStream::close() noexcept {
    if(file_)std::fclose(file_);
    file_=nullptr;fd_=-1;size_=0;provider_.fill(0);
}
bool OriginalHostAssetStream::open(
    const std::filesystem::path& root,std::string_view asset_path,
    std::size_t maximum_size,bool* file_found) {
    close();
    if(file_found)*file_found=false;
    if(!maximum_size||maximum_size>64u*1024u*1024u)return false;
    const auto path=resolve_game_asset_path(root,asset_path,true);
    if(path.status!=GamePathStatus::Resolved)return false;
    if(file_found)*file_found=true;
    file_=open_native_binary(path.path);
    if(!file_)return false;
    fd_=native_descriptor(file_);
    if(fd_<0){close();return false;}
    // The owned host file descriptor is bound explicitly into a
    // temporary original provider layout (fd +0x10, file-mode +0xA4).
    put32(provider_.data()+0x10u,static_cast<std::uint32_t>(fd_));
    const auto size=original_runtime::original_embedded_provider_size_file_0x2763ac(
        provider_.data(),provider_.size(),fd_);
    if(size.status!=ok||size.original_r0==0xffffffffu||
       size.original_r0>maximum_size){
        close();return false;
    }
    size_=size.original_r0;
    const auto seek_result=seek(0,0);
    if(seek_result.status!=ok||seek_result.original_r0!=1u){
        close();return false;
    }
    return true;
}
original_runtime::OriginalEmbeddedProviderResult OriginalHostAssetStream::read(
    void* destination,std::size_t count) noexcept {
    if(!file_)return {invalid,0};
    return original_runtime::original_embedded_provider_read_file_0x2716f8(
        provider_.data(),provider_.size(),fd_,destination,count);
}
original_runtime::OriginalEmbeddedProviderResult OriginalHostAssetStream::seek(
    std::int32_t offset,std::uint32_t original_origin) noexcept {
    if(!file_)return {invalid,0};
    return original_runtime::original_embedded_provider_seek_file_0x273034(
        provider_.data(),provider_.size(),fd_,offset,original_origin);
}
original_runtime::OriginalEmbeddedProviderResult OriginalHostAssetStream::position(
    std::uint32_t original_selector) noexcept {
    if(!file_)return {invalid,0};
    return original_runtime::original_embedded_provider_position_file_0x271794(
        provider_.data(),provider_.size(),fd_,original_selector);
}
original_runtime::OriginalOwnerWideLineResult OriginalHostAssetStream::read_utf16_line(
    original_runtime::OriginalNativeUtf16String& output) noexcept {
    if(!file_)return {
        original_runtime::OriginalOwnerWideLineStatus::InvalidInput,0,0};
    // Original native text loader in file-backed mode 8.
    // Does not claim original owner record materialization or game readiness.
    return original_runtime::original_read_native_file_line_0x276d18(
        8u,{provider_.data(),provider_.size(),fd_},output);
}

bool read_original_asset_file(
    const std::filesystem::path& root,std::string_view relative_path,
    std::size_t max_bytes,std::vector<std::uint8_t>& bytes,
    bool* file_found) {
    bytes.clear();
    OriginalHostAssetStream file;
    if(!file.open(root,relative_path,max_bytes,file_found))return false;
    const auto length=file.size_bytes();
    if(!length)return false;
    bytes.resize(length);
    std::size_t offset=0;
    while(offset<bytes.size()){
        const auto count=std::min<std::size_t>(
            bytes.size()-offset,1024u*1024u);
        const auto part=file.read(bytes.data()+offset,count);
        if(part.status!=ok||part.original_r0==0u||part.original_r0>count){
            bytes.clear();return false;
        }
        offset+=part.original_r0;
    }
    return true;
}
} // namespace ds_port
