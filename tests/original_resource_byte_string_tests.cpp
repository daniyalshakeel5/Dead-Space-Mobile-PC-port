#include "ds_port/original_resource_byte_string.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <string>
using namespace ds_port::original_runtime;
namespace {
OriginalResourceAppendStatus append(OriginalResourceByteString& buffer,
                                   const char* str,std::size_t len,
                                   std::size_t max=1024) noexcept {
    return original_append_bytes_0x285918(buffer,
        reinterpret_cast<const std::uint8_t*>(str),len,max);
}
}
int main() {
    using S=OriginalResourceAppendStatus;
    OriginalResourceByteString str{};
    assert(append(str,nullptr,0)==S::Completed);
    assert(str.original_capacity_bytes==1u);
    assert(append(str,"AB",2)==S::Completed);
    assert(str.contents=="AB");
    assert(str.original_capacity_bytes==9u); // original minimum 8+NUL
    assert(append(str,"CDEFGH",6)==S::Completed);
    assert(str.contents=="ABCDEFGH");
    assert(str.original_capacity_bytes==9u);
    assert(append(str,"I",1)==S::Completed);
    assert(str.contents=="ABCDEFGHI");
    assert(str.original_capacity_bytes==10u); // exact growth branch
    assert(append(str,"JKLMNOPQR",9)==S::Completed);
    assert(str.contents=="ABCDEFGHIJKLMNOPQR");
    assert(str.original_capacity_bytes==19u); // 9*2+1
    // Original raw buffer can contain embedded NUL; it still maintains
    // a distinct terminator after the last appended byte.
    const char bytes[4]={'a',0,'b','c'};
    OriginalResourceByteString raw{};
    assert(append(raw,bytes,4)==S::Completed);
    assert(raw.contents.size()==4&&raw.contents[1]==0);
    assert(raw.contents.c_str()[4]==0);
    // Aliasing: append data from same native buffer without corruption
    // even when a growth-triggered allocation moves the storage.
    const auto* self=raw.contents.data();
    assert(append(raw,self,4)==S::Completed);
    assert(raw.contents.size()==8&&raw.contents.substr(0,4)==
           raw.contents.substr(4,4));
    assert(append(raw,"x",1)==S::Completed);
    assert(raw.original_capacity_bytes==10u);
    const auto before=raw.contents;
    const auto cap=raw.original_capacity_bytes;
    assert(append(raw,"TOO-LONG",8,9)==S::ExceedsLimit);
    assert(raw.contents==before&&raw.original_capacity_bytes==cap);
    assert(append(raw,nullptr,1)==S::InvalidRange);
    assert(raw.contents==before);
    return 0;
}
