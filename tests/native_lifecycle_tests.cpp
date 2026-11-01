#include "ds_port/native_lifecycle.hpp"
#include <cassert>
#include <string>
using namespace ds_port;
struct Trace{std::string steps;};
static bool jni(void* p){static_cast<Trace*>(p)->steps+="J";return true;}
static bool audio(void* p){static_cast<Trace*>(p)->steps+="A";return true;}
static bool activity(void* p){static_cast<Trace*>(p)->steps+="C";return true;}
static bool surface(void* p){static_cast<Trace*>(p)->steps+="S";return true;}
static bool keyboard(void* p,int module,int hidden) {
    assert(module==600&&hidden==1);
    static_cast<Trace*>(p)->steps+="K";return true;
}
static bool graphics(void* p){static_cast<Trace*>(p)->steps+="G";return true;}
static bool input(void* p){static_cast<Trace*>(p)->steps+="I";return true;}
static bool poll(void* p){static_cast<Trace*>(p)->steps+="P";return true;}
static bool draw(void* p){static_cast<Trace*>(p)->steps+="D";return true;}
static bool present(void* p){static_cast<Trace*>(p)->steps+="F";return true;}
static void stop_audio(void* p){static_cast<Trace*>(p)->steps+="a";}
static void stop_graphics(void* p){static_cast<Trace*>(p)->steps+="g";}
int main(){
    Trace t;
    NativeLifecycleCallbacks c{};
    c.context=&t;c.jni_load=jni;c.audio_start=audio;
    c.activity_create=activity;c.surface_create=surface;
    c.keyboard_visibility=keyboard;c.graphics_init=graphics;
    c.input_init=input;c.input_poll=poll;c.draw_frame=draw;
    c.present_frame=present;c.audio_stop=stop_audio;c.graphics_stop=stop_graphics;
    NativeLifecycle lifecycle;
    assert(lifecycle.start(c)==NativeLifecycleResult::Success);
    assert(t.steps=="JACSK");
    assert(lifecycle.frame()==NativeLifecycleResult::Success);
    assert(t.steps=="JACSKD");
    assert(lifecycle.frame()==NativeLifecycleResult::Success);
    assert(t.steps=="JACSKDGID");
    assert(lifecycle.frame()==NativeLifecycleResult::Success);
    assert(t.steps=="JACSKDGIDPDF");
    assert(lifecycle.frames()==3);
    lifecycle.stop();
    assert(t.steps=="JACSKDGIDPDFga");
    assert(lifecycle.frame()==NativeLifecycleResult::InvalidPhase);
    lifecycle.stop();
    NativeLifecycle broken;
    c.jni_load=nullptr;
    assert(broken.start(c)==NativeLifecycleResult::MissingCallback);
    return 0;
}
