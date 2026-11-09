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

Local asset-free native-controller, generator and Win32 host-link tests pass.
The Windows workflow tests the genuine game with a virtual SDL pad, captures
framebuffers, and rejects legacy pointer/key generation. Its latest run is
listed in [Actions](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/workflows/windows-startup-check.yml).
Runtime verification is pending for this new native-controller ZIP.

The [previous startup-fix build](DeadSpaceMobile-Windows-Startup-Fix.zip?raw=true)
passed [three Windows startup runs](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42690400367),
but retained the old controller frontend. Do not use it for native pad testing.

The ZIP includes the maintained source patch, build information, controls and
coverage audit. No APK, assets, generated game C or saves are bundled.
Physical controller hardware, motion hardware, campaign completion and long
sessions are not yet verified.

ZIP SHA256: `7b662c0f81eb79298062506c8e44bc161326a6991da0730a97fbadea60f7f555`

EXE SHA256: `3e01a9f5ad0dee8ba2536df296246cbc93abffab78dbf892fea8b4faaa325490`
