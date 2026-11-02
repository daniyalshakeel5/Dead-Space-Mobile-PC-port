#pragma once
#include <cstdint>
namespace ds_port {
struct NativeMenuInputCallbacks {
    void* context=nullptr;
    void (*pointer)(void*,int,int,int,float,float)=nullptr;
    void (*key_down)(void*,int,int,int)=nullptr;
    void (*key_up)(void*,int,int,int)=nullptr;
};
struct NativeMenuInputState {
    bool mouse_held=false;
    bool enter_held=false;
    bool escape_held=false;
    float last_u=0.f,last_v=0.f;
};
// Dispatches desktop events using the ORIGINAL JNI callback signatures.
// Android event constants: ACTION_DOWN=0, ACTION_UP=1, ACTION_MOVE=2.
// A real original game dispatch binding is required for menu interaction.
bool native_menu_pointer(NativeMenuInputState&,const NativeMenuInputCallbacks&,
    int event,int x,int y,int width,int height) noexcept;
bool native_menu_key(NativeMenuInputState&,const NativeMenuInputCallbacks&,
    int android_key,bool down) noexcept;
void native_menu_focus_lost(NativeMenuInputState&,
    const NativeMenuInputCallbacks&) noexcept;
}
