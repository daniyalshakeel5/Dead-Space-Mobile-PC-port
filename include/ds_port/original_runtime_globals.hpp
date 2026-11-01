#pragma once
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {
// Original ARM ELF virtual addresses established by two independent PC-
// relative literal-pool chains (frame+transition) and the newly recovered
// stage-zero functions. These are NOT host pointers or full object types.
// Verified original 0x27e930 lookup uses the runtime singleton pointer
// at 0x0049CAA0 (8 bytes BEFORE the frame-state global). This slot is
// BSS/runtime-initialized, not a file-backed pointer in the APK.
inline constexpr std::uint32_t kOriginalRegistrySlotVA=0x0049caa0u;
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
inline constexpr std::size_t kOriginalRegistryToFrameOffset=0x08;
inline constexpr std::size_t kOriginalRegistryToRecordOffset=0xf4;
inline constexpr std::size_t kOriginalRegistryToType2FloatOffset=0x27c;
inline constexpr std::size_t kOriginalRegistryAndGlobalsSpan=0x280;

// Maps the original BSS singleton slot, frame state, record pointer and
// type-2 float using one contiguous host backing span starting at the
// original ARM registry slot VA 0x0049CAA0. Never guesses the registry
// pointer's live value or attempts to call the ARM code on the host.
// Invalid bounds leave all output arguments unmodified.
bool original_bind_registry_and_runtime_globals(
    void* original_registry_based_host_span,
    std::size_t accessible_bytes,
    void** registry_slot_0x0049caa0,
    void** frame_state_0x0049caa8,
    void** record_slot_0x0049cb94,
    const void** type2_float_0x0049cd1c) noexcept;
} // namespace ds_port::original_runtime
