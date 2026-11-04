#include "ds_port/original_native_resource_map.hpp"
#include <algorithm>
#include <new>
namespace ds_port::original_runtime {
bool OriginalUnsignedByteLess::operator()(const std::string& a,
                                          const std::string& b) const noexcept {
    return std::lexicographical_compare(
        a.begin(),a.end(),b.begin(),b.end(),
        [](char x,char y) noexcept {
            return static_cast<unsigned char>(x) <
                   static_cast<unsigned char>(y);
        });
}
bool OriginalNativeResourceMap::replace(
    std::string_view key,std::string_view descriptor,
    std::uint32_t arm_va) noexcept {
    if(key.empty()||!arm_va)return false;
    try {
        std::string owned_key(key);
        OriginalNativeResourceEntry record{std::string(descriptor),arm_va};
        std::lock_guard<std::mutex> guard(mutex_);
        // The ARM code first removes a preexisting key, then inserts
        // a new pair. Preserve those observable semantics, but use
        // the native STL allocator instead of allocating ARM nodes.
        auto it=entries_.find(owned_key);
        if(it!=entries_.end()) {
            it->second=std::move(record);
            ++replacements_;
        } else {
            entries_.emplace(std::move(owned_key),std::move(record));
        }
        ++writes_;
        return true;
    } catch(const std::bad_alloc&) {
        return false;
    } catch(...) {
        return false;
    }
}
bool OriginalNativeResourceMap::find(
    std::string_view key,OriginalNativeResourceEntry* out) const noexcept {
    if(!out)return false;
    try {
        const std::string owned(key);
        std::lock_guard<std::mutex> guard(mutex_);
        const auto it=entries_.find(owned);
        if(it==entries_.end())return false;
        *out=it->second;
        return true;
    } catch(...) {
        return false;
    }
}
std::size_t OriginalNativeResourceMap::count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return entries_.size();
}
std::size_t OriginalNativeResourceMap::successful_writes() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return writes_;
}
std::size_t OriginalNativeResourceMap::replacement_count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return replacements_;
}
} // namespace ds_port::original_runtime
