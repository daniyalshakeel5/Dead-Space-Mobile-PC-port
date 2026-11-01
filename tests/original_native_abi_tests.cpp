#include "ds_port/original_native_abi.hpp"
#include <array>
#include <cstdint>
#include <iostream>
using namespace ds_port::original_abi;

#define CHECK(expr) do { if (!(expr)) { \
    std::cerr << "Original ARM ABI mismatch at " << __LINE__ << ": " << #expr << '\n'; \
    return 1; } } while (0)

int main() {
    // Literal independent expected values, from actual code disassembly.
    CHECK(original_display_value(DisplayOrientation::Unknown)==0);
    CHECK(original_display_value(DisplayOrientation::Normal)==1);
    CHECK(original_display_value(DisplayOrientation::RotatedRight)==2);
    CHECK(original_display_value(DisplayOrientation::UpsideDown)==4);
    CHECK(original_display_value(DisplayOrientation::RotatedLeft)==8);
    CHECK(original_device_value(DeviceOrientation::Unknown)==0);
    CHECK(original_device_value(DeviceOrientation::TopSideUp)==1);
    CHECK(original_device_value(DeviceOrientation::LeftSideUp)==2);
    CHECK(original_device_value(DeviceOrientation::BottomSideUp)==4);
    CHECK(original_device_value(DeviceOrientation::RightSideUp)==8);
    CHECK(original_device_value(DeviceOrientation::FrontSideUp)==16);
    CHECK(original_module_type_value(NativeModuleType::Accelerometer)==100);
    CHECK(original_module_type_value(NativeModuleType::Display)==400);
    CHECK(original_module_type_value(NativeModuleType::PhysicalKeyboard)==600);
    CHECK(original_module_type_value(NativeModuleType::VirtualKeyboard)==700);
    CHECK(original_module_type_value(NativeModuleType::TouchScreen)==1000);
    CHECK(original_touch_value(NativeTouchEvent::Undefined)==0);
    CHECK(original_touch_value(NativeTouchEvent::Cancel)==12);
    CHECK(original_enter_label_value(VirtualKeyboardEnter::Default)==0);
    CHECK(original_enter_label_value(VirtualKeyboardEnter::Go)==1);
    CHECK(original_enter_label_value(VirtualKeyboardEnter::Next)==2);
    CHECK(original_enter_label_value(VirtualKeyboardEnter::Search)==3);
    CHECK(original_enter_label_value(VirtualKeyboardEnter::Send)==4);
    CHECK(original_enter_label_value(VirtualKeyboardEnter::Done)==5);
    CHECK(original_layout_value(VirtualKeyboardLayout::Default)==0);
    CHECK(original_layout_value(VirtualKeyboardLayout::Digits)==1);
    CHECK(original_layout_value(VirtualKeyboardLayout::Email)==2);
    CHECK(original_layout_value(VirtualKeyboardLayout::Phone)==3);
    CHECK(original_layout_value(VirtualKeyboardLayout::Url)==4);
    std::cout << "All 29 original native JNI ABI values match verified ARM returns\n";
    return 0;
}
