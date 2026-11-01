#include "ds_port/original_record_insert.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdint>
#include <string>
using namespace ds_port::original_runtime;
namespace {
struct Fixture {unsigned calls=0;bool refuse=false;std::u16string last{};};
bool receiver(std::uint32_t owner,std::uint32_t slot,
              std::u16string_view text,std::uint32_t tag,
              std::uint32_t argument,void* context) noexcept {
    auto& f=*static_cast<Fixture*>(context);
    ++f.calls;
    assert(owner==0x3000u&&slot==0x58u);
    assert(tag==0x4000u&&argument==0x5000u);
    try{f.last.assign(text);}catch(...){return false;}
    return !f.refuse;
}
}
int main() {
    using S=OriginalRecordInsertStatus0x2862e8;
    Fixture f{};
    // The original UTF-16 source is mapped from the original APK.
    // Synthetic fixture bytes are not proprietary game contents.
    const std::uint8_t wide[]={ 'S',0,'E',0,'E',0,'D',0 };
    const std::uint8_t utf8[]={ 'H','E','L','L','O' };
    OriginalRecordInsertInputs0x2862e8 in{
        0x3000u,0x4000u,0x5000u,
        wide,sizeof(wide),utf8,sizeof(utf8),receiver,&f
    };
    auto result=original_insert_record_0x2862e8(in);
    assert(result.status==S::CompletedCallSequence);
    assert(result.decoded_code_units==5&&f.calls==1);
    assert(f.last==u"HELLO"); // 0x28611C replaces literal, not appends
    f.last.clear();
    const std::uint8_t high[]={0xe2,0x82,0xac};
    in.original_record_utf8=high;in.record_utf8_bytes=sizeof(high);
    result=original_insert_record_0x2862e8(in);
    assert(result.status==S::CompletedCallSequence);
    assert(result.decoded_code_units==1&&f.last==u"\u20ac");
    // The engine receiver method +0x58 remains unported and MUST
    // fail closed when no real native method is supplied.
    in.original_virtual_0x58=nullptr;
    result=original_insert_record_0x2862e8(in);
    assert(result.status==S::MissingOriginalReceiver&&f.calls==2);
    in.original_virtual_0x58=receiver;
    f.refuse=true;
    result=original_insert_record_0x2862e8(in);
    assert(result.status==S::OriginalReceiverRefused&&f.calls==3);
    f.refuse=false;
    in.tag_literal_bytes=3; // odd byte count is not valid UTF-16 range
    result=original_insert_record_0x2862e8(in);
    assert(result.status==S::OriginalLiteralBlocked&&f.calls==3);
    in.tag_literal_bytes=sizeof(wide);
    const std::uint8_t bad[]={0xe2,0x82};
    in.original_record_utf8=bad;in.record_utf8_bytes=sizeof(bad);
    result=original_insert_record_0x2862e8(in);
    assert(result.status==S::OriginalRecordTextBlocked&&f.calls==3);
    in.original_receiver_arm_va=0;
    result=original_insert_record_0x2862e8(in);
    assert(result.status==S::InvalidOriginalInput&&f.calls==3);
    return 0;
}
