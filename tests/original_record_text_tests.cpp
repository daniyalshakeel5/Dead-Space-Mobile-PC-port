#include "ds_port/original_record_text.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
using namespace ds_port::original_runtime;
namespace {
OriginalRecordReplaceStatus replace(OriginalRecordUtf16Buffer& target,
                                   const char* s) noexcept {
    return original_replace_record_text_0x28611c(
        target,reinterpret_cast<const std::uint8_t*>(s),
        std::strlen(s));
}
}
int main(){
    using S=OriginalRecordReplaceStatus;
    OriginalRecordUtf16Buffer record{};
    assert(replace(record,"HELLO")==S::Completed);
    assert(record.contents==u"HELLO");
    assert(record.original_capacity_code_units==9);
    assert(replace(record,"HI")==S::Completed);
    assert(record.contents==u"HI");
    assert(record.original_capacity_code_units==9);
    assert(replace(record,"ABCDEFGH")==S::Completed);
    assert(record.contents==u"ABCDEFGH");
    assert(record.original_capacity_code_units==9);
    assert(replace(record,"ABCDEFGHI")==S::Completed);
    assert(record.contents==u"ABCDEFGHI");
    assert(record.original_capacity_code_units==10);
    assert(replace(record,"ABCDEFGHIJKLMNOPQR")==S::Completed);
    assert(record.contents==u"ABCDEFGHIJKLMNOPQR");
    assert(record.original_capacity_code_units==19);
    assert(replace(record,"")==S::Completed);
    assert(record.contents.empty());
    assert(record.original_capacity_code_units==19);
    assert(record.contents.c_str()[0]==0);
    // Non-ASCII: original 0x28CC88 truncates 4-byte code points
    // into a single UTF-16 unit instead of a surrogate pair.
    const std::uint8_t input[]={0xe2,0x82,0xac,0xf0,0x9f,0x98,0x80};
    assert(original_replace_record_text_0x28611c(
        record,input,sizeof(input))==S::Completed);
    assert(record.contents.size()==2);
    assert(record.contents[0]==char16_t(0x20ac));
    assert(record.contents[1]==char16_t(0xf600));
    assert(record.original_capacity_code_units==19);
    const auto prior=record.contents;
    const auto capacity=record.original_capacity_code_units;
    const std::uint8_t corrupt[]={0xe2,0x80};
    assert(original_replace_record_text_0x28611c(
        record,corrupt,sizeof(corrupt))==S::MalformedEncoding);
    assert(record.contents==prior&&record.original_capacity_code_units==capacity);
    assert(original_replace_record_text_0x28611c(
        record,nullptr,2)==S::InvalidInput);
    assert(record.contents==prior);
    record.original_capacity_code_units=0;
    assert(replace(record,"safe")==S::InvalidDestination);
    assert(record.contents==prior);
    record.original_capacity_code_units=capacity;
    const std::uint8_t too_long[]={ 'A','B','C','D','E'};
    assert(original_replace_record_text_0x28611c(
        record,too_long,sizeof(too_long),4)==S::ExceedsLimit);
    assert(record.contents==prior);
    return 0;
}
