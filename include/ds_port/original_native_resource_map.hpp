#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
namespace ds_port::original_runtime {
// Original 0x2838EC and 0x2837B4 compare byte strings using memcmp
// and byte length. The original tree uses an ARM32 node allocator;
// this is a native x86-64 ordered tree of the same observable pairs.
// It is not a reconstruction of original physical ARM32 node layouts.
struct OriginalUnsignedByteLess {
    bool operator()(const std::string& lhs,
                    const std::string& rhs) const noexcept;
};
struct OriginalNativeResourceEntry {
    std::string descriptor;
    std::uint32_t original_descriptor_arm_va;
};
class OriginalNativeResourceMap {
public:
    // 0x2839C8 lookup -> optional 0x283F5C old-node erase ->
    // 0x2837B4 insertion. Duplicate keys replace old descriptors.
    bool replace(std::string_view converted_key,
                 std::string_view descriptor,
                 std::uint32_t original_descriptor_arm_va) noexcept;
    bool find(std::string_view converted_key,
              OriginalNativeResourceEntry* result) const noexcept;
    std::size_t count() const noexcept;
    std::size_t successful_writes() const noexcept;
    std::size_t replacement_count() const noexcept;
private:
    mutable std::mutex mutex_;
    std::map<std::string,OriginalNativeResourceEntry,OriginalUnsignedByteLess> entries_;
    std::size_t writes_ = 0;
    std::size_t replacements_ = 0;
};
} // namespace ds_port::original_runtime
