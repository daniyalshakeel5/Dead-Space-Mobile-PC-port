#include "ds_port/input.hpp"
#include <iostream>

using ds_port::Action;
using ds_port::Input;
using ds_port::Key;
using ds_port::Mouse;

#define CHECK(expr) do { if (!(expr)) { \
    std::cerr << "FAIL line " << __LINE__ << ": " << #expr << '\n'; return 1; \
} } while (false)

int main() {
    Input input;
    CHECK(!input.current().active(Action::Forward));

    input.key(Key::W, true);
    CHECK(input.current().active(Action::Forward));
    CHECK(input.current().started(Action::Forward));
    input.begin_frame();
    input.key(Key::W, true); // Repeated down must not emit another edge.
    CHECK(input.current().active(Action::Forward));
    CHECK(!input.current().started(Action::Forward));
    input.key(Key::W, false);
    CHECK(input.current().stopped(Action::Forward));

    input.begin_frame();
    input.mouse_button(Mouse::Left, true);
    CHECK(input.current().active(Action::Melee));
    CHECK(!input.current().active(Action::Fire));
    input.mouse_button(Mouse::Right, true);
    CHECK(input.current().active(Action::Aim));
    CHECK(input.current().active(Action::Fire));
    CHECK(!input.current().active(Action::Melee));
    CHECK(input.current().stopped(Action::Melee));
    input.begin_frame();
    input.mouse_button(Mouse::Right, false);
    CHECK(input.current().active(Action::Melee));
    CHECK(input.current().stopped(Action::Fire));

    input.mouse_button(Mouse::Left, false);
    input.begin_frame();
    input.key(Key::Space, true);
    CHECK(input.current().active(Action::Stomp));
    input.mouse_button(Mouse::Right, true);
    CHECK(!input.current().active(Action::Stomp));
    CHECK(input.current().active(Action::AltFire));
    input.key(Key::Space, false);

    input.begin_frame();
    input.key(Key::RightShift, true);
    CHECK(input.current().active(Action::Sprint));
    input.key(Key::LeftShift, true);
    input.key(Key::RightShift, false);
    CHECK(input.current().active(Action::Sprint));
    CHECK(!input.current().stopped(Action::Sprint));
    input.key(Key::LeftShift, false);
    CHECK(input.current().stopped(Action::Sprint));

    input.begin_frame();
    input.key(Key::Num2, true);
    CHECK(input.current().weapon_slot == 2);
    input.key(Key::Num2, true);
    input.wheel(2);
    input.wheel(-1);
    input.mouse_motion(12, -4);
    input.mouse_motion(-2, 3);
    CHECK(input.current().weapon_scroll == 1);
    CHECK(input.current().mouse_dx == 10);
    CHECK(input.current().mouse_dy == -1);
    input.begin_frame();
    CHECK(input.current().weapon_slot == 0);
    CHECK(input.current().weapon_scroll == 0);
    CHECK(input.current().mouse_dx == 0);
    CHECK(input.current().mouse_dy == 0);

    input.key(Key::E, true);
    CHECK(input.current().active(Action::Interact));
    input.focus_lost();
    CHECK(!input.current().active(Action::Interact));
    CHECK(!input.current().active(Action::Aim));
    CHECK(input.current().stopped(Action::Interact));
    input.key(Key::W, true); // Ignored when unfocused.
    CHECK(!input.current().active(Action::Forward));
    input.focus_gained();
    input.key(Key::W, true);
    CHECK(input.current().started(Action::Forward));

    input.begin_frame();
    input.key(Key::E, true);
    input.key(Key::E, false); // Two edges in same update frame.
    CHECK(input.current().started(Action::Interact));
    CHECK(input.current().stopped(Action::Interact));

    input.begin_frame();
    input.key(Key::G, true);
    CHECK(input.current().active(Action::ZeroGJump));
    CHECK(input.current().started(Action::ZeroGJump));
    input.key(Key::V, true);
    CHECK(input.current().active(Action::RotateWeapon));
    input.begin_frame();
    CHECK(!input.current().started(Action::RotateWeapon));
    input.key(Key::G, false);
    CHECK(input.current().stopped(Action::ZeroGJump));
    input.focus_lost();
    CHECK(!input.current().active(Action::RotateWeapon));
    input.focus_gained();

    std::cout << "All deterministic input checks passed.\\n";
    return 0;
}
