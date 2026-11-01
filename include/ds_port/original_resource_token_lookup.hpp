#pragma once
#include "ds_port/original_resource_registration.hpp"
#include "ds_port/original_native_resource_map.hpp"
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <set>
#include <string>
#include <string_view>

namespace ds_port::original_runtime {
// Native equivalent of the original 0x2854B8 sorted byte-string tree
// search used by 0x285630. Original ARM tree-node addresses are replaced
// with owned strings. Entries must originate in real parsed game data.
class OriginalNativeResourceTokenIndex {
public:
    // Insert already stored key bytes as emitted by original tree producer;
    // do NOT silently lowercase stored keys. Only the 0x285630 query
    // path converts input through the game's 16-bit char map.
    bool insert_stored(std::string_view key) noexcept;
    bool contains(std::string_view key) const noexcept;
    std::size_t count() const noexcept;
private:
    mutable std::mutex mutex_;
    std::set<std::string,OriginalUnsignedByteLess> keys_;
};
enum class OriginalTokenLookupStatus : std::uint8_t {
    Found, NotFound, MissingOriginalIndex, InvalidOriginalCharacterTable,
    InvalidOriginalName, UnterminatedOriginalName, AllocationFailed
};
struct OriginalTokenLookupResult {
    OriginalTokenLookupStatus status;
    std::size_t name_bytes;
};
// Original 0x285630: copy a bounded NUL-terminated name, replace each
// byte with LDRSH [table+unsigned_byte*2+2] truncated to 8 bits,
// search original 0x2854B8 tree using unsigned-byte lexicographic order,
// and report whether the result != original tree header+4.
OriginalTokenLookupResult original_token_lookup_0x285630(
    const OriginalNativeResourceTokenIndex* original_index,
    OriginalRegistrationCharacterTable original_character_table,
    const std::uint8_t* mapped_name,std::size_t accessible_bytes) noexcept;
} // namespace ds_port::original_runtime
