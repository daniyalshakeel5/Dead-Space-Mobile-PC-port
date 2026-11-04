#pragma once
#include <cstdint>
#include <string>
namespace ds_port {
// Ported orchestration from the platform-neutral call order in
// v-atamanenko/deadspace-vita loader/main.c (MIT; original authors credited
// in docs/UPSTREAM_NATIVE_REUSE.md). Never executes original ARM code.
enum class NativeLifecyclePhase { Idle, Running, Failed, Stopped };
enum class NativeLifecycleResult { Success, MissingCallback, CallbackFailed, InvalidPhase };
struct NativeLifecycleCallbacks {
    void* context=nullptr;
    bool (*jni_load)(void*)=nullptr;
    bool (*audio_start)(void*)=nullptr;
    bool (*activity_create)(void*)=nullptr;
    bool (*surface_create)(void*)=nullptr;
    bool (*keyboard_visibility)(void*,int,int)=nullptr;
    bool (*graphics_init)(void*)=nullptr;
    bool (*input_init)(void*)=nullptr;
    bool (*input_poll)(void*)=nullptr;
    bool (*draw_frame)(void*)=nullptr;
    bool (*present_frame)(void*)=nullptr;
    void (*audio_stop)(void*)=nullptr;
    void (*graphics_stop)(void*)=nullptr;
};
class NativeLifecycle {
public:
    NativeLifecycleResult start(const NativeLifecycleCallbacks& callbacks);
    NativeLifecycleResult frame();
    void stop() noexcept;
    NativeLifecyclePhase phase()const noexcept{return phase_;}
    std::uint64_t frames()const noexcept{return frames_;}
    const std::string& error()const noexcept{return error_;}
private:
    NativeLifecycleCallbacks cb_{};
    NativeLifecyclePhase phase_=NativeLifecyclePhase::Idle;
    std::uint64_t frames_=0;
    bool audio_started_=false;
    bool graphics_started_=false;
    std::string error_;
    NativeLifecycleResult fail(const char* reason);
};
}
