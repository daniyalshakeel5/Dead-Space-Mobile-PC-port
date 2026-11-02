#include "guest_xperia_input.hpp"
#include <cstring>
#include <cmath>
#include <algorithm>
namespace xperia {
namespace {
std::uint32_t bits(float x) noexcept {
    std::uint32_t result=0;
    static_assert(sizeof(result)==sizeof(x),"ARM32 float must be 32-bit");
    std::memcpy(&result,&x,sizeof(result));return result;
}
constexpr int down=0x6000c,move=0x4000c,up=0x8000c;
}
bool GuestXperiaInput::bind(const GuestElf32& image,std::string& error) {
    const auto lookup=[&](const char* name)->std::uint32_t {
        auto it=image.jni_exports.find(name);
        return it==image.jni_exports.end()?0u:it->second;
    };
    const auto a=lookup("Java_com_ea_blast_KeyboardAndroid_NativeOnKeyDown");
    const auto b=lookup("Java_com_ea_blast_KeyboardAndroid_NativeOnKeyUp");
    const auto c=lookup("Java_com_ea_blast_TouchSurfaceAndroid_NativeOnPointerEvent");
    if(!a||!b||!c){
        error="Original Xperia ELF lacks mandatory keyboard or touch JNI exports";
        return false;
    }
    on_down_=a;on_up_=b;on_pointer_=c;
    error.clear();return true;
}
bool GuestXperiaInput::push(const Event& event) {
    if(queue_.size()>=max_events)return false;
    queue_.push_back(event);return true;
}
void GuestXperiaInput::emit_key(void* ctx,const ds_port::Ds4GuestKeyEvent& e){
    auto& self=*static_cast<GuestXperiaInput*>(ctx);
    (void)self.push({false,e.android_key,600,0,0,0,e.pressed});
}
void GuestXperiaInput::emit_pointer(void* ctx,const ds_port::Ds4GuestPointerEvent& e){
    auto& self=*static_cast<GuestXperiaInput*>(ctx);
    (void)self.push({true,e.raw_event,e.module,e.finger,e.x,e.y,false});
}
void GuestXperiaInput::button(ds_port::Ds4Button b,bool pressed){
    ds_port::ds4_emit_guest_button(buttons_,b,pressed,{this,&emit_key});
}
void GuestXperiaInput::sticks(std::int16_t lx,std::int16_t ly,
                               std::int16_t rx,std::int16_t ry) {
    ds_port::ds4_emit_guest_sticks(sticks_,lx,ly,rx,ry,{this,&emit_pointer});
}
void GuestXperiaInput::pointer(int event,int module,int finger,float x,float y) {
    if((event!=down && event!=move && event!=up) ||
       module!=1000 && module!=1100 || finger<0 || finger>15 ||
       !std::isfinite(x)||!std::isfinite(y))return;
    push({true,event,module,finger,x,y,false});
}
void GuestXperiaInput::disconnect(){
    ds_port::ds4_release_guest_buttons(buttons_,{this,&emit_key});
    ds_port::ds4_release_guest_sticks(sticks_,{this,&emit_pointer});
}
bool GuestXperiaInput::flush(GuestArmCallRunner& runner,std::string& error){
    if(!on_down_||!on_up_||!on_pointer_){
        error="Xperia input events not bound to original ARM JNI exports";
        return false;
    }
    // Process in FIFO order; never silently discard failed event delivery.
    while(!queue_.empty()){
        const auto e=queue_.front();
        std::uint32_t result=0;
        bool success=false;
        if(e.pointer){
            success=runner.invoke_aapcs(on_pointer_,
                {GuestJniVm::env_address,0u,
                 static_cast<std::uint32_t>(e.type),
                 static_cast<std::uint32_t>(e.module),
                 static_cast<std::uint32_t>(e.finger),bits(e.x),bits(e.y)},
                 result,error);
        }else{
            success=runner.invoke_aapcs(e.down?on_down_:on_up_,
                {GuestJniVm::env_address,0u,static_cast<std::uint32_t>(e.module),
                 static_cast<std::uint32_t>(e.type),1u},result,error);
        }
        if(!success)return false;
        queue_.pop_front();
    }
    error.clear();return true;
}
}
