#pragma once
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {
// Entire original ARM type-2 path at ELF 0x0028ca24, via 0x0028cad8
// and common epilogue 0x0028caa8. Original caller passes r1=2.
// The original runtime source of VLDR s15 is ELF VA 0x0049cd1c.
// Caller must supply the CURRENT four native bytes for that address;
// we neither invent a value nor dereference the ARM32 address on x86-64.
// Copy as 32-bit floating-point bit patterns; no arithmetic or rounding.
// Writes raw ARM little-endian type and float fields at +0x10,+0x14.
// False means no memory is modified.
bool original_initialize_type_two_0x28ca24(
    void* original_destination,
    std::size_t destination_bytes,
    const void* runtime_float_field_0x0049cd1c,
    std::size_t source_bytes) noexcept;
} // namespace ds_port::original_runtime
