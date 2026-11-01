#include "ds_port/original_native_abi.hpp"

namespace ds_port::original_abi {
// These are independent C++ equivalents of the original game's PROVEN
// two-instruction JNI constant-return routines. The actual game logic is
// not yet reconstructed and is intentionally not stubbed here.
std::uint32_t original_display_value(DisplayOrientation value) noexcept {
    return static_cast<std::uint32_t>(value);
}
std::uint32_t original_device_value(DeviceOrientation value) noexcept {
    return static_cast<std::uint32_t>(value);
}
std::uint32_t original_module_type_value(NativeModuleType value) noexcept {
    return static_cast<std::uint32_t>(value);
}
std::uint32_t original_touch_value(NativeTouchEvent value) noexcept {
    return static_cast<std::uint32_t>(value);
}
std::uint32_t original_enter_label_value(VirtualKeyboardEnter value) noexcept {
    return static_cast<std::uint32_t>(value);
}
std::uint32_t original_layout_value(VirtualKeyboardLayout value) noexcept {
    return static_cast<std::uint32_t>(value);
}
}  // namespace ds_port::original_abi
