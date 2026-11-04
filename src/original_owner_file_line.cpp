#include "ds_port/original_owner_file_line.hpp"
#include "ds_port/original_resource_byte_string.hpp"
#include <array>
#include <climits>
#include <new>
#include <stdexcept>
#include <string>

namespace ds_port::original_runtime {
OriginalOwnerWideLineResult original_read_native_file_line_0x276d18(
    std::uint32_t mode,const OriginalNativeFileLineSource& file,
    OriginalNativeUtf16String& output) noexcept {
    using S=OriginalOwnerWideLineStatus;
    using P=OriginalEmbeddedProviderStatus;
    if(mode!=8u)return {S::UnsupportedOriginalMode,0,0};
    if(!file.mapped_original_embedded_provider ||
       file.provider_accessible_bytes<0xa8u || file.native_host_fd<0)
        return {S::InvalidInput,0,0};
    const auto start=original_embedded_provider_position_file_0x271794(
        file.mapped_original_embedded_provider,
        file.provider_accessible_bytes,file.native_host_fd,0u);
    if(start.status!=P::Completed ||
       start.original_r0==0xffffffffu ||
       start.original_r0>static_cast<std::uint32_t>(INT_MAX))
        return {S::InvalidInput,0,0};
    auto reset=[&]() noexcept {
        const auto undo=original_embedded_provider_seek_file_0x273034(
            file.mapped_original_embedded_provider,
            file.provider_accessible_bytes,file.native_host_fd,
            static_cast<std::int32_t>(start.original_r0),0u);
        return undo.status==P::Completed && undo.original_r0==1u;
    };
    OriginalResourceByteString pending{};
    std::array<std::uint8_t,0x40u> chunk{};
    bool has_line=false;
    for(;;) {
        // The original owner +0x68 reads at most 0x40 bytes via
        // provider +0x34. Its host-file branch calls libc read.
        const auto read=original_embedded_provider_read_file_0x2716f8(
            file.mapped_original_embedded_provider,
            file.provider_accessible_bytes,file.native_host_fd,
            chunk.data(),chunk.size());
        if(read.status!=P::Completed || read.original_r0==0xffffffffu ||
           read.original_r0>chunk.size()){
            reset();return {S::InvalidInput,0,0};
        }
        const std::size_t count=read.original_r0;
        if(!count) {
            if(!has_line && pending.contents.empty()) {
                output.contents.clear();
                return {S::EndOfStream,0,0};
            }
            break;
        }
        has_line=true;
        std::size_t text_length=0;
        while(text_length<count &&
              chunk[text_length]!='\r' && chunk[text_length]!='\n')
            ++text_length;
        if(text_length) {
            if(text_length>0x4000u-pending.contents.size()) {
                reset();return {S::LineTooLong,0,0};
            }
            const auto appended=original_append_bytes_0x285918(
                pending,chunk.data(),text_length,0x4000u);
            if(appended!=OriginalResourceAppendStatus::Completed){
                reset();
                return {appended==OriginalResourceAppendStatus::AllocationFailed?
                        S::AllocationFailed:S::LineTooLong,0,0};
            }
        }
        if(text_length==count)continue;
        // read() advanced by the entire chunk. The original +0x68
        // uses provider +0x28 (position) and +0x2C (seek) to
        // preserve unread bytes after the line delimiter.
        const auto after=original_embedded_provider_position_file_0x271794(
            file.mapped_original_embedded_provider,
            file.provider_accessible_bytes,file.native_host_fd,0u);
        if(after.status!=P::Completed || after.original_r0==0xffffffffu ||
           after.original_r0<count-text_length-1u){
            reset();return {S::InvalidInput,0,0};
        }
        const auto next_position=
            after.original_r0-static_cast<std::uint32_t>(count-text_length-1u);
        if(next_position>static_cast<std::uint32_t>(INT_MAX)){
            reset();return {S::InvalidInput,0,0};
        }
        const auto positioned=original_embedded_provider_seek_file_0x273034(
            file.mapped_original_embedded_provider,
            file.provider_accessible_bytes,file.native_host_fd,
            static_cast<std::int32_t>(next_position),0u);
        if(positioned.status!=P::Completed || positioned.original_r0!=1u){
            reset();return {S::InvalidInput,0,0};
        }
        // CRLF is one line ending, including when split across
        // original 64-byte provider reads.
        if(chunk[text_length]=='\r') {
            std::uint8_t lookahead=0;
            const auto peek=original_embedded_provider_read_file_0x2716f8(
                file.mapped_original_embedded_provider,
                file.provider_accessible_bytes,file.native_host_fd,
                &lookahead,1u);
            if(peek.status!=P::Completed ||
               peek.original_r0==0xffffffffu) {
                reset();return {S::InvalidInput,0,0};
            }
            if(peek.original_r0==1u && lookahead!='\n'){
                const auto putback=original_embedded_provider_seek_file_0x273034(
                    file.mapped_original_embedded_provider,
                    file.provider_accessible_bytes,file.native_host_fd,
                    static_cast<std::int32_t>(next_position),0u);
                if(putback.status!=P::Completed||putback.original_r0!=1u) {
                    reset();return {S::InvalidInput,0,0};
                }
            }
        }
        break;
    }
    const auto* bytes=reinterpret_cast<const std::uint8_t*>(
        pending.contents.data());
    const auto decoded=original_decode_stream_utf8_0x26d5fc(
        nullptr,0,bytes,pending.contents.size());
    if(decoded.status!=OriginalOwnerUtf8Status::Completed &&
       decoded.status!=OriginalOwnerUtf8Status::StoppedAtInvalidSequence){
        reset();return {S::InvalidInput,0,0};
    }
    try {
        std::u16string text(decoded.original_r0_code_units,char16_t(0));
        const auto second=original_decode_stream_utf8_0x26d5fc(
            text.data(),text.size()+1u,bytes,pending.contents.size());
        if(second.status!=decoded.status ||
           second.original_r0_code_units!=decoded.original_r0_code_units){
            reset();return {S::InvalidInput,0,0};
        }
        output.contents.swap(text);
        const auto finish=original_embedded_provider_position_file_0x271794(
            file.mapped_original_embedded_provider,
            file.provider_accessible_bytes,file.native_host_fd,0u);
        if(finish.status!=P::Completed ||
           finish.original_r0<start.original_r0){
            reset();return {S::InvalidInput,0,0};
        }
        return {decoded.status==OriginalOwnerUtf8Status::Completed?
                S::LineDecoded:S::LineDecodedPrefix,
                finish.original_r0-start.original_r0,
                output.contents.size()};
    } catch(const std::bad_alloc&) {
        reset();return {S::AllocationFailed,0,0};
    } catch(const std::length_error&) {
        reset();return {S::LineTooLong,0,0};
    } catch(...) {
        reset();return {S::AllocationFailed,0,0};
    }
}
} // namespace ds_port::original_runtime
