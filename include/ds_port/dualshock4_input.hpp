#pragma once
#include <cstdint>
namespace ds_port {
// All public functions here are host-side input conversions. Only the
// actual ARM guest JNI input bridge can decide whether an event is consumed.
// The virtual joystick positions come from MIT-licensed DeadSpace Vita
// loader/reimpl/controls.c, Xperia Play v1.1.33, not from our game's APK.
struct Ds4WindowPointer {int x=0,y=0;};
bool ds4_touchpad_to_window(
    int window_width,int window_height,float normalized_x,float normalized_y,
    Ds4WindowPointer& output) noexcept;

float ds4_normalize_axis(std::int16_t sample,float deadzone,
                         float max_value=1.f) noexcept;

struct Ds4GuestPointerEvent {
    int raw_event=0, module=0, finger=0;
    float x=0.f,y=0.f;
};
struct Ds4GuestPointerSink {
    void* context=nullptr;
    void (*emit)(void*,const Ds4GuestPointerEvent&)=nullptr;
};
struct Ds4GuestStickState {
    bool left_held=false, right_held=false;
    float left_last_x=192.f,left_last_y=75.f;
    float right_last_x=786.f,right_last_y=180.f;
};
// Reconstruct Xperia Play/Vita touch protocol and configurable source
// logical coordinates (original source reference canvas is 960x544).
// Caller must convert/verify guest JNI ABI for the exact game library;
// no original guest events are sent until an ARM runtime binds its queue.
void ds4_emit_guest_sticks(
    Ds4GuestStickState& state,
    std::int16_t left_x,std::int16_t left_y,
    std::int16_t right_x,std::int16_t right_y,
    Ds4GuestPointerSink sink,
    float left_deadzone=0.18f,float right_deadzone=0.16f) noexcept;
void ds4_release_guest_sticks(
    Ds4GuestStickState& state,Ds4GuestPointerSink sink) noexcept;
enum class Ds4Button : std::uint8_t {
    Cross,Circle,Square,Triangle,L1,R1,DpadUp,DpadDown,DpadLeft,
    DpadRight,Options,Share,Unknown
};
int ds4_vita_android_key(Ds4Button button) noexcept;
} // namespace ds_port
