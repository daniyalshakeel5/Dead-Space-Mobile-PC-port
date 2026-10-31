#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ds_port {

// Desktop input abstraction, not recovered original mobile engine codes.
// The mapping targets original PC keyboard/mouse behavior and can change.
enum class Key : std::uint8_t {
    W, A, S, D, LeftShift, RightShift, E, R, Space, Q, B, Tab,
    C, F, Escape, Num1, Num2, Num3, Num4, Count
};
enum class Mouse : std::uint8_t { Left, Right, Middle, Count };
enum class Action : std::uint8_t {
    Forward, Backward, Left, Right, Sprint, Aim, Fire, Melee,
    Stomp, AltFire, Reload, Interact, Heal, Locator,
    Inventory, Stasis, Kinesis, Pause, Count
};

constexpr std::size_t to_index(Key value) { return static_cast<std::size_t>(value); }
constexpr std::size_t to_index(Mouse value) { return static_cast<std::size_t>(value); }
constexpr std::size_t to_index(Action value) { return static_cast<std::size_t>(value); }
constexpr std::size_t action_count = to_index(Action::Count);

struct Frame {
    std::array<bool, action_count> down{};
    std::array<bool, action_count> pressed{};
    std::array<bool, action_count> released{};
    int mouse_dx = 0;
    int mouse_dy = 0;
    int weapon_scroll = 0;
    int weapon_slot = 0; // 1-4 on number press; 0 = none

    bool active(Action action) const { return down[to_index(action)]; }
    bool started(Action action) const { return pressed[to_index(action)]; }
    bool stopped(Action action) const { return released[to_index(action)]; }
};

class Input {
public:
    void begin_frame();  // Clear transients; retains held input
    void key(Key key, bool down);
    void mouse_button(Mouse button, bool down);
    void mouse_motion(int dx, int dy);
    void wheel(int steps);
    void focus_lost();  // Releases held input to prevent stuck keys
    void focus_gained();
    const Frame& current() const { return frame_; }

private:
    std::array<bool, to_index(Key::Count)> keys_{};
    std::array<bool, to_index(Mouse::Count)> mouse_{};
    Frame frame_{};
    bool focused_ = true;
    void recompute();
    bool held(Key key) const { return keys_[to_index(key)]; }
    bool held(Mouse button) const { return mouse_[to_index(button)]; }
};

}  // namespace ds_port
