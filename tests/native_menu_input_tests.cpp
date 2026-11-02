#include "ds_port/native_menu_input.hpp"
#include "ds_port/original_input_events.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <vector>
struct Event {int type,code,module;float x,y;};
struct Trace {std::vector<Event> events;};
int main(){
    using namespace ds_port;
    Trace trace;
    NativeMenuInputCallbacks cb{};
    cb.context=&trace;
    cb.pointer=[](void* p,int action,int module,int,float x,float y){
        static_cast<Trace*>(p)->events.push_back({1,action,module,x,y});
    };
    cb.key_down=[](void* p,int module,int key,int){
        static_cast<Trace*>(p)->events.push_back({2,key,module,0,0});
    };
    cb.key_up=[](void* p,int module,int key,int){
        static_cast<Trace*>(p)->events.push_back({3,key,module,0,0});
    };
    NativeMenuInputState state{};
    assert(!native_menu_pointer(state,cb,2,320,240,640,480));
    assert(!state.mouse_held&&trace.events.empty());
    assert(native_menu_pointer(state,cb,0,320,240,640,480));
    assert(!native_menu_pointer(state,cb,0,320,240,640,480));
    assert(native_menu_pointer(state,cb,2,640,480,640,480));
    assert(trace.events.back().x==1.f&&trace.events.back().y==1.f);
    assert(native_menu_pointer(state,cb,1,640,480,640,480));
    assert(!native_menu_pointer(state,cb,1,640,480,640,480));
    assert(!native_menu_pointer(state,cb,2,640,480,640,480));
    assert(!state.mouse_held&&trace.events.size()==3);
    assert(native_menu_key(state,cb,66,true));
    assert(!native_menu_key(state,cb,66,true));
    assert(native_menu_key(state,cb,4,true));
    native_menu_focus_lost(state,cb);
    assert(trace.events.size()==7);
    assert(trace.events[0].module==600);
    assert(trace.events[0].code==int(ds_port::original_runtime::kOriginalRawPointerDown));
    assert(trace.events[1].code==int(ds_port::original_runtime::kOriginalRawPointerMove));
    assert(trace.events[2].code==int(ds_port::original_runtime::kOriginalRawPointerUp));
    assert(trace.events[5].type==3&&trace.events[5].code==66);
    assert(trace.events[6].type==3&&trace.events[6].code==4);
    return 0;
}
