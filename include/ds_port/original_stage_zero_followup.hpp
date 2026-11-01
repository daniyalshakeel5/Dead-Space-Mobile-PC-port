#pragma once
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {
// The complete, five-instruction original ARM function at ELF 0x002833f4:
//   MOV r3,#0; STR r3,[r0,#0x20]; STR r1,[r0,#0x1c];
//   STR r1,[r0,#0x24]; BX LR.
// ARM r0 is an object pointer; r1 is a 32-bit value (possibly an original
// pointer token). Never dereference r1 or convert it into a host pointer.
// Returns false without changing memory when the destination is insufficient.
bool original_followup_store_0x2833f4(
    void* original_object,
    std::size_t accessible_bytes,
    std::uint32_t original_r1_value) noexcept;
} // namespace ds_port::original_runtime
