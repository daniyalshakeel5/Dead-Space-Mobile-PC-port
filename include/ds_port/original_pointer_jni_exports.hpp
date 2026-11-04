#pragma once
#include <cstdint>
// JNI leaf functions from the user's original ARM APK, not Vita ELF.
// Their independent machine-code proof is stored by remote APK research.
#if defined(_WIN32)
#define DS_ORIGINAL_POINTER_EXPORT __declspec(dllexport)
#else
#define DS_ORIGINAL_POINTER_EXPORT __attribute__((visibility("default")))
#endif
extern "C" {
DS_ORIGINAL_POINTER_EXPORT std::int32_t Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerDown(void*,void*) noexcept;
DS_ORIGINAL_POINTER_EXPORT std::int32_t Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerMove(void*,void*) noexcept;
DS_ORIGINAL_POINTER_EXPORT std::int32_t Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerUp(void*,void*) noexcept;
}
