# Dead Space Mobile — portable Linux QEMU runtime for x86-64

This test bundle is designed for a normal Linux desktop. It requires
**no WSL2, Docker, Git, Python, compiler, root access or terminal setup**.

It contains a statically linked x86-64 QEMU translator, an upstream
GPL ARM Linux Dead Space runtime, ARMHF desktop SDL2/GLES1/Mesa libraries,
a tiny unzip tool and a graphical-file-picker-aware launch script.
The archive includes **no EA game files** or Reanimated assets.

## Testing

1. Extract the ZIP somewhere in your Linux home folder.
2. Copy your already supplied Xperia Play **`deadspace.zip`** next to
   `Run Dead Space.sh`, or select that ZIP when the launcher prompts.
3. Optionally copy `DeadSpaceReanimated-1.3.apk` beside the launcher.
   It stores HD KTX files separately but does NOT activate HD textures.
4. Double-click `Dead Space Mobile.desktop` (depending on your file
   manager, first mark it trusted) or select **Run as Program** for
   `Run Dead Space.sh`.
5. The first run checks the original ZIP's SHA256 and unpacks the exact
   Xperia v1.1.33 game. Later runs reuse these local assets.
6. If booting/rendering fails, share the latest `logs/game-*.log`.

This is an **experimental real-game launch**, not yet verified on the user's
Linux GPU. EapRules separately demonstrated original-game startup through
qemu-arm on Linux in an offscreen software-rendering test. Real interactive
window, audio, keyboard/mouse and DualShock 4 require device testing.
This portable package selects the normal SDL2 video/audio drivers
instead of offscreen/dummy backends and uses Mesa llvmpipe for ARMHF GLES.
Performance could be poor on low-end machines.

Only x86-64 Linux is supported by this test package. Running the upstream
ARMHF loader natively on an ARM64 Linux system is a different build.

Licenses: EapRules (GPL-3.0), QEMU (GPL-2.0), BusyBox (GPL-2.0), Ubuntu
ARMHF packages (multiple licences, see bundled copyright notices). Game
code and original assets remain owned by their respective rightsholders.
No game assets or APKs are embedded or uploaded.
