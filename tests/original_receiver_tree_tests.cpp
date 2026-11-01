#include "ds_port/original_receiver_tree.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdint>
using namespace ds_port::original_runtime;
namespace {
void put(std::uint8_t* p,std::uint32_t n){
    for(unsigned i=0;i<4;++i)p[i]=static_cast<std::uint8_t>(n>>(8*i));
}
struct OriginalMappedFixture {
    std::array<std::uint8_t,16> tree{};
    std::array<std::array<std::uint8_t,0x18>,3> nodes{};
    std::array<std::array<std::uint8_t,10>,3> text{};
    bool lose_text=false;
    bool lose_node=false;
};
bool nodes(std::uint32_t va,void* context,OriginalTreeNodeSpan* out) noexcept {
    auto& f=*static_cast<OriginalMappedFixture*>(context);
    if(f.lose_node&&va==0x5000)return false;
    const std::uint32_t addresses[3]={0x3000,0x4000,0x5000};
    for(unsigned i=0;i<3;++i)if(va==addresses[i]){
        *out={f.nodes[i].data(),f.nodes[i].size()};return true;
    }
    return false;
}
bool texts(std::uint32_t va,void* context,
           OriginalReceiverWideSpan* out) noexcept {
    auto& f=*static_cast<OriginalMappedFixture*>(context);
    if(f.lose_text&&va==0x6000)return false;
    const std::uint32_t addresses[3]={0x6100,0x6200,0x6000};
    for(unsigned i=0;i<3;++i)if(va==addresses[i]){
        *out={f.text[i].data(),f.text[i].size()};return true;
    }
    return false;
}
void setup(OriginalMappedFixture& f) {
    // Original 0x2710A8 tree root at +0x0c. For its comparator
    // branch, +0 is the greater-than link and +4 is less-than.
    put(f.tree.data()+0x0c,0x3000);
    put(f.nodes[0].data()+0,0x4000);
    put(f.nodes[0].data()+4,0x5000);
    const std::uint32_t src[]={0x6100,0x6200,0x6000};
    constexpr const char* keys[]={"beta","gamma","alpha"};
    for(unsigned i=0;i<3;++i) {
        put(f.nodes[i].data()+0x10,src[i]);
        put(f.nodes[i].data()+0x14,src[i]+(i==0?8u:10u));
        for(unsigned k=0;k<(i==0?4u:5u);++k){
            f.text[i][k*2]=static_cast<std::uint8_t>(keys[i][k]);
            f.text[i][k*2+1]=0;
        }
    }
}
}
int main() {
    assert(original_compare_utf16_0x26e670(u"alpha",u"alpha")==0);
    assert(original_compare_utf16_0x26e670(u"alp",u"alpha")<0);
    assert(original_compare_utf16_0x26e670(u"alpha",u"alp")>0);
    assert(original_compare_utf16_0x26e670(u"\uffff",u"z")>0);
    OriginalMappedFixture f{};
    setup(f);
    OriginalReceiverTreeLookupInputs in{
        0x1000,f.tree.data(),f.tree.size(),nodes,texts,&f};
    for(const auto& expected:
        {std::pair<std::u16string_view,std::uint32_t>{u"alpha",0x5000},
         {u"beta",0x3000},{u"gamma",0x4000}}){
        const auto result=original_find_utf16_tree_0x2710a8(in,expected.first);
        assert(result.status==OriginalReceiverTreeStatus::Found);
        assert(result.original_node_arm_va==expected.second);
    }
    auto result=original_find_utf16_tree_0x2710a8(in,u"bravo");
    assert(result.status==OriginalReceiverTreeStatus::NotFound);
    assert(result.original_node_arm_va==0x1004);
    result=original_find_utf16_tree_0x2710a8(in,u"theta");
    assert(result.status==OriginalReceiverTreeStatus::NotFound);
    f.lose_text=true;
    result=original_find_utf16_tree_0x2710a8(in,u"alpha");
    assert(result.status==OriginalReceiverTreeStatus::UnmappedUtf16);
    f.lose_text=false;
    f.lose_node=true;
    result=original_find_utf16_tree_0x2710a8(in,u"alpha");
    assert(result.status==OriginalReceiverTreeStatus::UnmappedNode);
    f.lose_node=false;
    put(f.nodes[2].data()+0x14,0x6009);
    result=original_find_utf16_tree_0x2710a8(in,u"alpha");
    assert(result.status==OriginalReceiverTreeStatus::InvalidOriginalUtf16Range);
    put(f.nodes[2].data()+0x14,0x600a);
    put(f.nodes[0].data()+4,0x3000);
    result=original_find_utf16_tree_0x2710a8(in,u"alpha");
    assert(result.status==OriginalReceiverTreeStatus::TraversalLimit);
    put(f.nodes[0].data()+4,0x5000);
    put(f.tree.data()+0x0c,0);
    result=original_find_utf16_tree_0x2710a8(in,u"alpha");
    assert(result.status==OriginalReceiverTreeStatus::NotFound);
    in.tree_accessible_bytes=12;
    result=original_find_utf16_tree_0x2710a8(in,u"alpha");
    assert(result.status==OriginalReceiverTreeStatus::InvalidTree);
    return 0;
}
