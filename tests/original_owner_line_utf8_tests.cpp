#include "ds_port/original_owner_line_utf8.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdint>
#include <string>

using namespace ds_port::original_runtime;
int main(){
    using D=OriginalOwnerUtf8Status;
    using L=OriginalOwnerWideLineStatus;
    const std::uint8_t utf8[]={'A',0xc2,0xa2,0xe2,0x82,0xac,
                               0xf0,0x9f,0x98,0x80};
    auto result=original_decode_stream_utf8_0x26d5fc(
        nullptr,0,utf8,sizeof(utf8));
    assert(result.status==D::Completed);
    assert(result.original_r0_code_units==4);
    assert(result.consumed_source_bytes==sizeof(utf8));
    std::array<char16_t,5> wide{};
    result=original_decode_stream_utf8_0x26d5fc(
        wide.data(),wide.size(),utf8,sizeof(utf8));
    assert(result.status==D::Completed);
    assert(wide[0]==u'A'&&wide[1]==char16_t(0xa2));
    assert(wide[2]==char16_t(0x20ac));
    assert(wide[3]==char16_t(0xf600)); // original one-unit truncation
    assert(wide[4]==0);
    // The original 0x26D5FC returns the number of valid prefix
    // units on malformed input (not -1 as original 0x28CC88).
    const std::uint8_t invalid[]={'X',0xe2,0x41,0xac};
    wide.fill(u'Z');
    result=original_decode_stream_utf8_0x26d5fc(
        wide.data(),wide.size(),invalid,sizeof(invalid));
    assert(result.status==D::StoppedAtInvalidSequence);
    assert(result.original_r0_code_units==1);
    assert(result.consumed_source_bytes==1);
    assert(wide[0]==u'X'&&wide[1]==0);
    const std::uint8_t overlong[]={'Y',0xe0,0x80,0x80,'Z'};
    result=original_decode_stream_utf8_0x26d5fc(
        nullptr,0,overlong,sizeof(overlong));
    assert(result.status==D::StoppedAtInvalidSequence);
    assert(result.original_r0_code_units==1);
    const std::uint8_t four_byte_oob[]={0xf4,0x90,0x80,0x80};
    result=original_decode_stream_utf8_0x26d5fc(
        nullptr,0,four_byte_oob,sizeof(four_byte_oob));
    assert(result.status==D::StoppedAtInvalidSequence);
    assert(result.original_r0_code_units==0);
    const std::uint8_t clipped[]={'Q',0,'R'};
    result=original_decode_stream_utf8_0x26d5fc(
        nullptr,0,clipped,sizeof(clipped));
    assert(result.status==D::Completed);
    assert(result.original_r0_code_units==1);
    assert(result.consumed_source_bytes==1);
    result=original_decode_stream_utf8_0x26d5fc(nullptr,0,nullptr,2);
    assert(result.status==D::InvalidInput);
    result=original_decode_stream_utf8_0x26d5fc(
        nullptr,0,utf8,sizeof(utf8),4);
    assert(result.status==D::ExceedsLimit);
    // Native owner data path: source is the user's game resource
    // data, and the output is real owned UTF-16 text, not a viewer.
    const std::string asset=std::string("KEY: ")+
        std::string("\xE2\x82\xAC",3)+"\r\nNEXT\n";
    OriginalNativeByteSource source{
        reinterpret_cast<const std::uint8_t*>(asset.data()),asset.size(),0
    };
    OriginalNativeUtf16String line{};
    auto read=original_read_wide_line_0x276d18(8,source,line);
    assert(read.status==L::LineDecoded);
    assert(line.contents==u"KEY: \u20ac");
    read=original_read_wide_line_0x276d18(8,source,line);
    assert(read.status==L::LineDecoded&&line.contents==u"NEXT");
    read=original_read_wide_line_0x276d18(8,source,line);
    assert(read.status==L::EndOfStream&&line.contents.empty());
    // The native mode dispatcher cannot substitute its byte reader
    // for original +0x6C UTF-16 mode.
    source.position=0;
    read=original_read_wide_line_0x276d18(0x10,source,line);
    assert(read.status==L::UnsupportedOriginalMode);
    assert(source.position==0);
    const std::string damaged("OK\xC3\x28\n",5);
    source={reinterpret_cast<const std::uint8_t*>(damaged.data()),
            damaged.size(),0};
    read=original_read_wide_line_0x276d18(8,source,line);
    assert(read.status==L::LineDecodedPrefix);
    assert(line.contents==u"OK");
    // Failure to read an overlong original 0x4000-byte record
    // leaves the caller's previous wide output and cursor intact.
    std::string oversized(0x4001,'X');
    oversized+='\n';
    source={reinterpret_cast<const std::uint8_t*>(oversized.data()),
            oversized.size(),0};
    read=original_read_wide_line_0x276d18(8,source,line);
    assert(read.status==L::LineTooLong);
    assert(source.position==0);
    assert(line.contents==u"OK");
    return 0;
}
