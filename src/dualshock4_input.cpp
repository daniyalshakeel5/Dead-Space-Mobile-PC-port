#include "ds_port/dualshock4_input.hpp"
#include "ds_port/original_ui_interaction.hpp"
#include <algorithm>
#include <cmath>
namespace ds_port {
namespace {
constexpr int kDown=0x6000c,kMove=0x4000c,kUp=0x8000c;
constexpr int kScreen=1000,kTouchpad=1100;
void send(Ds4GuestPointerSink sink,int ev,int mod,int finger,float x,float y) noexcept {
    if(sink.emit)sink.emit(sink.context,Ds4GuestPointerEvent{ev,mod,finger,x,y});
}
}
bool ds4_touchpad_to_window(int w,int h,float x,float y,
                             Ds4WindowPointer& out) noexcept {
    if(!std::isfinite(x)||!std::isfinite(y))return false;
    const auto vp=original_ui_viewport(w,h);
    if(vp.width<=0||vp.height<=0)return false;
    x=std::clamp(x,0.f,1.f);y=std::clamp(y,0.f,1.f);
    out.x=vp.x+static_cast<int>(x*float(vp.width-1));
    out.y=vp.y+static_cast<int>(y*float(vp.height-1));
    return true;
}
float ds4_normalize_axis(std::int16_t sample,float deadzone,float max_value) noexcept {
    if(!std::isfinite(deadzone)||!std::isfinite(max_value)||
       deadzone<0.f||deadzone>=1.f||max_value<=0.f||max_value>1.f)
        return 0.f;
    const float value=sample<0?float(sample)/32768.f:float(sample)/32767.f;
    const float magnitude=std::fabs(value);
    if(magnitude<=deadzone)return 0.f;
    return std::copysign(
        std::min(1.f,(magnitude-deadzone)/(1.f-deadzone))*max_value,value);
}
void ds4_release_guest_sticks(
    Ds4GuestStickState& st,Ds4GuestPointerSink sink) noexcept {
    if(st.right_held)
        send(sink,kUp,kTouchpad,2,st.right_last_x,st.right_last_y);
    if(st.left_held)
        send(sink,kUp,kScreen,1,st.left_last_x,st.left_last_y);
    st.left_held=false;st.right_held=false;
}
void ds4_emit_guest_sticks(
    Ds4GuestStickState& st,
    std::int16_t l_x,std::int16_t l_y,std::int16_t r_x,std::int16_t r_y,
    Ds4GuestPointerSink sink,float left_dz,float right_dz) noexcept {
    // DeadSpace Vita loader/reimpl/controls.c maps a 960x544 source
    // touchscreen, not PC window pixels. It uses separate input modules
    // for movement and camera and leaves camera touch transient.
    const float lx=ds4_normalize_axis(l_x,left_dz,0.76f);
    const float ly=ds4_normalize_axis(l_y,left_dz,0.76f);
    const float rx=ds4_normalize_axis(r_x,right_dz,1.f);
    const float ry=ds4_normalize_axis(r_y,right_dz,1.f);
    const bool left=lx!=0.f||ly!=0.f;
    const bool right=rx!=0.f||ry!=0.f;
    // The original Vita bridge releases its previous camera gesture
    // before starting the next directional swipe.
    if(st.right_held)send(sink,kUp,kTouchpad,2,st.right_last_x,st.right_last_y);
    st.right_held=false;
    if(right){
        const float x=786.f+155.f*rx,y=180.f-105.f*ry;
        send(sink,kDown,kTouchpad,2,786.f,180.f);
        send(sink,kMove,kTouchpad,2,x,y);
        st.right_last_x=x;st.right_last_y=y;st.right_held=true;
    }
    if(!left && st.left_held){
        send(sink,kUp,kScreen,1,st.left_last_x,st.left_last_y);
        st.left_held=false;
    }
    if(left) {
        const float x=192.f+75.f*lx,y=75.f+75.f*ly;
        if(!st.left_held)send(sink,kDown,kScreen,1,192.f,75.f);
        send(sink,kMove,kScreen,1,x,y);
        st.left_last_x=x;st.left_last_y=y;st.left_held=true;
    }
}
int ds4_vita_android_key(Ds4Button b) noexcept {
    // Exact platform key-code mapping from MIT Vita loader controls.c.
    // The Google Play/Fixes ELF exposes the same JNI entrypoint names,
    // but matching controller behavior in that revision remains untested.
    switch(b){
        case Ds4Button::Cross:return 23; // DPAD_CENTER / interact
        case Ds4Button::Circle:return 4; // BACK / kinesis in gameplay
        case Ds4Button::Square:return 99;
        case Ds4Button::Triangle:return 100;
        case Ds4Button::L1:return 102;
        case Ds4Button::R1:return 103;
        case Ds4Button::DpadUp:return 19;
        case Ds4Button::DpadDown:return 20;
        case Ds4Button::DpadLeft:return 21;
        case Ds4Button::DpadRight:return 22;
        case Ds4Button::Options:return 108;
        case Ds4Button::Share:return 109;
        default:return -1;
    }
}
} // namespace ds_port
