#include "ds_port/original_resource_token_lookup.hpp"
#include <algorithm>
#include <new>
#include <string>
namespace ds_port::original_runtime {
namespace {
std::uint32_t read32(const void* ptr) noexcept {
    const auto* p=static_cast<const std::uint8_t*>(ptr);
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
bool valid_table(const OriginalRegistrationCharacterTable& t) noexcept {
    return t.mapped_got_word && t.got_word_bytes>=4 &&
           t.original_table_arm_va && t.mapped_table_bytes &&
           t.mapped_table_size>=kOriginalRegistrationCharMapMinBytes &&
           read32(t.mapped_got_word)==t.original_table_arm_va;
}
}
bool OriginalNativeResourceTokenIndex::insert_stored(
    std::string_view key) noexcept {
    if(key.size()>16384u)return false;
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        keys_.emplace(key);
        return true;
    } catch(...) {
        return false;
    }
}
bool OriginalNativeResourceTokenIndex::contains(
    std::string_view key) const noexcept {
    try {
        const std::string owned(key);
        std::lock_guard<std::mutex> lock(mutex_);
        return keys_.find(owned)!=keys_.end();
    } catch(...) {
        return false;
    }
}
std::size_t OriginalNativeResourceTokenIndex::count() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return keys_.size();
}
OriginalTokenLookupResult original_token_lookup_0x285630(
    const OriginalNativeResourceTokenIndex* index,
    OriginalRegistrationCharacterTable table,
    const std::uint8_t* name,std::size_t accessible_bytes) noexcept {
    using S=OriginalTokenLookupStatus;
    if(!index)return {S::MissingOriginalIndex,0};
    if(!valid_table(table))
        return {S::InvalidOriginalCharacterTable,0};
    if(!name||accessible_bytes==0)
        return {S::InvalidOriginalName,0};
    const auto limit=std::min<std::size_t>(accessible_bytes,16385u);
    const auto* end=std::find(name,name+limit,std::uint8_t(0));
    if(end==name+limit)return {S::UnterminatedOriginalName,0};
    const auto length=static_cast<std::size_t>(end-name);
    if(length>16384u)return {S::UnterminatedOriginalName,0};
    try {
        std::string copied;
        copied.reserve(length);
        // Original 0x2856A0..0x2856BC: LDRB, LDRSH +2, STRB.
        for(std::size_t i=0;i<length;++i) {
            const unsigned ch=name[i];
            copied.push_back(static_cast<char>(
                table.mapped_table_bytes[ch*2u+2u]));
        }
        // Original 0x2854B8 uses memcmp(min(lenA,lenB)) followed
        // by length comparison: same semantics as unsigned byte less.
        return {index->contains(copied)?S::Found:S::NotFound,length};
    } catch(...) {
        return {S::AllocationFailed,0};
    }
}
} // namespace ds_port::original_runtime
