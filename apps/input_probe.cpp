// This window is an input diagnostic only; it renders no original game content.
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "ds_port/input.hpp"

#include <cstdint>
#include <sstream>
#include <string>

using namespace ds_port;

static bool map_key(SDL_Scancode scancode, Key& key) {
    switch (scancode) {
        case SDL_SCANCODE_W: key=Key::W; break;
        case SDL_SCANCODE_A: key=Key::A; break;
        case SDL_SCANCODE_S: key=Key::S; break;
        case SDL_SCANCODE_D: key=Key::D; break;
        case SDL_SCANCODE_LSHIFT: key=Key::LeftShift; break;
        case SDL_SCANCODE_RSHIFT: key=Key::RightShift; break;
        case SDL_SCANCODE_E: key=Key::E; break;
        case SDL_SCANCODE_R: key=Key::R; break;
        case SDL_SCANCODE_SPACE: key=Key::Space; break;
        case SDL_SCANCODE_Q: key=Key::Q; break;
        case SDL_SCANCODE_B: key=Key::B; break;
        case SDL_SCANCODE_TAB: key=Key::Tab; break;
        case SDL_SCANCODE_C: key=Key::C; break;
        case SDL_SCANCODE_F: key=Key::F; break;
        case SDL_SCANCODE_ESCAPE: key=Key::Escape; break;
        case SDL_SCANCODE_1: key=Key::Num1; break;
        case SDL_SCANCODE_2: key=Key::Num2; break;
        case SDL_SCANCODE_3: key=Key::Num3; break;
        case SDL_SCANCODE_4: key=Key::Num4; break;
        default: return false;
    }
    return true;
}

static bool map_mouse(std::uint8_t value, Mouse& button) {
    switch (value) {
        case SDL_BUTTON_LEFT: button=Mouse::Left; return true;
        case SDL_BUTTON_RIGHT: button=Mouse::Right; return true;
        case SDL_BUTTON_MIDDLE: button=Mouse::Middle; return true;
        default: return false;
    }
}

int main(int, char**) {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) return 1;
    SDL_Window* window = SDL_CreateWindow(
        "Dead Space Mobile: input diagnostics only (F1 capture / F10 quit)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 960, 540,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Quit();
        return 2;
    }

    Input input;
    bool running = true;
    bool capture = false;
    while (running) {
        input.begin_frame();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT: running = false; break;
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) input.focus_lost();
                    if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) input.focus_gained();
                    break;
                case SDL_KEYDOWN:
                    if (!event.key.repeat && event.key.keysym.scancode == SDL_SCANCODE_F10)
                        running = false;
                    else if (!event.key.repeat && event.key.keysym.scancode == SDL_SCANCODE_F1) {
                        capture = !capture;
                        SDL_SetRelativeMouseMode(capture ? SDL_TRUE : SDL_FALSE);
                    } else {
                        Key key;
                        if (map_key(event.key.keysym.scancode, key))
                            input.key(key, true);
                    }
                    break;
                case SDL_KEYUP: {
                    Key key;
                    if (map_key(event.key.keysym.scancode, key)) input.key(key, false);
                    break;
                }
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP: {
                    Mouse button;
                    if (map_mouse(event.button.button, button))
                        input.mouse_button(button, event.type == SDL_MOUSEBUTTONDOWN);
                    break;
                }
                case SDL_MOUSEMOTION:
                    input.mouse_motion(event.motion.xrel, event.motion.yrel);
                    break;
                case SDL_MOUSEWHEEL:
                    input.wheel(event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y);
                    break;
                default: break;
            }
        }

        const Frame& frame = input.current();
        std::ostringstream caption;
        caption << "Input probe | WASD " << frame.active(Action::Forward)
                << frame.active(Action::Left) << frame.active(Action::Backward)
                << frame.active(Action::Right)
                << " | aim " << frame.active(Action::Aim)
                << " fire " << frame.active(Action::Fire)
                << " melee " << frame.active(Action::Melee)
                << " | scroll " << frame.weapon_scroll
                << " | slot " << frame.weapon_slot
                << " | F1 capture / F10 quit | NO GAME RUNTIME";
        const std::string title = caption.str();
        SDL_SetWindowTitle(window, title.c_str());
        SDL_Delay(16);
    }

    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
