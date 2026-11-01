#pragma once
#include <cstdint>

// Linkable implementations of 29 instruction-proven JNI return functions from
// the original Dead Space Mobile ARM library. Does not initialize any gameplay,
// implement a JVM, or invent unverified NativeGetRawPointerDown/Move/Up IDs.
// Original functions return their constants unconditionally; JNI's two opaque
// calling convention arguments (JNIEnv*, jobject/jclass) are unused.
#if defined(_WIN32)
  #define DS_ORIGINAL_JNI_EXPORT __declspec(dllexport)
#else
  #define DS_ORIGINAL_JNI_EXPORT __attribute__((visibility("default")))
#endif
extern "C" {
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationBottomSideUp(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationFrontSideUp(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationLeftSideUp(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationRightSideUp(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationTopSideUp(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationUnknown(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_DisplayAndroidDelegate_NativeGetOrientationNormal(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_DisplayAndroidDelegate_NativeGetOrientationRotatedLeft(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_DisplayAndroidDelegate_NativeGetOrientationRotatedRight(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_DisplayAndroidDelegate_NativeGetOrientationUnknown(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_DisplayAndroidDelegate_NativeGetOrientationUpsideDown(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdAccelerometer(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdDisplay(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdPhysicalKeyboard(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdTouchScreen(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdVirtualKeyboard(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerCancel(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdUndefined(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelDefault(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelDone(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelGo(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelNext(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelSearch(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelSend(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetLayoutDefault(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetLayoutDigits(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetLayoutEmail(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetLayoutPhone(void*, void*) noexcept;
DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetLayoutUrl(void*, void*) noexcept;
}
