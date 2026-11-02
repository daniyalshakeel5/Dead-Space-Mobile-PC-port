#include "ds_port/original_input_jni_bridge.hpp"
#include "ds_port/original_runtime_globals.hpp"
#include <mutex>
namespace ds_port::original_runtime {
namespace {
std::mutex runtime_mutex;
OriginalInputBinding bound{};
bool is_bound=false;
thread_local OriginalInputStatus last_status=OriginalInputStatus::MissingOriginalService;
}
bool original_input_ready_0x27e97c(
    const void* original_frame_state,std::size_t bytes) noexcept {
    // Real ARM at 0x27E97C: LDR r0,[PC,r3] of original 0x0049CAA8;
    // CMP r0,#1; MOVNE r0,#0; MOVEQ r0,#1; BX lr.
    if(!original_frame_state || bytes<4)return false;
    const auto* p=static_cast<const std::uint8_t*>(original_frame_state);
    const std::uint32_t original_stage=
        std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
        (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
    return original_stage==1u;
}
void original_bind_input_services(const OriginalInputBinding& input) noexcept {
    std::lock_guard<std::mutex> lock(runtime_mutex);
    bound=input;is_bound=true;
}
bool original_bind_input_mapped_runtime(
    const OriginalInputMappedOwners& mapped,
    const void* frame_state,std::size_t frame_bytes,
    OriginalInputAllocate allocator,OriginalInputQueue queue,
    void* ctx) noexcept {
    if(!frame_state || frame_bytes<4 || !allocator || !queue)return false;
    OriginalInputServices services{};
    if(!original_resolve_input_virtual_owners(
        mapped,allocator,queue,ctx,&services))
        return false;
    if(!services.pool_owner_arm_va || !services.allocator_method_arm_va ||
       !services.queue_owner_arm_va || !services.queue_method_arm_va)return false;
    original_bind_input_services({services,frame_state,frame_bytes});
    return true;
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
    if(!local.mapped_original_frame_state ||
       local.mapped_frame_state_bytes<4) {
        last_status=OriginalInputStatus::MissingOriginalService;
        return;
    }
    last_status=original_dispatch_input_event(
        original_input_ready_0x27e97c(
            local.mapped_original_frame_state,
            local.mapped_frame_state_bytes),packet,local.services);
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
