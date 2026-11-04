#include "ds_port/original_resource_utf8_decode.hpp"
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

namespace ds_port::original_runtime {
OriginalUtf8DecodeResult original_utf8_to_utf16_0x28cc88(
    char16_t* output,std::size_t capacity,const std::uint8_t* src,
    std::size_t length,std::size_t limit) noexcept {
    using S=OriginalUtf8DecodeStatus;
    if(length>limit || length>std::size_t(std::numeric_limits<std::int32_t>::max()-1))
        return {S::ExceedsHostLimit,-1};
    if(length && !src)return {S::InvalidHostInput,-1};
    // Verified original numeric tables (0x28CCA0 lookup and range
    // blocks accessed at 0x28CD70..0x28CD88). These are mathematical
    // validation constants, not original game code or binary assets.
    constexpr std::uint32_t offsets[]={0,0,12416u,925824u,63447168u};
    constexpr std::uint32_t minimum[]={0,0,128u,2048u,65536u};
    constexpr std::uint32_t maximum[]={0,0,2048u,65536u,1114112u};
    std::size_t produced=0,stored=0;
    std::size_t i=0;
    auto finish=[&]() noexcept -> OriginalUtf8DecodeResult {
        // Original 0x28CCEC..0x28CCF4 appends UTF-16 terminator when
        // r0!=0 and r2!=0, even if output capacity was insufficient.
        if(output && capacity>0)output[stored]=0;
        return {S::Completed,static_cast<std::int32_t>(produced)};
    };
    while(i<length) {
        const std::uint8_t lead=src[i];
        if(lead==0)break; // original terminates at first ASCII NUL
        unsigned width=1;
        if(lead<0x80) {
            width=1;
        } else if(lead>=0xc2 && lead<=0xdf) {
            width=2;
        } else if(lead>=0xe0 && lead<=0xef) {
            width=3;
        } else if(lead>=0xf0 && lead<=0xf4) {
            width=4;
        } else {
            return {S::MalformedOriginalEncoding,-1};
        }
        if(width>length-i)return {S::MalformedOriginalEncoding,-1};
        std::uint32_t value=lead;
        for(unsigned extra=1;extra<width;++extra) {
            const std::uint8_t next=src[i+extra];
            if(next<0x80||next>0xbf)
                return {S::MalformedOriginalEncoding,-1};
            // Original ADD ip, continuation, ip LSL #6 retains
            // the high marker bits and then subtracts the offset.
            value=value*64u+next;
        }
        if(width>1) {
            value-=offsets[width];
            // Original 0x28CD80/0x28CD88 branch to its normal end,
            // rather than return -1, for a noncanonical scalar range.
            if(value<minimum[width]||value>=maximum[width])
                return finish();
        }
        // Original STRHHI IP,[r0],#2 stores one 16-bit unit, including
        // for four-byte sequences: do NOT synthesize surrogate pairs.
        if(output && capacity>produced+1u)
            output[stored++]=static_cast<char16_t>(value);
        ++produced;
        i+=width;
    }
    return finish();
}
OriginalUtf8DecodeStatus original_decode_record_text_0x28611c(
    OriginalNativeUtf16String& out,const std::uint8_t* source,
    std::size_t length,std::size_t limit) noexcept {
    using S=OriginalUtf8DecodeStatus;
    const auto measured=original_utf8_to_utf16_0x28cc88(
        nullptr,0,source,length,limit);
    if(measured.status!=S::Completed)return measured.status;
    try {
        const auto n=static_cast<std::size_t>(measured.original_r0);
        std::u16string copy(n,char16_t(0));
        // r2=needed+1 in the original second call. std::u16string
        // owns an additional trailing NUL slot without resizing.
        const auto converted=original_utf8_to_utf16_0x28cc88(
            copy.data(),n+1u,source,length,limit);
        if(converted.status!=S::Completed ||
           converted.original_r0!=measured.original_r0)
            return S::MalformedOriginalEncoding;
        out.contents.swap(copy);
        return S::Completed;
    } catch(const std::bad_alloc&) {
        return S::ExceedsHostLimit;
    } catch(const std::length_error&) {
        return S::ExceedsHostLimit;
    } catch(...) {
        return S::ExceedsHostLimit;
    }
}
} // namespace ds_port::original_runtime
