#pragma once
#include "guest_arm_call_runner.hpp"
#include "ds_port/dualshock4_input.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
namespace xperia {
// This game's original Java layer calls exported Java_* methods, not
// Android NDK AInputQueue. The queue owns guest events until an ARM guest
// frame can consume them; nothing in this bridge calls native x86 code
// in place of the user's original game implementation.
class GuestXperiaInput {
public:
    struct Event {
        bool pointer=false;
        int type=0,module=0,finger=0;
        float x=0,y=0;
        bool down=false;
    };
    bool bind(const GuestElf32& image,std::string& error);
    void button(ds_port::Ds4Button b,bool down);
    void sticks(std::int16_t lx,std::int16_t ly,std::int16_t rx,std::int16_t ry);
    void pointer(int event,int module,int finger,float x,float y);
    void disconnect();
    bool flush(GuestArmCallRunner& runner,std::string& error);
    std::size_t pending() const noexcept {return queue_.size();}
    static constexpr std::size_t max_events=2048;
private:
    std::uint32_t on_down_=0,on_up_=0,on_pointer_=0;
    ds_port::Ds4GuestButtonState buttons_{};
    ds_port::Ds4GuestStickState sticks_{};
    std::deque<Event> queue_;
    bool push(const Event& event);
    static void emit_key(void* ctx,const ds_port::Ds4GuestKeyEvent& event);
    static void emit_pointer(void* ctx,const ds_port::Ds4GuestPointerEvent& event);
};
}
