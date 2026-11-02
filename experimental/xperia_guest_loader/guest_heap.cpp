#include "guest_heap.hpp"
#include <algorithm>
#include <climits>
#include <cstring>

namespace xperia {
std::uint32_t GuestHeap::allocate(std::uint32_t n) noexcept {
    const auto bytes=std::max<std::uint32_t>(n,1u);
    const auto aligned=(std::uint64_t(bytes)+15u)&~std::uint64_t(15);
    if(bytes>max_single||aligned>max_total-used_||
       std::uint64_t(next_)+aligned>0x60000000ull||
       sizes_.size()>=16384)return 0;
    const auto address=next_;
    try {
        // GuestElf32's existing sparse PT_LOAD scheme also handles
        // separately owned guest heap segments. No direct host pointer
        // is visible to ARM or stored in an import relocation.
        GuestSegment seg;
        seg.start=address;
        seg.contents.resize(bytes,0);
        image_.segments.push_back(std::move(seg));
        try {
            sizes_.emplace(address,bytes);
        }catch(...){
            image_.segments.pop_back();
            return 0;
        }
        next_+=static_cast<std::uint32_t>(aligned);
        used_+=aligned;
        return address;
    }catch(...){return 0;}
}
std::uint32_t GuestHeap::allocate_zeroed(std::uint32_t count,
                                         std::uint32_t bytes) noexcept {
    const std::uint64_t total=std::uint64_t(count)*bytes;
    if(total>UINT32_MAX||total>max_single)return 0;
    // std::vector value-initialization ensures calloc zero-fill.
    return allocate(static_cast<std::uint32_t>(total));
}
bool GuestHeap::release(std::uint32_t address) noexcept {
    if(address==0)return true; // free(NULL)
    const auto it=sizes_.find(address);
    if(it==sizes_.end())return false;
    for(auto seg=image_.segments.begin();seg!=image_.segments.end();++seg){
        if(seg->start!=address)continue;
        const auto aligned=(it->second+15u)&~std::size_t(15);
        image_.segments.erase(seg);
        sizes_.erase(it);
        used_-=aligned;
        return true;
    }
    return false;
}
std::uint32_t GuestHeap::resize(std::uint32_t address,
                                std::uint32_t bytes) noexcept {
    if(!address)return allocate(bytes);
    if(!bytes) {
        release(address);
        return 0;
    }
    const auto old=sizes_.find(address);
    if(old==sizes_.end()||bytes>max_single)return 0;
    const auto old_bytes=old->second;
    const auto* from=image_.read(address,std::min<std::size_t>(bytes,old_bytes));
    if(!from)return 0;
    // Need enough space for both allocations during copy.
    const auto fresh=allocate(bytes);
    if(!fresh)return 0;
    auto* target=image_.writeable(fresh,std::min<std::size_t>(bytes,old_bytes));
    const auto* origin=image_.read(address,std::min<std::size_t>(bytes,old_bytes));
    if(!target||!origin) {
        release(fresh);
        return 0;
    }
    std::memcpy(target,origin,std::min<std::size_t>(bytes,old_bytes));
    if(!release(address)) {
        release(fresh);
        return 0;
    }
    return fresh;
}
} // namespace xperia
