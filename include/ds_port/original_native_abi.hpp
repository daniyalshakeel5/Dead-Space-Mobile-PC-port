#pragma once
#include <cstdint>

namespace ds_port::original_abi {

// ORIGINAL GAME NATIVE ABI, NOT INFERRED DESKTOP CONTROLS.
// Numeric values come from unconditional ARM/Thumb JNI return functions
// in original RAFALAMAO-release native library, independently verified by
// GitHub Actions run 41294452264, 2026-11-01.
//
// No values are fabricated for functions whose code did not pass the
// instruction-level proof gate, including RawPointerDown/Move/Up.

enum class DisplayOrientation : std::uint32_t {
    Unknown = 0,
    Normal = 1,
    RotatedRight = 2,
    UpsideDown = 4,
    RotatedLeft = 8
};

enum class DeviceOrientation : std::uint32_t {
    Unknown = 0,
    TopSideUp = 1,
    LeftSideUp = 2,
    BottomSideUp = 4,
    RightSideUp = 8,
    FrontSideUp = 16
};

enum class NativeModuleType : std::uint32_t {
    Accelerometer = 100,
    Display = 400,
    PhysicalKeyboard = 600,
    VirtualKeyboard = 700,
    TouchScreen = 1000
};

// Only these two original touch IDs have been proved so far.
// The original ARM wrappers for Down/Move/Up do not match the restricted
// constant-return pattern and must be reconstructed separately.
enum class NativeTouchEvent : std::uint32_t {
    Undefined = 0,
    Cancel = 12
};

enum class VirtualKeyboardEnter : std::uint32_t {
    Default = 0,
    Go = 1,
    Next = 2,
    Search = 3,
    Send = 4,
    Done = 5
};

enum class VirtualKeyboardLayout : std::uint32_t {
    Default = 0,
    Digits = 1,
    Email = 2,
    Phone = 3,
    Url = 4
};

// Windows/Linux code may pass these original protocol values to a later
// verified reimplementation of native callbacks. These functions do not
// emulate Android, change the game's state, or implement its frame loop.
std::uint32_t original_display_value(DisplayOrientation value) noexcept;
std::uint32_t original_device_value(DeviceOrientation value) noexcept;
std::uint32_t original_module_type_value(NativeModuleType value) noexcept;
std::uint32_t original_touch_value(NativeTouchEvent value) noexcept;
std::uint32_t original_enter_label_value(VirtualKeyboardEnter value) noexcept;
std::uint32_t original_layout_value(VirtualKeyboardLayout value) noexcept;
}  // namespace ds_port::original_abi
