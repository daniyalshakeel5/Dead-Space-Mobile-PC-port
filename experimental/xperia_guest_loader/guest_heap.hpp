#pragma once
#include "guest_elf32.hpp"
#include <cstdint>
#include <cstddef>
#include <unordered_map>

namespace xperia {
// Owned, bounded ARM32 heap. Allocations remain guest pointers;
// never expose the host allocator or an x86 pointer to translated code.
// Used by malloc/calloc/realloc/free import service bridge.
class GuestHeap {
public:
    explicit GuestHeap(GuestElf32& image) noexcept: image_(image) {}
    std::uint32_t allocate(std::uint32_t bytes) noexcept;
    std::uint32_t allocate_zeroed(std::uint32_t count,std::uint32_t size) noexcept;
    std::uint32_t resize(std::uint32_t address,std::uint32_t size) noexcept;
    bool release(std::uint32_t address) noexcept;
    std::size_t live_bytes() const noexcept {return used_;}
    static constexpr std::size_t max_total=64u*1024u*1024u;
    static constexpr std::size_t max_single=16u*1024u*1024u;
private:
    GuestElf32& image_;
    std::uint32_t next_=0x50000000u;
    std::size_t used_=0;
    std::unordered_map<std::uint32_t,std::size_t> sizes_;
};
} // namespace xperia
