#include "ds_port/original_resource_utf8_decode.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdint>
using namespace ds_port::original_runtime;
int main() {
    using S=OriginalUtf8DecodeStatus;
    const std::uint8_t input[]={'A',0xc2,0xa2,0xe2,0x82,0xac,
                                0xf0,0x9f,0x98,0x80};
    auto measured=original_utf8_to_utf16_0x28cc88(
        nullptr,0,input,sizeof(input));
    assert(measured.status==S::Completed&&measured.original_r0==4);
    std::array<char16_t,5> words{};
    const auto converted=original_utf8_to_utf16_0x28cc88(
        words.data(),words.size(),input,sizeof(input));
    assert(converted.status==S::Completed&&converted.original_r0==4);
    assert(words[0]==u'A');
    assert(words[1]==char16_t(0xa2));
    assert(words[2]==char16_t(0x20ac));
    // Original STRH stores a single truncated 16-bit unit for U+1F600,
    // not a surrogate pair. This is a genuine original ARM quirk.
    assert(words[3]==char16_t(0xf600));
    assert(words[4]==char16_t(0));
    OriginalNativeUtf16String owned{};
    assert(original_decode_record_text_0x28611c(
        owned,input,sizeof(input))==S::Completed);
    assert(owned.contents.size()==4u);
    for(unsigned i=0;i<4;++i)assert(owned.contents[i]==words[i]);
    // r2 counts output units including NUL. The decoder returns
    // the *required* count even when the output buffer is smaller.
    std::array<char16_t,2> shortbuf{u'X',u'X'};
    auto short_result=original_utf8_to_utf16_0x28cc88(
        shortbuf.data(),2,input,sizeof(input));
    assert(short_result.status==S::Completed&&short_result.original_r0==4);
    assert(shortbuf[0]==u'A'&&shortbuf[1]==0);
    shortbuf={u'X',u'X'};
    short_result=original_utf8_to_utf16_0x28cc88(
        shortbuf.data(),1,input,sizeof(input));
    assert(short_result.original_r0==4);
    assert(shortbuf[0]==0&&shortbuf[1]==u'X');
    // Original ASCII zero ends conversion, even before original r3 limit.
    const std::uint8_t early[]={'A',0,'B'};
    measured=original_utf8_to_utf16_0x28cc88(
        nullptr,0,early,sizeof(early));
    assert(measured.status==S::Completed&&measured.original_r0==1);
    // Invalid prefix, malformed continuation and truncated sequence
    // all return original r0=-1.
    const std::uint8_t badlead[]={0xc0,0x80};
    measured=original_utf8_to_utf16_0x28cc88(
        nullptr,0,badlead,sizeof(badlead));
    assert(measured.status==S::MalformedOriginalEncoding&&measured.original_r0==-1);
    const std::uint8_t badcontinuation[]={0xe2,0x41,0xac};
    measured=original_utf8_to_utf16_0x28cc88(
        nullptr,0,badcontinuation,sizeof(badcontinuation));
    assert(measured.status==S::MalformedOriginalEncoding&&measured.original_r0==-1);
    const std::uint8_t truncated[]={0xe2,0x82};
    measured=original_utf8_to_utf16_0x28cc88(
        nullptr,0,truncated,sizeof(truncated));
    assert(measured.status==S::MalformedOriginalEncoding&&measured.original_r0==-1);
    // Original 0x28CD80 scalar-range failures EXIT NORMALLY with the
    // units already emitted, unlike invalid continuation which returns -1.
    const std::uint8_t overlong[]={'X',0xe0,0x80,0x80,'Y'};
    measured=original_utf8_to_utf16_0x28cc88(
        nullptr,0,overlong,sizeof(overlong));
    assert(measured.status==S::Completed&&measured.original_r0==1);
    const std::uint8_t beyond_unicode[]={0xf4,0x90,0x80,0x80};
    measured=original_utf8_to_utf16_0x28cc88(
        nullptr,0,beyond_unicode,sizeof(beyond_unicode));
    assert(measured.status==S::Completed&&measured.original_r0==0);
    const std::uint8_t above_lead[]={0xf5,0x80,0x80,0x80};
    measured=original_utf8_to_utf16_0x28cc88(
        nullptr,0,above_lead,sizeof(above_lead));
    assert(measured.status==S::MalformedOriginalEncoding&&measured.original_r0==-1);
    // Native host bounds never read beyond the mapped source and
    // leave an existing destination untouched on invalid conversion.
    assert(original_decode_record_text_0x28611c(
        owned,badcontinuation,sizeof(badcontinuation))==
           S::MalformedOriginalEncoding);
    assert(owned.contents.size()==4);
    measured=original_utf8_to_utf16_0x28cc88(
        nullptr,0,nullptr,3);
    assert(measured.status==S::InvalidHostInput);
    measured=original_utf8_to_utf16_0x28cc88(
        nullptr,0,input,sizeof(input),4);
    assert(measured.status==S::ExceedsHostLimit);
    return 0;
}
