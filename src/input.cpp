#include "ds_port/input.hpp"

namespace ds_port {

void Input::begin_frame() {
    frame_.pressed.fill(false);
    frame_.released.fill(false);
    frame_.mouse_dx = 0;
    frame_.mouse_dy = 0;
    frame_.weapon_scroll = 0;
    frame_.weapon_slot = 0;
}

void Input::recompute() {
    std::array<bool, action_count> next{};
    const auto set = [&](Action action, bool active) { next[to_index(action)] = active; };
    const bool aiming = held(Mouse::Right);
    const bool left = held(Mouse::Left);
    const bool space = held(Key::Space);

    set(Action::Forward, held(Key::W));
    set(Action::Backward, held(Key::S));
    set(Action::Left, held(Key::A));
    set(Action::Right, held(Key::D));
    set(Action::Sprint, held(Key::LeftShift) || held(Key::RightShift));
    set(Action::Aim, aiming);
    // 2008 PC-style contextual left click, for input testing only.
    set(Action::Fire, aiming && left);
    set(Action::Melee, !aiming && left);
    set(Action::Stomp, !aiming && space);
    set(Action::AltFire, aiming && space);
    set(Action::Reload, held(Key::R));
    set(Action::Interact, held(Key::E));
    set(Action::Heal, held(Key::Q));
    set(Action::Locator, held(Key::B));
    set(Action::Inventory, held(Key::Tab));
    set(Action::Stasis, held(Key::C));
    set(Action::Kinesis, held(Key::F));
    set(Action::Pause, held(Key::Escape));
    // Vita research confirms original game expects motion gestures for
    // Zero-G jumping and weapon rotation. Desktop uses explicit key actions:
    // G = Zero-G jump, V = rotate/alternate weapon orientation.
    // These are host input intentions, not an emulated accelerometer.
    set(Action::ZeroGJump, held(Key::G));
    set(Action::RotateWeapon, held(Key::V));

    for (std::size_t i = 0; i != action_count; ++i) {
        frame_.pressed[i] = frame_.pressed[i] || (next[i] && !frame_.down[i]);
        frame_.released[i] = frame_.released[i] || (!next[i] && frame_.down[i]);
        frame_.down[i] = next[i];
    }
}

void Input::key(Key key, bool down) {
    if (!focused_ || key == Key::Count) return;
    const auto i = to_index(key);
    if (keys_[i] == down) return;  // Ignore key repeats
    keys_[i] = down;
    if (down && key >= Key::Num1 && key <= Key::Num4)
        frame_.weapon_slot = static_cast<int>(i - to_index(Key::Num1) + 1);
    recompute();
}

void Input::mouse_button(Mouse button, bool down) {
    if (!focused_ || button == Mouse::Count) return;
    const auto i = to_index(button);
    if (mouse_[i] == down) return;
    mouse_[i] = down;
    recompute();
}

void Input::mouse_motion(int dx, int dy) {
    if (!focused_) return;
    frame_.mouse_dx += dx;
    frame_.mouse_dy += dy;
}

void Input::wheel(int steps) {
    if (focused_) frame_.weapon_scroll += steps;
}

void Input::focus_lost() {
    if (!focused_) return;
    focused_ = false;
    keys_.fill(false);
    mouse_.fill(false);
    recompute();
}

void Input::focus_gained() { focused_ = true; }

}  // namespace ds_port
