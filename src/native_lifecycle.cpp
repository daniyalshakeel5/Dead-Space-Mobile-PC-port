#include "ds_port/native_lifecycle.hpp"
namespace ds_port {
NativeLifecycleResult NativeLifecycle::fail(const char* reason) {
    error_=reason;
    phase_=NativeLifecyclePhase::Failed;
    return NativeLifecycleResult::CallbackFailed;
}
NativeLifecycleResult NativeLifecycle::start(const NativeLifecycleCallbacks& c) {
    if(phase_!=NativeLifecyclePhase::Idle)return NativeLifecycleResult::InvalidPhase;
    cb_=c;
    if(!c.jni_load || !c.audio_start || !c.activity_create ||
       !c.surface_create || !c.keyboard_visibility || !c.graphics_init ||
       !c.input_init || !c.input_poll || !c.draw_frame || !c.present_frame) {
        phase_=NativeLifecyclePhase::Failed;
        error_="required native engine lifecycle callback missing";
        return NativeLifecycleResult::MissingCallback;
    }
    if(!c.jni_load(c.context))return fail("JNI startup");
    if(!c.audio_start(c.context))return fail("audio startup");
    audio_started_=true;
    if(!c.activity_create(c.context))return fail("activity creation");
    if(!c.surface_create(c.context))return fail("surface creation");
    if(!c.keyboard_visibility(c.context,600,1))
        return fail("keyboard visibility");
    phase_=NativeLifecyclePhase::Running;
    return NativeLifecycleResult::Success;
}
NativeLifecycleResult NativeLifecycle::frame() {
    if(phase_!=NativeLifecyclePhase::Running)
        return NativeLifecycleResult::InvalidPhase;
    // Retain the original loader's first-frame and second-frame ordering.
    if(frames_==1) {
        if(!cb_.graphics_init(cb_.context))return fail("graphics startup");
        graphics_started_=true;
        if(!cb_.input_init(cb_.context))return fail("input startup");
    }
    if(frames_>=2 && !cb_.input_poll(cb_.context))
        return fail("input polling");
    if(!cb_.draw_frame(cb_.context))return fail("draw frame");
    if(frames_>=2 && !cb_.present_frame(cb_.context))
        return fail("frame present");
    ++frames_;
    return NativeLifecycleResult::Success;
}
void NativeLifecycle::stop() noexcept {
    if(phase_==NativeLifecyclePhase::Stopped)return;
    if(graphics_started_ && cb_.graphics_stop)
        cb_.graphics_stop(cb_.context);
    if(audio_started_ && cb_.audio_stop)
        cb_.audio_stop(cb_.context);
    graphics_started_=false;
    audio_started_=false;
    phase_=NativeLifecyclePhase::Stopped;
}
}
