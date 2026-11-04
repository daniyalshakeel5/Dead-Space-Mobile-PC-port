#include "ds_port/original_owner_byte_line.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

using namespace ds_port::original_runtime;
int main() {
    using S=OriginalNativeByteLineStatus;
    using D=OriginalNativeOwnerStreamStatus;
    const std::string raw="FIRST\r\nSECOND\nTHIRD\rFOURTH";
    OriginalNativeByteSource source{
        reinterpret_cast<const std::uint8_t*>(raw.data()),raw.size(),0
    };
    OriginalResourceByteString line{};
    auto read=original_read_byte_line_0x276fe4(source,line);
    assert(read.status==S::LineReady&&line.contents=="FIRST");
    assert(read.consumed_bytes==7&&source.position==7);
    read=original_read_byte_line_0x276fe4(source,line);
    assert(read.status==S::LineReady&&line.contents=="SECOND");
    read=original_read_byte_line_0x276fe4(source,line);
    assert(read.status==S::LineReady&&line.contents=="THIRD");
    read=original_read_byte_line_0x276fe4(source,line);
    assert(read.status==S::LineReady&&line.contents=="FOURTH");
    const auto total=source.position;
    assert(total==raw.size());
    read=original_read_byte_line_0x276fe4(source,line);
    assert(read.status==S::EndOfStream&&read.consumed_bytes==0);
    assert(line.contents.empty());
    // The original 0x276FE4 calls +0x34 with a 64-byte buffer.
    // Preserve the line through multiple reads and the next line.
    std::string many(140,'x');
    many+="\r\nLAST\n";
    source={reinterpret_cast<const std::uint8_t*>(many.data()),many.size(),0};
    read=original_read_byte_line_0x276fe4(source,line);
    assert(read.status==S::LineReady&&line.contents==std::string(140,'x'));
    assert(read.consumed_bytes==142&&source.position==142);
    read=original_read_byte_line_0x276fe4(source,line);
    assert(read.status==S::LineReady&&line.contents=="LAST");
    assert(source.position==many.size());
    // Empty text lines from consecutive newlines remain individual
    // records, including CRLF across the 64-byte chunk boundary.
    const std::string blank="\r\n\n\rB\n";
    source={reinterpret_cast<const std::uint8_t*>(blank.data()),blank.size(),0};
    for(int i=0;i<3;++i){
        read=original_read_byte_line_0x276fe4(source,line);
        assert(read.status==S::LineReady&&line.contents.empty());
    }
    read=original_read_byte_line_0x276fe4(source,line);
    assert(read.status==S::LineReady&&line.contents=="B");
    // Embedded zero is preserved in the native dynamic byte string,
    // since original 0x276EAC appends a bounded byte range.
    const std::string binary("A\0B\n",4);
    source={reinterpret_cast<const std::uint8_t*>(binary.data()),binary.size(),0};
    read=original_read_byte_line_0x276fe4(source,line);
    assert(read.status==S::LineReady&&line.contents.size()==3);
    assert(line.contents[1]=='\0'&&line.contents[2]=='B');
    // Exactly 0x4000 bytes is the observed native owner stream
    // limit; the next byte must fail with no source advancement.
    std::string exact(0x4000,'q');
    exact+='\n';
    source={reinterpret_cast<const std::uint8_t*>(exact.data()),exact.size(),0};
    read=original_read_byte_line_0x276fe4(source,line);
    assert(read.status==S::LineReady&&line.contents.size()==0x4000u);
    assert(source.position==exact.size());
    exact.insert(exact.begin(),'!');
    source={reinterpret_cast<const std::uint8_t*>(exact.data()),exact.size(),0};
    line.contents="PRESERVE";
    const auto saved=line.original_capacity_bytes;
    read=original_read_byte_line_0x276fe4(source,line);
    assert(read.status==S::LineTooLong&&source.position==0);
    assert(line.contents=="PRESERVE"&&line.original_capacity_bytes==saved);
    read=original_read_byte_line_0x276fe4(source,line,0x4001);
    assert(read.status==S::InvalidSource&&source.position==0);
    // The original +0x70 mode 8 uses the byte reader; other modes
    // branch to unported +0x6C UTF-16 or other processing.
    source={reinterpret_cast<const std::uint8_t*>(raw.data()),raw.size(),0};
    auto disp=original_dispatch_owner_line_0x276d18(0x10,source,line);
    assert(disp.status==D::UnsupportedOriginalMode&&source.position==0);
    disp=original_dispatch_owner_line_0x276d18(8,source,line);
    assert(disp.status==D::LineReadyAwaitingOwnerPostprocess&&line.contents=="FIRST");
    assert(disp.consumed_bytes==7);
    source.position=source.total_bytes+1;
    read=original_read_byte_line_0x276fe4(source,line);
    assert(read.status==S::InvalidSource);
    source={nullptr,1,0};
    read=original_read_byte_line_0x276fe4(source,line);
    assert(read.status==S::InvalidSource);
    return 0;
}
