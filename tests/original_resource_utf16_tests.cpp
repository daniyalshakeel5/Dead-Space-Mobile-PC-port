#include "ds_port/original_resource_utf16.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
using namespace ds_port::original_runtime;
int main() {
    using S=OriginalNativeUtf16Status;
    OriginalNativeUtf16String text{};
    const std::uint8_t words[]={0x41,0x00,0xff,0x00,0x34,0xd8,0x1e,0xdd};
    assert(original_copy_utf16_range_0x285590(
        text,words,sizeof(words))==S::Completed);
    assert(text.contents.size()==4u);
    assert(text.contents[0]==u'A'&&text.contents[1]==char16_t(0xff));
    assert(text.contents[2]==char16_t(0xd834));
    assert(text.contents[3]==char16_t(0xdd1e));
    assert(text.contents.c_str()[4]==0);
    const auto old=text.contents;
    assert(original_copy_utf16_range_0x285590(
        text,words,7)==S::InvalidOriginalRange);
    assert(original_copy_utf16_range_0x285590(
        text,nullptr,2)==S::InvalidOriginalRange);
    assert(original_copy_utf16_range_0x285590(
        text,words,sizeof(words),3)==S::ExceedsLimit);
    assert(text.contents==old); // invalid ranges cannot corrupt prior data
    assert(original_copy_utf16_range_0x285590(
        text,nullptr,0)==S::Completed);
    assert(text.contents.empty());
    assert(text.contents.c_str()[0]==0);
    return 0;
}
