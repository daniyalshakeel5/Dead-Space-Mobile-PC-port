#include "ds_port/original_receiver_prefix.hpp"
#include "ds_port/original_record_insert.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdint>
using namespace ds_port::original_runtime;
namespace {
void put(std::uint8_t* p,std::uint32_t x) {
    for(unsigned n=0;n<4;++n)p[n]=std::uint8_t(x>>(8*n));
}
struct Fixture {
    std::array<std::uint8_t,0x8cc> owner{};
    std::array<std::uint8_t,0x18> node{};
    std::array<std::uint8_t,4> got{};
    std::array<std::uint8_t,kOriginalRegistrationCharMapMinBytes> chartable{};
    std::array<std::uint8_t,2> text{'a',0};
};
bool nodes(std::uint32_t va,void* context,OriginalTreeNodeSpan* s) noexcept {
    auto& f=*static_cast<Fixture*>(context);
    if(va!=0x3000)return false;
    *s={f.node.data(),f.node.size()};return true;
}
bool wide(std::uint32_t va,void* context,OriginalReceiverWideSpan* s) noexcept{
    auto& f=*static_cast<Fixture*>(context);
    if(va!=0x4000)return false;
    *s={f.text.data(),f.text.size()};return true;
}
void init(Fixture& f) {
    f.owner[0x8b5]=1; // original nonzero mode bypasses owner reset
    f.owner[0x8b6]=1; // original UTF-16 receiver branch enabled
    put(f.got.data(),0x7000);
    for(unsigned i=0;i<256;++i){
        const unsigned k=i>='A'&&i<='Z'?i+('a'-'A'):i;
        f.chartable[i*2u+2u]=static_cast<std::uint8_t>(k);
    }
    put(f.node.data()+0x10,0x4000);
    put(f.node.data()+0x14,0x4002);
}
OriginalReceiverPrefixInputs0x278018 inputs(Fixture& f) {
    return {0x2000,f.owner.data(),f.owner.size(),
            {f.got.data(),f.got.size(),0x7000,f.chartable.data(),
             f.chartable.size()},
            nodes,wide,&f};
}
}
int main() {
    using S=OriginalReceiverPrefixStatus0x278018;
    Fixture f{};init(f);
    auto in=inputs(f);
    // Empty original receiver tree => original r0=0 without
    // executing the unreconstructed 0x278018 matched subroutines.
    auto out=original_execute_receiver_prefix_0x278018(in,u"A");
    assert(out.status==S::CompletedNoMatch&&out.original_r0==0);
    // Real mapped original ARM tree root at owner+0x8c8.
    put(f.owner.data()+0x8c8,0x3000);
    out=original_execute_receiver_prefix_0x278018(in,u"A");
    assert(out.status==S::UnreconstructedMatchedReceiver);
    assert(out.original_tree_match_va==0x3000);
    // Character table is not optional: game-specific byte folding.
    out=original_execute_receiver_prefix_0x278018(in,u"B");
    assert(out.status==S::CompletedNoMatch);
    f.owner[0x8b5]=0;
    out=original_execute_receiver_prefix_0x278018(in,u"B");
    assert(out.status==S::UnreconstructedOwnerReset);
    f.owner[0x8b5]=1;
    f.owner[0x8b6]=0;
    out=original_execute_receiver_prefix_0x278018(in,u"B");
    assert(out.status==S::UnreconstructedAlternateMode);
    f.owner[0x8b6]=1;
    const auto original_va=in.original_character_table.original_table_arm_va;
    in.original_character_table.original_table_arm_va=0x9999;
    out=original_execute_receiver_prefix_0x278018(in,u"B");
    assert(out.status==S::InvalidOriginalCharacterTable);
    in.original_character_table.original_table_arm_va=original_va;
    in.owner_accessible_bytes=0x8cb;
    out=original_execute_receiver_prefix_0x278018(in,u"B");
    assert(out.status==S::InvalidOriginalOwner);
    in.owner_accessible_bytes=f.owner.size();
    // End-to-end direct connection to native 0x2862E8 record
    // preparation. Original 0x2862E8 ignores receiver r0.
    f.owner[0x8b5]=1;f.owner[0x8b6]=1;
    const std::uint8_t wide_tag[]={'S',0};
    const std::uint8_t utf8_no_match[]={'B'};
    OriginalRecordInsertInputs0x2862e8 record{
        0x2000,0x4000,0x5000,
        wide_tag,sizeof(wide_tag),
        utf8_no_match,sizeof(utf8_no_match),nullptr,&f,&in};
    auto inserted=original_insert_record_0x2862e8(record);
    assert(inserted.status==
           OriginalRecordInsertStatus0x2862e8::CompletedOriginalNoMatch);
    assert(inserted.decoded_code_units==1);
    const std::uint8_t utf8_match[]={'A'};
    record.original_record_utf8=utf8_match;
    record.record_utf8_bytes=sizeof(utf8_match);
    inserted=original_insert_record_0x2862e8(record);
    assert(inserted.status==
           OriginalRecordInsertStatus0x2862e8::OriginalReceiverRefused);
    return 0;
}
