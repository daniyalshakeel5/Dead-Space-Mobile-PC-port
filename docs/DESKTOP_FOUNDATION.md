# Native desktop foundation checkpoint

**Status: unverified until CI passes.** This is a **desktop input interface
and diagnostic**, not a PC port of the Dead Space Mobile game. There is no
original renderer, engine, mission, player controller, physics or gameplay
loop. Never label the diagnostic as a playable game.

## Architecture

- `include/ds_port/input.hpp`: portable action/state interface.
- `src/input.cpp`: deterministic press/hold/release handling, focus release.
- `apps/input_probe.cpp`: optional SDL2 hardware keyboard/mouse tester.
  F1 toggles relative capture; F10 exits. Displays input states in title.
- `tests/input_tests.cpp`: independent deterministic tests.

Initial key mapping follows the original Dead Space PC conventions:
WASD movement, Shift sprint, right mouse aim, left mouse melee outside
aim or fire while aiming, Space stomp outside aim or alternate fire while
aiming, E interact, R reload, Q heal, B locator, Tab inventory, C stasis,
F kinesis, Escape pause, 1-4 weapon hotkeys and weapon mouse-wheel.
Mapping is provisional relative to **mobile** functions; it is *not*
a claim about recovered mobile engine behavior. PC reference:
https://gamefaqs.gamespot.com/pc/949616-dead-space-2008/faqs/55328

## Remote builds

GitHub Actions `.github/workflows/desktop-smoke.yml` compiles/tests
the dependency-free C++17 core on Windows and Ubuntu. It also compiles
the optional SDL2 tester on both platforms. Windows builds a pinned,
checksum-verified SDL2 2.30.12 source; Ubuntu uses system SDL2.
No heavy work is run in ChatGPT's local VM.

## Reproduce (optional)

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDS_BUILD_INPUT_PROBE=OFF
    cmake --build build --config Release
    ctest --test-dir build --build-config Release --output-on-failure

For the optional SDL2 tester, install SDL2 development files and pass
`-DDS_BUILD_INPUT_PROBE=ON`. Or add `-DDS_FETCH_SDL2=ON` to
download the pinned SDL2 source automatically.

## Real port dependency

The source repository's main branch contains README.md and front.jpg,
not decompiled source or the original APK. The archived APK held in the
user's private Library remains separate. The research workflow run
41236282588 was green but **skipped the APK analysis**.

See `docs/APK_INPUT_SETUP.md` to make the original APK available
privately to the GitHub-hosted research job. First verify the ABI,
native entrypoints, mobile update loop and asset readers. Only then
bind input actions and game state to reconstructed functions.
