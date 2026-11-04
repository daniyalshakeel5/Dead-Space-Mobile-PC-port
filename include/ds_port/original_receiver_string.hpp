#pragma once
#include "ds_port/original_default_pool.hpp"
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {
// Original native 0x27541C constructor called three times during
// the provider path of receiver activation 0x278E58.
// Verified original slot is shared global 0x0049BCA8
// (user APK evidence run 41337608259). Object size is 0x21C.
inline constexpr std::size_t kOriginalReceiverStringBytes=0x21cu;
struct OriginalReceiverStringInputs0x27541c {
    std::uint32_t original_string_arm_va=0;
    void* mapped_string=nullptr;
    std::size_t accessible_bytes=0;
    const OriginalDefaultPoolInputs0x271914* native_allocator_pool=nullptr;
};
enum class OriginalReceiverStringStatus0x27541c:std::uint8_t {
    Completed, InvalidMemory, MissingAllocator, OriginalAllocatorBlocked
};
struct OriginalReceiverStringResult0x27541c {
    OriginalReceiverStringStatus0x27541c status;
    std::uint32_t original_allocator_va;
};
// Original 0x27541C builds a native-mapped fixed inline UTF-16
// string. It uses the real static-singleton allocator rather than
// fabricated ARM addresses or compatibility-layer object pointers.
OriginalReceiverStringResult0x27541c
original_construct_receiver_string_0x27541c(
    const OriginalReceiverStringInputs0x27541c&) noexcept;
} // namespace ds_port::original_runtime
