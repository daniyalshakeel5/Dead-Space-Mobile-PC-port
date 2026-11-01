#pragma once
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {

// Proven original ARM NativeOnDrawFrame branch at ELF 0x0027ef68.
// Only stage 0 and the early stage selectors are reconstructed. An active
// game frame (stage 1, original call 0x0027fc68) is NOT yet implemented.
enum class FramePrefixResult : std::uint8_t {
    InvalidContext,
    MissingOriginalInitRoutine,
    OriginalInitCompleted,
    OriginalActiveFrameNotYetReconstructed,
    OriginalOtherStateReturned
};

// Original ARM stage 0 passes the address of the state object + 8 bytes in r0
// to internal native routine 0x0027f870, then stores 1 to its first word.
// Caller must supply a reconstructed implementation of that routine; no
// placeholder game simulation is introduced here.
using OriginalFrameInit = void (*)(void* state_plus_eight) noexcept;

FramePrefixResult execute_original_frame_prefix(
    void* original_state_object, std::size_t accessible_bytes,
    OriginalFrameInit original_initializer) noexcept;

} // namespace ds_port::original_runtime
