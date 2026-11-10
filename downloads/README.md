# Windows Native Controller Build

[Download the native-controller Windows ZIP](DeadSpaceMobile-Windows-Native-Controller.zip?raw=true)

Replaces the old touchscreen/keyboard pad frontend with original game movement,
look, combat events and live menu-button callbacks. Controller input does not
generate screen taps, swipes or Android keyboard codes. The implementation is
reconstructed from the latest Linux release executable and its documented DS2
layout, including the equipped-weapon guard and unarmed melee/stomp behavior.

Copy the package files into your existing installation. Keep `game` and
`userdata`. New installations can select the upstream Fixes or Reanimated APK.
The earlier accelerometer startup fix, original EA audio mixer, VFP/physics
fixes and optional HD texture support remain included.

**[Windows verification passed](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42756082739)**

- Fresh start, existing-install relaunch and Reanimated HD startup: 1,200 frames
  each, exit code 0.
- Native controller: 14,000 genuine-game frames, exit code 0, driven only by an
  SDL virtual pad. Native loading-screen continue, difficulty OK and pause/resume
  callbacks were recorded.
- Gameplay analog movement/look, unarmed melee and stomp were recorded. Captures
  show the character moving through the opening level and advancing its tutorials.
- No legacy touch or Android-key input was generated in the controller run.
  All eleven captures were nonblank 1366x768 framebuffers.

The Windows-tested EXE hash matches the ZIP below. Test-only Mesa and SDL dummy
audio were used; physical GPU/audio behavior is not established by this run.
All 27 local regressions also pass, including native event layouts, unarmed
aim/fire safety, hold release, stale/disconnected input, VFP preservation, live
menu identity checks and native loading-screen continuation. Win32 host linking
also passed. Armed combat and later campaign workflows still need gameplay tests.

The original game's touch tutorial artwork/text remains; this does not mean the
controller is emulating touches. The recorded controller run generated none.

The [previous startup-fix build](DeadSpaceMobile-Windows-Startup-Fix.zip?raw=true)
passed [three Windows startup runs](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42690400367),
but retained the old controller frontend. Do not use it for native pad testing.

The ZIP includes the maintained source patch, build information, controls and
coverage audit. No APK, assets, generated game C or saves are bundled.
Physical controller hardware, motion hardware, campaign completion and long
sessions are not yet verified.

ZIP SHA256: `42b9cd5e44b41c9d1c0f5ed233db20998c254cc193d199f9a29b6c3660372a98`

EXE SHA256: `3ca956224167f5df7771d0d5592c822c6282091c328d2c6bcf0f4e14bb406ca0`
