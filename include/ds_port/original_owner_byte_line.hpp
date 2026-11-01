#pragma once
#include "ds_port/original_resource_byte_string.hpp"
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {

// Evidence: original owner vtable +0x68 -> ARM 0x276FE4, and
// +0x70 -> 0x276D18 for the default owner mode 8.
// 0x276FE4 reads 0x40-byte chunks from provider virtual +0x34,
// queries position at +0x28 and seeks at +0x2C on CR/LF.
// The host source owns NO copyrighted data; callers supply
// previously extracted user-owned resource bytes.
struct OriginalNativeByteSource {
    const std::uint8_t* mapped_bytes=nullptr;
    std::size_t total_bytes=0;
    std::size_t position=0;
};

enum class OriginalNativeByteLineStatus : std::uint8_t {
    LineReady, EndOfStream, InvalidSource, LineTooLong,
    OutOfMemory
};
struct OriginalNativeByteLineResult {
    OriginalNativeByteLineStatus status;
    std::size_t consumed_bytes;
    std::size_t line_bytes;
};

// Native equivalent of original 0x276FE4 line splitting from an
// already-mapped resource. This does NOT claim the original provider
// virtual methods +0x28/+0x2C/+0x34 or their file backends are ported.
// Preserves the original observed 64-byte chunking, maximum 0x4000
// line bytes (observed in original owner +0x70), and CR/LF handling.
// Writes into the existing native 0x285918 dynamic byte-string.
OriginalNativeByteLineResult original_read_byte_line_0x276fe4(
    OriginalNativeByteSource& source,
    OriginalResourceByteString& output,
    std::size_t maximum_line_bytes=0x4000u) noexcept;

// Dispatch only the *originally verified* default mode=8 owner
// branch in 0x276D18. Other modes require original UTF-16 +0x6C
// or alternate reader paths and return an explicit blocked status.
enum class OriginalNativeOwnerStreamStatus : std::uint8_t {
    LineReady, EndOfStream, InvalidSource, UnsupportedOriginalMode,
    LineTooLong, OutOfMemory
};
struct OriginalNativeOwnerStreamResult {
    OriginalNativeOwnerStreamStatus status;
    std::size_t consumed_bytes;
};
OriginalNativeOwnerStreamResult original_dispatch_owner_line_0x276d18(
    std::uint32_t original_owner_mode_0x8b0,
    OriginalNativeByteSource& source,
    OriginalResourceByteString& output) noexcept;

} // namespace ds_port::original_runtime
