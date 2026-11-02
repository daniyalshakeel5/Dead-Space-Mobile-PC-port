#ifdef NDEBUG
#undef NDEBUG
#endif
#include "ds_port/dualshock4_input.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>
using namespace ds_port;
int main(){
    Ds4WindowPointer point{};
    assert(ds4_touchpad_to_window(1280,720,0.f,0.f,point));
    assert(point.x==100&&point.y==0);
    assert(ds4_touchpad_to_window(1280,720,1.f,1.f,point));
    assert(point.x==1179&&point.y==719);
    assert(ds4_touchpad_to_window(720,1280,0.5f,0.5f,point));
    assert(point.x>=0&&point.x<720&&point.y>=0&&point.y<1280);
    assert(!ds4_touchpad_to_window(0,720,0.f,0.f,point));
    assert(!ds4_touchpad_to_window(1280,720,
        std::numeric_limits<float>::quiet_NaN(),0.f,point));
    assert(ds4_touchpad_to_window(480,320,2.f,-5.f,point));
    assert(point.x==479&&point.y==0);
    assert(ds4_normalize_axis(0,0.18f)==0.f);
    assert(ds4_normalize_axis(1000,0.18f)==0.f);
    assert(std::fabs(ds4_normalize_axis(32767,0.18f,0.76f)-0.76f)<0.0001f);
    assert(std::fabs(ds4_normalize_axis(-32768,0.18f,0.76f)+0.76f)<0.0001f);
    assert(ds4_normalize_axis(100,1.0f)==0.f);
    std::vector<Ds4GuestPointerEvent> received;
    Ds4GuestPointerSink sink{&received,
        [](void* ctx,const Ds4GuestPointerEvent& e) {
            static_cast<std::vector<Ds4GuestPointerEvent>*>(ctx)->push_back(e);
        }};
    Ds4GuestStickState st{};
    ds4_emit_guest_sticks(st,0,0,0,0,sink);
    assert(received.empty());
    ds4_emit_guest_sticks(st,32767,0,32767,0,sink);
    assert(st.left_held&&st.right_held&&received.size()==4);
    assert(received[0].raw_event==0x6000c && received[0].module==1100);
    assert(received[1].raw_event==0x4000c && received[1].finger==2);
    assert(std::fabs(received[1].x-941.f)<0.01f);
    assert(received[2].raw_event==0x6000c && received[2].module==1000);
    assert(received[3].raw_event==0x4000c && received[3].finger==1);
    assert(std::fabs(received[3].x-249.f)<0.01f);
    received.clear();
    ds4_emit_guest_sticks(st,32767,0,32767,0,sink);
    assert(received.size()==4);
    assert(received[0].raw_event==0x8000c && received[0].module==1100);
    assert(received[1].raw_event==0x6000c && received[2].raw_event==0x4000c);
    assert(received[3].raw_event==0x4000c && received[3].module==1000);
    received.clear();
    ds4_emit_guest_sticks(st,0,0,0,0,sink);
    assert(received.size()==2 && received[0].module==1100 &&
           received[1].module==1000);
    assert(!st.right_held&&!st.left_held);
    received.clear();
    ds4_release_guest_sticks(st,sink);
    assert(received.empty());
    assert(ds4_vita_android_key(Ds4Button::Cross)==23);
    assert(ds4_vita_android_key(Ds4Button::Circle)==4);
    assert(ds4_vita_android_key(Ds4Button::Options)==108);
    assert(ds4_vita_android_key(Ds4Button::Unknown)==-1);
    std::vector<Ds4GuestKeyEvent> keys;
    Ds4GuestKeySink key_sink{&keys,
        [](void* ctx,const Ds4GuestKeyEvent& event) {
            static_cast<std::vector<Ds4GuestKeyEvent>*>(ctx)->push_back(event);
        }};
    Ds4GuestButtonState buttons{};
    ds4_emit_guest_button(buttons,Ds4Button::Cross,true,key_sink);
    ds4_emit_guest_button(buttons,Ds4Button::Cross,true,key_sink);
    assert(keys.size()==1 && keys[0].android_key==23 && keys[0].pressed);
    ds4_emit_guest_button(buttons,Ds4Button::Options,true,key_sink);
    assert(keys.size()==2 && keys[1].android_key==108);
    ds4_emit_guest_button(buttons,Ds4Button::Unknown,true,key_sink);
    assert(keys.size()==2);
    ds4_release_guest_buttons(buttons,key_sink);
    assert(keys.size()==4);
    assert(keys[2].android_key==23 && !keys[2].pressed);
    assert(keys[3].android_key==108 && !keys[3].pressed);
    assert(buttons.pressed_mask==0);
    ds4_release_guest_buttons(buttons,key_sink);
    assert(keys.size()==4);
    ds4_emit_guest_button(buttons,Ds4Button::Circle,true,key_sink);
    ds4_emit_guest_button(buttons,Ds4Button::Circle,false,key_sink);
    assert(keys.size()==6 && keys[4].android_key==4 &&
           keys[4].pressed && !keys[5].pressed);
    return 0;
}
