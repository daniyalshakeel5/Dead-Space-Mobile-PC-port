#include "ds_port/original_receiver_prefix.hpp"
#include <new>
#include <string>
namespace ds_port::original_runtime {
namespace {
std::uint32_t read32(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
bool table_valid(const OriginalRegistrationCharacterTable& t) noexcept {
    return t.mapped_got_word&&t.got_word_bytes>=4u&&
           t.original_table_arm_va&&t.mapped_table_bytes&&
           t.mapped_table_size>=kOriginalRegistrationCharMapMinBytes&&
           read32(static_cast<const std::uint8_t*>(t.mapped_got_word))==
           t.original_table_arm_va;
}
}
OriginalReceiverPrefixResult0x278018
original_execute_receiver_prefix_0x278018(
    const OriginalReceiverPrefixInputs0x278018& in,
    std::u16string_view input) noexcept {
    using S=OriginalReceiverPrefixStatus0x278018;
    auto fail=[](S s,std::uint32_t matched=0) noexcept {
        return OriginalReceiverPrefixResult0x278018{s,matched,0};
    };
    if(!in.mapped_owner||!in.original_owner_arm_va||
       in.original_owner_arm_va>0xfffff743u||
       in.owner_accessible_bytes<0x8ccu)
        return fail(S::InvalidOriginalOwner);
    if(input.size()>32768u)
        return fail(S::InvalidOriginalText);
    const auto* owner=static_cast<const std::uint8_t*>(in.mapped_owner);
    // ARM 0x278064..0x278078: original flag at owner+0x8b6.
    // When zero it jumps to an independent path at 0x2786F0.
    if(owner[0x8b6u]==0)
        return fail(S::UnreconstructedAlternateMode);
    if(!table_valid(in.original_character_table))
        return fail(S::InvalidOriginalCharacterTable);
    try {
        std::u16string normalized(input);
        // ARM 0x27808C..0x2780B4: LDRH text unit, when <=0xff
        // LDRH table+2*unit+2, overwrite that same UTF-16 unit.
        const auto* map=in.original_character_table.mapped_table_bytes;
        for(auto& value:normalized) {
            const auto index=static_cast<std::uint16_t>(value);
            if(index<=0xffu) {
                const auto off=std::size_t(index)*2u+2u;
                value=static_cast<char16_t>(
                    std::uint16_t(map[off])|
                    (std::uint16_t(map[off+1])<<8));
            }
        }
        // ARM 0x2780B8..0x2780CC:
        // 0x2710A8(&found,owner+0x8bc,&normalized)
        OriginalReceiverTreeLookupInputs lookup{
            in.original_owner_arm_va+0x8bcu,
            owner+0x8bcu,in.owner_accessible_bytes-0x8bcu,
            in.resolve_node,in.resolve_utf16,in.context};
        const auto found=original_find_utf16_tree_0x2710a8(
            lookup,normalized);
        if(found.status!=OriginalReceiverTreeStatus::Found &&
           found.status!=OriginalReceiverTreeStatus::NotFound)
            return fail(S::LookupBlocked);
        if(found.status==OriginalReceiverTreeStatus::NotFound){
            // ARM 0x2780D8..0x27814C: if mode owner+0x8b5=0,
            // virtual+0x28 must run before return. Only the original
            // nonzero mode with direct r0=0 is implemented.
            if(owner[0x8b5u]==0)
                return fail(S::UnreconstructedOwnerReset);
            return {S::CompletedNoMatch,0,0};
        }
        // ARM 0x2780DC+ requires an original provider virtual+0x2c,
        // owner +0x70, allocation and materialization. Do not return
        // a fabricated success on a matching parsed resource.
        return fail(S::UnreconstructedMatchedReceiver,
                    found.original_node_arm_va);
    } catch(const std::bad_alloc&){
        return fail(S::AllocationBlocked);
    } catch(...){
        return fail(S::AllocationBlocked);
    }
}
} // namespace ds_port::original_runtime
