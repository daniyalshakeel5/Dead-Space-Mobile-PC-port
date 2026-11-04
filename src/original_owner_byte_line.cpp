#include "ds_port/original_owner_byte_line.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

namespace ds_port::original_runtime {
OriginalNativeByteLineResult original_read_byte_line_0x276fe4(
    OriginalNativeByteSource& source,
    OriginalResourceByteString& output,
    std::size_t limit) noexcept {
    using S=OriginalNativeByteLineStatus;
    if((source.total_bytes && !source.mapped_bytes) ||
       source.position>source.total_bytes ||
       limit>0x4000u || output.original_capacity_bytes==0)
        return {S::InvalidSource,0,0};

    const auto before=source.position;
    if(before==source.total_bytes){
        output.contents.clear();
        return {S::EndOfStream,0,0};
    }

    // The original +0x68 resets its byte-string at entry and
    // reads at most 0x40 bytes from provider+0x34 per iteration.
    // Stage the result to avoid corrupting the caller on failure.
    OriginalResourceByteString pending{};
    std::size_t cursor=before;
    bool got_newline=false;
    while(cursor<source.total_bytes) {
        const auto available=source.total_bytes-cursor;
        const auto chunk=std::min<std::size_t>(0x40u,available);
        const auto* bytes=source.mapped_bytes+cursor;
        std::size_t segment=0;
        while(segment<chunk && bytes[segment]!=0x0au &&
              bytes[segment]!=0x0du)
            ++segment;
        if(segment) {
            if(pending.contents.size()>limit ||
               segment>limit-pending.contents.size())
                return {S::LineTooLong,0,0};
            const auto appended=original_append_bytes_0x285918(
                pending,bytes,segment,limit);
            if(appended==OriginalResourceAppendStatus::AllocationFailed)
                return {S::OutOfMemory,0,0};
            if(appended!=OriginalResourceAppendStatus::Completed)
                return {S::InvalidSource,0,0};
        }
        cursor+=segment;
        if(segment<chunk) {
            // Original 0x2770F4 rewinds the provider to the
            // detected line boundary; +0x34 then consumes the
            // terminator, and +0x2C puts back an unrelated byte.
            // Equivalent host cursor keeps the next line intact.
            const auto terminator=source.mapped_bytes[cursor++];
            if(terminator==0x0du && cursor<source.total_bytes &&
               source.mapped_bytes[cursor]==0x0au)
                ++cursor;
            got_newline=true;
            break;
        }
        // The full 0x40-byte chunk was appended. Original
        // 0x2770D0 queries provider position and reads another.
    }
    (void)got_newline;
    output.contents.swap(pending.contents);
    output.original_capacity_bytes=pending.original_capacity_bytes;
    source.position=cursor;
    return {S::LineReady,cursor-before,output.contents.size()};
}
OriginalNativeOwnerStreamResult original_dispatch_owner_line_0x276d18(
    std::uint32_t owner_mode,OriginalNativeByteSource& source,
    OriginalResourceByteString& output) noexcept {
    using S=OriginalNativeOwnerStreamStatus;
    // Verified original 0x276D48/0x276E04 branches: modes
    // 0x10,0x4b0,0x4b1 use owner vtable+0x6c (UTF-16).
    // The original constructor starts with mode 8, routed
    // through +0x68 / 0x276FE4.
    if(owner_mode!=8u)return {S::UnsupportedOriginalMode,0};
    const auto read=original_read_byte_line_0x276fe4(source,output);
    switch(read.status){
        case OriginalNativeByteLineStatus::LineReady:
            return {S::LineReadyAwaitingOwnerPostprocess,read.consumed_bytes};
        case OriginalNativeByteLineStatus::EndOfStream:
            return {S::EndOfStream,0};
        case OriginalNativeByteLineStatus::LineTooLong:
            return {S::LineTooLong,0};
        case OriginalNativeByteLineStatus::OutOfMemory:
            return {S::OutOfMemory,0};
        case OriginalNativeByteLineStatus::InvalidSource:
            return {S::InvalidSource,0};
    }
    return {S::InvalidSource,0};
}
} // namespace ds_port::original_runtime
