#pragma once
#include "ds_port/original_input_events.hpp"
#include <cstdint>
#include <cstddef>
namespace ds_port::original_runtime {
// The user-confirmed APK exports the exact JNI names below. The native
// PC port binds them to reconstructed C++ record creation/queue dispatch.
// A missing original runtime is an explicit failure, not synthetic input.
// ARM 0x0027E97C reads runtime global word 0x0049CAA8 and compares
// it with 1. APK ELF and frame dispatcher independently prove this address.
// Host supplies a pointer to that *mapped original word*, not a fake stage.
bool original_input_ready_0x27e97c(
    const void* mapped_original_frame_state,std::size_t mapped_bytes) noexcept;
struct OriginalInputBinding {
    OriginalInputServices services;
    const void* mapped_original_frame_state;
    std::size_t mapped_frame_state_bytes;
};
// The integration owner must keep callback contexts valid while bound.
void original_bind_input_services(const OriginalInputBinding&) noexcept;
void original_unbind_input_services() noexcept;
OriginalInputStatus original_last_input_status() noexcept;
} // namespace ds_port::original_runtime

#if defined(_WIN32)
#define DS_ORIGINAL_INPUT_JNI_API __declspec(dllexport)
#else
#define DS_ORIGINAL_INPUT_JNI_API __attribute__((visibility("default")))
#endif
extern "C" {
// JNI's first two original arguments are opaque pointers; these exports
// intentionally neither load an ARM ELF nor require an Android runtime.
DS_ORIGINAL_INPUT_JNI_API void Java_com_ea_blast_TouchSurfaceAndroid_NativeOnPointerEvent(
    void* env,void* object,std::int32_t raw_event,std::int32_t module_id,
    std::int32_t pointer_id,float x,float y) noexcept;
DS_ORIGINAL_INPUT_JNI_API void Java_com_ea_blast_KeyboardAndroid_NativeOnKeyDown(
    void* env,void* object,std::int32_t module_id,
    std::int32_t android_key,std::int32_t alt_pressed) noexcept;
DS_ORIGINAL_INPUT_JNI_API void Java_com_ea_blast_KeyboardAndroid_NativeOnKeyUp(
    void* env,void* object,std::int32_t module_id,
    std::int32_t android_key,std::int32_t alt_pressed) noexcept;
}
