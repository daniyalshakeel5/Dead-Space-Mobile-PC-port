#include "ds_port/original_pointer_jni_exports.hpp"
#include "ds_port/original_input_events.hpp"
// Original ELF 0x0027F958: MOV r0,#0x60000; ADD r0,#0xC; BX lr
extern "C" DS_ORIGINAL_POINTER_EXPORT std::int32_t Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerDown(void*,void*) noexcept {
    return static_cast<std::int32_t>(ds_port::original_runtime::kOriginalRawPointerDown);
}
// Original ELF 0x0027F964: MOV r0,#0x40000; ADD r0,#0xC; BX lr
extern "C" DS_ORIGINAL_POINTER_EXPORT std::int32_t Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerMove(void*,void*) noexcept {
    return static_cast<std::int32_t>(ds_port::original_runtime::kOriginalRawPointerMove);
}
// Original ELF 0x0027F978: MOV r0,#0x80000; ADD r0,#0xC; BX lr
extern "C" DS_ORIGINAL_POINTER_EXPORT std::int32_t Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerUp(void*,void*) noexcept {
    return static_cast<std::int32_t>(ds_port::original_runtime::kOriginalRawPointerUp);
}
