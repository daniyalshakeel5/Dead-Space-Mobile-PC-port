#include "ds_port/native_menu_input.hpp"
#include <algorithm>
namespace ds_port {
namespace {
constexpr int kMenuModule=600;
constexpr int kPointer=0;
}
bool native_menu_pointer(NativeMenuInputState& state,
    const NativeMenuInputCallbacks& cb,int event,int x,int y,
    int width,int height) noexcept {
    if(!cb.pointer||width<=0||height<=0||event<0||event>2)return false;
    if(event==0 && state.mouse_held)return false;
    if(event==1 && !state.mouse_held)return false;
    state.mouse_held=(event!=1);
    const float u=std::clamp(float(x)/float(width),0.f,1.f);
    const float v=std::clamp(float(y)/float(height),0.f,1.f);
    state.last_u=u;state.last_v=v;
    cb.pointer(cb.context,event,kMenuModule,kPointer,u,v);
    return true;
}
bool native_menu_key(NativeMenuInputState& state,
    const NativeMenuInputCallbacks& cb,int key,bool down) noexcept {
    if(key!=66&&key!=4)return false; // Android ENTER and BACK
    bool& held=key==66?state.enter_held:state.escape_held;
    if(held==down)return false;
    if(down && !cb.key_down)return false;
    if(!down && !cb.key_up)return false;
    held=down;
    if(down)cb.key_down(cb.context,kMenuModule,key,0);
    else cb.key_up(cb.context,kMenuModule,key,0);
    return true;
}
void native_menu_focus_lost(NativeMenuInputState& state,
    const NativeMenuInputCallbacks& cb) noexcept {
    if(state.mouse_held && cb.pointer) {
        state.mouse_held=false;
        cb.pointer(cb.context,1,kMenuModule,kPointer,
                   state.last_x>=0?0.f:0.f,0.f);
    }
    if(state.enter_held && cb.key_up)
        cb.key_up(cb.context,kMenuModule,66,0);
    if(state.escape_held && cb.key_up)
        cb.key_up(cb.context,kMenuModule,4,0);
    state.enter_held=false;state.escape_held=false;
}
}
