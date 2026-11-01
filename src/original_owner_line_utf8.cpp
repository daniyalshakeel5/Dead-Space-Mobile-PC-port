#include "ds_port/original_owner_line_utf8.hpp"
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

namespace ds_port::original_runtime {
OriginalOwnerUtf8Result original_decode_stream_utf8_0x26d5fc(
    char16_t* output,std::size_t capacity,
    const std::uint8_t* input,std::size_t count,
    std::size_t limit) noexcept {
    using S=OriginalOwnerUtf8Status;
    if(count>limit || count>std::numeric_limits<std::uint32_t>::max() ||
       (count&&!input))
        return {count>limit?S::ExceedsLimit:S::InvalidInput,0,0};
    // Original table at 0x26D614 (width) and 0x26D618 (ranges)
    // uses the same scalar boundaries as 0x28CC88, recovered
    // INDEPENDENTLY from the original libEAMGameDeadSpace.so.
    // This is a direct scalar recreation, not a codec dependency.
    constexpr std::uint32_t offsets[]={0,0,12416u,925824u,63447168u};
    constexpr std::uint32_t minimum[]={0,0,128u,2048u,65536u};
    constexpr std::uint32_t maximum[]={0,128u,2048u,65536u,1114112u};
    std::size_t units=0,pos=0;
    auto finish=[&](S state) noexcept -> OriginalOwnerUtf8Result {
        // Original exit 0x26D70C writes a UTF-16 terminator where
        // space is available. std::u16string additionally maintains
        // its own NUL; raw caller buffers are bounded here.
        if(output&&capacity)
            output[units<capacity?units:capacity-1u]=0;
        return {state,units,pos};
    };
    while(pos<count) {
        const auto start=pos;
        const std::uint8_t lead=input[pos];
        if(!lead)break;
        unsigned width=1;
        if(lead<0x80u)width=1;
        else if(lead>=0xc2u&&lead<=0xdfu)width=2;
        else if(lead>=0xe0u&&lead<=0xefu)width=3;
        else if(lead>=0xf0u&&lead<=0xf4u)width=4;
        else return finish(S::StoppedAtInvalidSequence);
        if(width>count-pos)return finish(S::StoppedAtInvalidSequence);
        std::uint32_t scalar=lead;
        for(unsigned i=1;i<width;++i){
            const std::uint8_t next=input[start+i];
            if(next<0x80u||next>0xbfu)
                return finish(S::StoppedAtInvalidSequence);
            scalar=scalar*64u+next;
        }
        if(width>1){
            scalar-=offsets[width];
            if(scalar<minimum[width]||scalar>=maximum[width])
                return finish(S::StoppedAtInvalidSequence);
        }
        // ARM 0x26D6F4 STRHHI: one 16-bit unit, including valid
        // four-byte UTF-8 scalars (truncated to 16 bits), matching
        // the game's actual historical behavior.
        if(output&&units+1u<capacity)
            output[units]=static_cast<char16_t>(scalar);
        ++units;
        pos+=width;
    }
    return finish(S::Completed);
}
OriginalOwnerWideLineResult original_read_wide_line_0x276d18(
    std::uint32_t mode,OriginalNativeByteSource& source,
    OriginalNativeUtf16String& out) noexcept {
    using S=OriginalOwnerWideLineStatus;
    if(mode!=8u)return {S::UnsupportedOriginalMode,0,0};
    // Transactional source/owned UTF-16 storage: when an allocation
    // fails or the original 0x4000-byte line limit is exceeded,
    // no cursor change or partially decoded record escapes.
    const auto old_position=source.position;
    OriginalResourceByteString bytes{};
    const auto line=original_read_byte_line_0x276fe4(source,bytes);
    if(line.status==OriginalNativeByteLineStatus::EndOfStream){
        out.contents.clear();
        return {S::EndOfStream,0,0};
    }
    if(line.status!=OriginalNativeByteLineStatus::LineReady){
        source.position=old_position;
        return {line.status==OriginalNativeByteLineStatus::LineTooLong?
                S::LineTooLong:line.status==OriginalNativeByteLineStatus::OutOfMemory?
                S::AllocationFailed:S::InvalidInput,0,0};
    }
    const auto* raw=reinterpret_cast<const std::uint8_t*>(bytes.contents.data());
    const auto measured=original_decode_stream_utf8_0x26d5fc(
        nullptr,0,raw,bytes.contents.size());
    if(measured.status!=OriginalOwnerUtf8Status::Completed &&
       measured.status!=OriginalOwnerUtf8Status::StoppedAtInvalidSequence){
        source.position=old_position;
        return {S::InvalidInput,0,0};
    }
    try{
        std::u16string decoded(measured.original_r0_code_units,char16_t(0));
        const auto converted=original_decode_stream_utf8_0x26d5fc(
            decoded.data(),decoded.size()+1u,raw,bytes.contents.size());
        if(converted.status!=measured.status ||
           converted.original_r0_code_units!=measured.original_r0_code_units){
            source.position=old_position;
            return {S::InvalidInput,0,0};
        }
        out.contents.swap(decoded);
        return {measured.status==OriginalOwnerUtf8Status::Completed?
                S::LineDecoded:S::LineDecodedPrefix,
                line.consumed_bytes,out.contents.size()};
    }catch(const std::bad_alloc&){
        source.position=old_position;
        return {S::AllocationFailed,0,0};
    }catch(const std::length_error&){
        source.position=old_position;
        return {S::LineTooLong,0,0};
    }catch(...){
        source.position=old_position;
        return {S::AllocationFailed,0,0};
    }
}
} // namespace ds_port::original_runtime
