#include "ds_port/original_jni_exports.hpp"
#include <array>
#include <cstdint>
#include <iostream>
// Independent literal expectations taken from the original ARM analysis,
// not inferred from compiled return bodies.
using Getter = std::int32_t(*)(void*,void*) noexcept;
struct Verified {const char* name; Getter get; std::int32_t expected;};
constexpr std::array<Verified, 29> kVerified = {{
    {"Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationBottomSideUp", &Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationBottomSideUp, 4},
    {"Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationFrontSideUp", &Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationFrontSideUp, 16},
    {"Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationLeftSideUp", &Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationLeftSideUp, 2},
    {"Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationRightSideUp", &Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationRightSideUp, 8},
    {"Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationTopSideUp", &Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationTopSideUp, 1},
    {"Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationUnknown", &Java_com_ea_blast_DeviceOrientationHandlerAndroidDelegate_NativeGetDeviceOrientationUnknown, 0},
    {"Java_com_ea_blast_DisplayAndroidDelegate_NativeGetOrientationNormal", &Java_com_ea_blast_DisplayAndroidDelegate_NativeGetOrientationNormal, 1},
    {"Java_com_ea_blast_DisplayAndroidDelegate_NativeGetOrientationRotatedLeft", &Java_com_ea_blast_DisplayAndroidDelegate_NativeGetOrientationRotatedLeft, 8},
    {"Java_com_ea_blast_DisplayAndroidDelegate_NativeGetOrientationRotatedRight", &Java_com_ea_blast_DisplayAndroidDelegate_NativeGetOrientationRotatedRight, 2},
    {"Java_com_ea_blast_DisplayAndroidDelegate_NativeGetOrientationUnknown", &Java_com_ea_blast_DisplayAndroidDelegate_NativeGetOrientationUnknown, 0},
    {"Java_com_ea_blast_DisplayAndroidDelegate_NativeGetOrientationUpsideDown", &Java_com_ea_blast_DisplayAndroidDelegate_NativeGetOrientationUpsideDown, 4},
    {"Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdAccelerometer", &Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdAccelerometer, 100},
    {"Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdDisplay", &Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdDisplay, 400},
    {"Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdPhysicalKeyboard", &Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdPhysicalKeyboard, 600},
    {"Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdTouchScreen", &Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdTouchScreen, 1000},
    {"Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdVirtualKeyboard", &Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdVirtualKeyboard, 700},
    {"Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerCancel", &Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerCancel, 12},
    {"Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdUndefined", &Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdUndefined, 0},
    {"Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelDefault", &Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelDefault, 0},
    {"Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelDone", &Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelDone, 5},
    {"Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelGo", &Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelGo, 1},
    {"Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelNext", &Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelNext, 2},
    {"Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelSearch", &Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelSearch, 3},
    {"Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelSend", &Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetEnterKeyLabelSend, 4},
    {"Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetLayoutDefault", &Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetLayoutDefault, 0},
    {"Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetLayoutDigits", &Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetLayoutDigits, 1},
    {"Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetLayoutEmail", &Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetLayoutEmail, 2},
    {"Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetLayoutPhone", &Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetLayoutPhone, 3},
    {"Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetLayoutUrl", &Java_com_ea_blast_VirtualKeyboardAndroidDelegate_NativeGetLayoutUrl, 4},
}};
int main() {
    for(const auto& row : kVerified) {
        if(!row.get || row.get(nullptr,nullptr) != row.expected) {
            std::cerr << "Original JNI mismatch: " << row.name << "\n";
            return 1;
        }
    }
    std::cout << "All 29 actual original JNI exports return exactly verified ARM values\n";
    return 0;
}
