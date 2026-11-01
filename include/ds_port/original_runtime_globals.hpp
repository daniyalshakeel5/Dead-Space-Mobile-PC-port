#pragma once
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {
// Original ARM ELF virtual addresses established by two independent PC-
// relative literal-pool chains (frame+transition) and the newly recovered
// stage-zero functions. These are NOT host pointers or full object types.
inline constexpr std::uint32_t kOriginalFrameGlobalVA=0x0049caa8u;
inline constexpr std::uint32_t kOriginalRecordGlobalVA=0x0049cb94u;
inline constexpr std::uint32_t kOriginalType2FloatVA=0x0049cd1cu;
inline constexpr std::size_t kOriginalRecordGlobalOffset=0xec;
inline constexpr std::size_t kOriginalType2FloatOffset=0x274;
inline constexpr std::size_t kOriginalKnownGlobalSpan=0x278;

// The caller supplies a real host-memory backing span representing
// consecutive ORIGINAL ARM virtual addresses beginning at 0x0049CAA8.
// Do not allocate fictional global data or fabricate unknown values.
// On invalid input, neither output pointer is modified.
bool original_bind_known_global_fields(
    void* original_global_host_span,
    std::size_t accessible_bytes,
    void** mapped_record_slot_0x0049cb94,
    const void** mapped_type2_float_0x0049cd1c) noexcept;
} // namespace ds_port::original_runtime
