# Dead Space Mobile native desktop port

The current Linux build translates the original game's ARM library into C and
compiles it into a native **32-bit x86 (i686)** executable. SDL supplies the
window, touchscreen events and audio output; a compatibility runtime supplies
the Android/JNI, graphics, file and threading interfaces the game expects.

- [All port source, launchers and tests](source/README.md)
- [Full technical breakdown and build instructions](docs/TECHNICAL_BUILD_GUIDE.md)
- [Linux touchscreen launcher and setup](source/packaging/linux-touchscreen/README-LINUX-TOUCH.txt)
- [Collision and accelerometer investigation](docs/LINUX_COLLISION_AND_ACCEL_FIX_2026-11-08.md)
- [768p and audio validation](docs/LINUX_AUDIO_AND_768P_2026-11-08.md)

On the tested Google Phaser360 Linux device, the build uses a 1366×768 drawable
and matching touch coordinates. The user confirmed working hit detection,
opening-cutscene skybox and audio. Real IIO accelerometer readings reach the
original game; a later motion-gated level has not yet been played through.

Use your own supported original APK. Game assets and the original library are
not supplied by the source tree. The current Linux executable does not need
an Android emulator or a runtime ARM emulator. Historical Unicorn experiments
and Windows work remain in `source/experimental/` with their own limitations.

Original Android package background: [XDA post](https://xdaforums.com/t/dead-space-android-13-100-brightness-fix-almost-21-9-full-screen-decompiled-apktool.4664045/).
