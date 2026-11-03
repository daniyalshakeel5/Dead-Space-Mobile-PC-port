#pragma once
#include "guest_elf32.hpp"
#include "guest_xperia_input.hpp"
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
namespace xperia {
// Windows release backend: SDL2's real OpenGL context, PlayStation controller
// support and PCM audio. ARM guest calls are validated and forwarded to real
// host APIs; unimplemented GLES1 calls remain unresolved named fault traps.
class GuestSdlHost {
public:
    GuestSdlHost();
    ~GuestSdlHost();
    bool start(std::string& error,int width=960,int height=544);
    void stop() noexcept;
    void pump(GuestXperiaInput& input,bool& quit) noexcept;
    void swap() noexcept;
    bool queue_pcm16(const std::uint8_t* data,std::size_t bytes,
                     std::string& error) noexcept;
    bool audio_ready() const noexcept {return audio_device_!=0;}
    bool audio_control(unsigned action,std::string& error) noexcept;
    bool graphics_ready() const noexcept {return context_!=nullptr;}
    const std::unordered_map<std::string,std::uint32_t>& symbols() const noexcept {
        return symbols_;
    }
    bool code_byte(std::uint32_t address,std::uint8_t& out) const noexcept;
    bool handles(std::uint32_t id) const noexcept;
    std::uint32_t return_pc(std::uint32_t id) const noexcept;
    bool dispatch(std::uint32_t id,std::array<std::uint32_t,16>& regs,
                  GuestElf32& guest,std::string& error) noexcept;
    static constexpr std::uint32_t veneer_base=0x74300000u;
    static constexpr std::uint32_t svc_base=0x3000u;
private:
    SDL_Window* window_=nullptr;
    SDL_GLContext context_=nullptr;
    SDL_AudioDeviceID audio_device_=0;
    SDL_GameController* controller_=nullptr;
    SDL_JoystickID controller_id_=-1;
    std::array<std::int16_t,4> axes_{};
    std::uint64_t last_tick_ms_=0;
    float cursor_x_=480.f,cursor_y_=272.f;
    int cursor_dx_=0,cursor_dy_=0;
    bool cursor_down_=false,mouse_down_=false;
    std::unordered_map<std::string,std::uint32_t> symbols_;
    std::vector<std::uint8_t> veneers_;
    struct ArrayDesc {
        int size=0,type=0,stride=0;
        std::uint32_t ptr=0;
        bool enabled=false;
        std::uint32_t buffer=0;
    } vertex_,color_,normal_;
    static constexpr std::size_t kMaxTextureUnits=8;
    std::array<ArrayDesc,kMaxTextureUnits> texcoord_units_{};
    unsigned client_texunit_=0;
    std::uint32_t array_buffer_=0,element_buffer_=0;
    std::unordered_map<std::uint32_t,std::size_t> buffer_sizes_;
    bool apply_arrays(GuestElf32& guest,std::uint32_t highest,
                      std::string& error) noexcept;
    void button(GuestXperiaInput& input,int button,bool pressed) noexcept;
    void controller_open(int device_index) noexcept;
    static std::uint32_t get_arg(const std::array<std::uint32_t,16>& regs,
                                 GuestElf32& guest,unsigned index);
};
}
