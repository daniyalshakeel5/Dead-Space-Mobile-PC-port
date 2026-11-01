#pragma once
#include "ds_port/original_input_events.hpp"
#include <cstdint>
namespace ds_port::original_runtime {
// The user-confirmed APK exports the exact JNI names below. The native
// PC port binds them to reconstructed C++ record creation/queue dispatch.
// A missing original runtime is an explicit failure, not synthetic input.
using OriginalInputReady=bool (*)(void* context) noexcept;
struct OriginalInputBinding {
    OriginalInputServices services;
    OriginalInputReady readiness;
    void* readiness_context;
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
