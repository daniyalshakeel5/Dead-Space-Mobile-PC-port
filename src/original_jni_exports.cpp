#include "ds_port/original_jni_exports.hpp"

// Reconstructed exact native behavior, one function per original ARM export.
// Each original function was proven to perform an unconditional constant
// integer return from machine instructions on the user-confirmed APK, not
// inferred from an identifier or inferred gameplay behavior. No ARM runtime.
extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationBottomSideUp(void*, void*) noexcept {
    return 4;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationFrontSideUp(void*, void*) noexcept {
    return 16;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationLeftSideUp(void*, void*) noexcept {
    return 2;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationRightSideUp(void*, void*) noexcept {
    return 8;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationTopSideUp(void*, void*) noexcept {
    return 1;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationUnknown(void*, void*) noexcept {
    return 0;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_DisplayAndroidDelegate_NativeGetOrientationNormal(void*, void*) noexcept {
    return 1;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_DisplayAndroidDelegate_NativeGetOrientationRotatedLeft(void*, void*) noexcept {
    return 8;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_DisplayAndroidDelegate_NativeGetOrientationRotatedRight(void*, void*) noexcept {
    return 2;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_DisplayAndroidDelegate_NativeGetOrientationUnknown(void*, void*) noexcept {
    return 0;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_DisplayAndroidDelegate_NativeGetOrientationUpsideDown(void*, void*) noexcept {
    return 4;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdAccelerometer(void*, void*) noexcept {
    return 100;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdDisplay(void*, void*) noexcept {
    return 400;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdPhysicalKeyboard(void*, void*) noexcept {
    return 600;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdTouchScreen(void*, void*) noexcept {
    return 1000;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdVirtualKeyboard(void*, void*) noexcept {
    return 700;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerCancel(void*, void*) noexcept {
    return 12;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdUndefined(void*, void*) noexcept {
    return 0;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelDefault(void*, void*) noexcept {
    return 0;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelDone(void*, void*) noexcept {
    return 5;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelGo(void*, void*) noexcept {
    return 1;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelNext(void*, void*) noexcept {
    return 2;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelSearch(void*, void*) noexcept {
    return 3;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelSend(void*, void*) noexcept {
    return 4;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetLayoutDefault(void*, void*) noexcept {
    return 0;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetLayoutDigits(void*, void*) noexcept {
    return 1;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetLayoutEmail(void*, void*) noexcept {
    return 2;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetLayoutPhone(void*, void*) noexcept {
    return 3;
}

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetLayoutUrl(void*, void*) noexcept {
    return 4;
}

