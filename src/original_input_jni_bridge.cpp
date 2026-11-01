#include "ds_port/original_input_jni_bridge.hpp"
#include <mutex>
namespace ds_port::original_runtime {
namespace {
std::mutex runtime_mutex;
OriginalInputBinding bound{};
bool is_bound=false;
thread_local OriginalInputStatus last_status=OriginalInputStatus::MissingOriginalService;
}
void original_bind_input_services(const OriginalInputBinding& input) noexcept {
    std::lock_guard<std::mutex> lock(runtime_mutex);
    bound=input;is_bound=true;
}
void original_unbind_input_services() noexcept {
    std::lock_guard<std::mutex> lock(runtime_mutex);
    bound={};is_bound=false;
}
OriginalInputStatus original_last_input_status() noexcept{return last_status;}
void original_emit_input(const OriginalInputPacket& packet) noexcept {
    OriginalInputBinding local{};
    {
        std::lock_guard<std::mutex> lock(runtime_mutex);
        if(!is_bound) {
            last_status=OriginalInputStatus::MissingOriginalService;
            return;
        }
        local=bound;
    }
    if(!local.readiness){
        last_status=OriginalInputStatus::MissingOriginalService;
        return;
    }
    last_status=original_dispatch_input_event(
        local.readiness(local.readiness_context),packet,local.services);
}
} // namespace ds_port::original_runtime

extern "C" DS_ORIGINAL_INPUT_JNI_API void
Java_com_ea_blast_TouchSurfaceAndroid_NativeOnPointerEvent(
    void*,void*,std::int32_t raw_event,std::int32_t module_id,
    std::int32_t pointer_id,float x,float y) noexcept {
    using namespace ds_port::original_runtime;
    original_emit_input({
        OriginalInputPacket::Kind::Pointer,
        static_cast<std::uint32_t>(raw_event),
        static_cast<std::uint32_t>(module_id),
        static_cast<std::uint32_t>(pointer_id),x,y,0,0
    });
}
extern "C" DS_ORIGINAL_INPUT_JNI_API void
Java_com_ea_blast_KeyboardAndroid_NativeOnKeyDown(
    void*,void*,std::int32_t module_id,std::int32_t key,std::int32_t alt) noexcept {
    using namespace ds_port::original_runtime;
    original_emit_input({
        OriginalInputPacket::Kind::KeyDown,0,
        static_cast<std::uint32_t>(module_id),0,0,0,
        static_cast<std::uint32_t>(key),static_cast<std::uint32_t>(alt)
    });
}
extern "C" DS_ORIGINAL_INPUT_JNI_API void
Java_com_ea_blast_KeyboardAndroid_NativeOnKeyUp(
    void*,void*,std::int32_t module_id,std::int32_t key,std::int32_t alt) noexcept {
    using namespace ds_port::original_runtime;
    original_emit_input({
        OriginalInputPacket::Kind::KeyUp,0,
        static_cast<std::uint32_t>(module_id),0,0,0,
        static_cast<std::uint32_t>(key),static_cast<std::uint32_t>(alt)
    });
}
