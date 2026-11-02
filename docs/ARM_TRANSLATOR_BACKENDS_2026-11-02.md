# ARM translation backend decision: Houdini vs uEmu vs Dynarmic vs qemu-arm

Reviewed 2026-11-02 for **Dead Space Mobile Xperia Play v1.1.33** and the PC
launcher. The user requires *Windows and Linux x86-64*, a PS4 DualShock 4
controller/touchpad, local original ZIP plus optional Reanimated HD APK,
and most importantly **actual original-game startup**.

## Evidence inspected

- [Rprop/libhoudini](https://github.com/Rprop/libhoudini) is the
  prebuilt Android-x86 ARM **Native Bridge**, extracted partly from Nexus
  Player (GitHub `master` has historical `3.4.4.44334/`,
  `4.0.8.45720/`, and README). It includes native binary
  `system/lib/libhoudini.so`, the Android `system/bin/houdini`
  interpreter, and numerous **Android system ARM libraries**.
  Its `system/bin/enable_houdini` explicitly registers an ARM ELF handler
  at `/proc/sys/fs/binfmt_misc/register` using
  `/system/bin/houdini`. These are not Windows processes or standalone
  non-Android Linux loader imports. The repository has no buildable
  translator source or LICENSE file, and the GitHub API reports no
  declared license. **Don't redistribute or embed these opaque binaries
  in our launcher** and don't assume it can load Xperia ELF without an
  Android-x86 userspace.
- [alexhude/uEmu](https://github.com/alexhude/uEmu) is an IDA/Unicorn
  debugger for small functions and firmware, not a game or Android
  runtime. Its own README says complex OS/library code is unsuitable.
  It may help inspect specific ARM routines but isn't our game-runner.
- [lioncash/dynarmic](https://github.com/lioncash/dynarmic)
  is 0BSD-licensed ARM JIT *CPU translation*, with no Android JNI/Bionic,
  GLES1, audio or game asset system included. Our synthetic original-free
  x86-64 ARM call runner, relocations, guest heap and 11 native libc
  functions passed CI. This remains the portable *long-term native-PC*
  translation backend, but the original game is **not yet bootable** on it.
- [EapRules/deadspace-native-arm](https://github.com/EapRules/deadspace-native-arm)
  is the verified **Xperia Play 1.1.33 game** ARM/Linux loader, JNI,
  Bionic and GLES1/audio implementation (GPL-3.0).
  Its README reports the **real engine** reaching M7/7 with
  `qemu-arm` and llvmpipe: 570 frames, 84 content opens, 151
  texture uploads, 35,511 draw calls, 11 input events, 4 scene changes.
  This is the strongest demonstrated way to *boot the game on a PC*.
  It still runs ARM instructions through QEMU (compatibility mode);
  performance and Windows integration need testing. Keep GPL
  attribution/license with any derivative or distributed build.

| Runtime | Uses original ARM game | Portable Windows executable | Real game boot evidence | Decision |
| --- | --- | --- | --- | --- |
| libhoudini | Inside Android-x86 Native Bridge | No (Android process/runtime) | Not tested with Xperia donor here | Research only, not bundled |
| uEmu | Isolated code / IDA | No | Not full-game runtime | Debugger only |
| Dynarmic + our JNI/GLES bridge | Yes, eventually | Planned | Synthetic ARM tests only | Continue native wrapper |
| qemu-arm + EapRules ARM/Linux loader | Yes | Linux x86-64 / WSL2 or container | **M7/7 real game** upstream | **Shortest boot route** |

## Next boot integration

Use an opt-in **Linux/WSL2 compatibility backend** that launches the
independently built EapRules GPL ARM loader against the user's locally
installed Xperia ZIP. Compare `TRACE: module loaded`,
`TRACE: JNI_OnLoad returned`, `TRACE: NativeOnCreate returned`,
`TRACE: surface created`, frame counters and content telemetry; do
not mistake a window opening for a working game. Use the source repo's
actual `emulator/run.sh` and `harness/verify.sh` as the boot ground truth.
Do not commit the original EA game binary or assets.

**Do not** automatically download or install Houdini into the Windows
launcher. In addition to the OS mismatch, its distribution terms are
not established. The current launcher retains local ZIP and optional
Reanimated APK selection. HD KTX overrides and DualShock 4 gameplay
routing still require running engine integration.

This decision is based on repository contents and verified upstream
behavior, not performance measurements or an assertion that the current
Xperia branch can play the original game already.
