#include "ds_port/original_receiver_tree.hpp"
namespace ds_port::original_runtime {
namespace {
std::uint32_t read32(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
        (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
bool node(const OriginalReceiverTreeLookupInputs& in,std::uint32_t va,
          std::uint32_t* left,std::uint32_t* right,
          std::uint32_t* first,std::uint32_t* last) noexcept {
    OriginalTreeNodeSpan mapped{};
    if(!va || !in.resolve_node ||
       !in.resolve_node(va,in.context,&mapped) ||
       !mapped.mapped_bytes || mapped.accessible_bytes<0x18u)
        return false;
    const auto* p=static_cast<const std::uint8_t*>(mapped.mapped_bytes);
    *left=read32(p);*right=read32(p+4);
    *first=read32(p+0x10);*last=read32(p+0x14);
    return true;
}
// The original 0x26E670 compares unsigned 16-bit words directly.
// On the native host read the original little-endian 16-bit range
// without relying on the host's char16_t alignment or endianness.
OriginalReceiverTreeStatus compare_node(
    const OriginalReceiverTreeLookupInputs& in,
    std::uint32_t first,std::uint32_t last,
    std::u16string_view needle,int* comparison) noexcept {
    using S=OriginalReceiverTreeStatus;
    if(!first||last<first||((last-first)&1u)||
       (last-first)>65536u)
        return S::InvalidOriginalUtf16Range;
    const std::size_t n=(last-first)/2u;
    OriginalReceiverWideSpan range{};
    if(n && (!in.resolve_utf16 ||
        !in.resolve_utf16(first,in.context,&range)||
        !range.mapped_bytes || range.accessible_bytes<n*2u))
        return S::UnmappedUtf16;
    const auto* p=range.mapped_bytes;
    for(std::size_t k=0;k<n&&k<needle.size();++k){
        const std::uint16_t a=std::uint16_t(p[k*2u])|
            (std::uint16_t(p[k*2u+1u])<<8);
        const std::uint16_t b=static_cast<std::uint16_t>(needle[k]);
        if(a!=b){*comparison=a<b?-1:1;return S::Found;}
    }
    *comparison=n<needle.size()?-1:n>needle.size()?1:0;
    return S::Found;
}
}
int original_compare_utf16_0x26e670(
    std::u16string_view a,std::u16string_view b) noexcept {
    for(std::size_t i=0;i<a.size()&&i<b.size();++i)
        if(a[i]!=b[i])return a[i]<b[i]?-1:1;
    return a.size()<b.size()?-1:a.size()>b.size()?1:0;
}
OriginalReceiverTreeResult original_find_utf16_tree_0x2710a8(
    const OriginalReceiverTreeLookupInputs& in,
    std::u16string_view target) noexcept {
    using S=OriginalReceiverTreeStatus;
    if(!in.original_tree_arm_va||in.original_tree_arm_va>0xfffffffbu||
       !in.mapped_tree||in.tree_accessible_bytes<0x10u||
       !in.resolve_node||!in.resolve_utf16)
        return {S::InvalidTree,0};
    const auto* p=static_cast<const std::uint8_t*>(in.mapped_tree);
    const auto sentinel=in.original_tree_arm_va+4u;
    std::uint32_t current=read32(p+0x0c);
    std::uint32_t candidate=sentinel;
    // ARM 0x2710A8 chooses the candidate for node >= key, then
    // follows the node's +4 link. For node < key, it takes +0.
    for(std::uint32_t count=0;current;++count) {
        if(count>=65536u)return {S::TraversalLimit,0};
        std::uint32_t left=0,right=0,first=0,last=0;
        if(!node(in,current,&left,&right,&first,&last))
            return {S::UnmappedNode,0};
        int cmp=0;
        const auto result=compare_node(in,first,last,target,&cmp);
        if(result!=S::Found)return {result,0};
        if(cmp>=0){candidate=current;current=right;}
        else current=left;
    }
    if(candidate==sentinel)return {S::NotFound,sentinel};
    std::uint32_t left=0,right=0,first=0,last=0;
    if(!node(in,candidate,&left,&right,&first,&last))
        return {S::UnmappedNode,0};
    int cmp=0;
    auto result=compare_node(in,first,last,target,&cmp);
    if(result!=S::Found)return {result,0};
    // ARM 0x271110: CMP result,0 / STRGE candidate /
    // STRLT sentinel. Both exact and lower-bound callers can use it.
    if(cmp<=0)return {S::Found,candidate};
    return {S::NotFound,sentinel};
}
} // namespace ds_port::original_runtime
