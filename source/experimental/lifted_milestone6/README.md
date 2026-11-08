# dspc - Dead Space Mobile desktop host (milestone 1)

Loads the game's ARM library (`libDeadSpace.so`) on Unicorn, binds its imports to host
code, runs the static constructors and `JNI_OnLoad`.

## Build on Windows (VS Code)
1. Install: Visual Studio 2022 Build Tools (workload "Desktop development with C++"),
   Git, and vcpkg (`git clone https://github.com/microsoft/vcpkg`, run `bootstrap-vcpkg.bat`).
2. Set the environment variable `VCPKG_ROOT` to the vcpkg folder, then restart VS Code.
3. In VS Code install the recommended extensions (CMake Tools, C/C++).
4. Unzip your APK (rename `.apk` to `.zip`) into a folder named `apk/` here, so that
   `apk/lib/armeabi/libDeadSpace.so` exists. Keep your own copy of the game assets there;
   do not redistribute them.
5. Ctrl+Shift+P -> "CMake: Select Configure Preset" -> `windows-msvc`, then
   "CMake: Build". If vcpkg cannot find the port, check `vcpkg search unicorn`.
6. Run the "dspc: load libDeadSpace.so" launch configuration (F5).

Expected output: `208/208 initializers completed`, `JNI_OnLoad returned 0x10004`.
Add `--selftest` to also check the host-to-guest call mechanism.

Status: verified on Linux (g++ against Unicorn 2.1.4). Not yet built on Windows.

## Milestone 5: the real game runs natively and draws frames (tools/)
The whole game library is translated to C (ARM -> 32-bit x86, SSE2) and runs with a host that fakes the
Android pieces it needs. With a software OpenGL it plays through: EA splash -> "use headphones" ->
in-game hints -> "Select Difficulty" menu, with real assets, UI text and textures from your APK.

Linux / WSL (Windows host not ported yet). Needs gcc-multilib, 32-bit SDL2 (+ dev headers), `pip install unicorn pyelftools pillow`:

    # unzip your APK into ../apk/   (lib/, assets/, classes.dex ...)
    cd tools
    python3 gen_all.py                              # translate everything + generate the GL layer -> out/
    cd out && SDL_CFLAGS="-I/usr/include/SDL2" SDL_LIBS="-lSDL2" ./build.sh
    # run (DS_DATA = where the game finds its data; here the APK's assets folder)
    DS_WINDOW=1 DS_DATA=$PWD/../../apk/assets DS_ASSETS=../../apk/assets DS_STEPS=create,surface,frame \
      DS_FRAMES=0 ./dspc_sdl ../../apk/lib/armeabi/libDeadSpace.so      # DS_FRAMES=0: run until the window is closed

Useful switches: DS_GLLOG=file (log every desktop GL call instead of drawing), DS_SHOT=N:file.ppm,
DS_SHOT_EVERY=N + DS_SHOT_DIR, DS_CLICKS="frame:x:y,..." (scripted taps), DS_WATCHDOG=seconds (dump all
thread stacks if no frame completes), DS_NOVSYNC=1. `bt.py log [binary]` maps crash backtraces to game function names.
Left mouse button = touch. ESC quits.

Pieces: lift.py (ARM->C), overrides.py (libgcc helpers), host.c (loader, dispatch, libc shims),
threads.c (real pthreads), libc_files.c (file/dir I/O with bionic struct layouts), jni_vm.c (fake Android: assets,
strings, arrays, device info), gen_gl.py + gl_shim.c (GLES 1.x -> desktop GL 2.1, incl. fixed-point, ETC1 decode,
OES framebuffer/texgen mapping), front_sdl.c (window, GL context, mouse).

Not done yet: Windows build of the host (it uses POSIX threads/mmap/dirent), keyboard/controller/remapping,
audio (all audio imports are still no-ops), save-data separation (writes go to DS_DATA), draw_texture/matrix-palette
extensions (not advertised), GL_FIXED vertex arrays, higher-resolution/AA options, performance work (-O2, native hot paths).

## Controller support (Xperia Play mode)
The library already contains the game's Xperia Play gamepad code (KeyboardAndroidXperiaPlay). It is switched on when
the device reports manufacturer "Sony..." and model "R800...", so the host presents itself as a Sony Ericsson R800i by
default (override with DS_MANUFACTURER / DS_MODEL / DS_DEVICE; DS_MODEL=PC turns it off).
Keys follow the game's own mapping: Cross=23 (Enter/E, pad A), Circle=4+alt (Q, pad B), Square=99 (R, pad X),
Triangle=100 (F, pad Y), L1/R1=102/103 (Z/X, shoulders), D-pad=19-22 (arrows), Start=108 (P), Select=109 (Tab).
Scripted test input: DS_KEYS="frame:keycode:alt,...". Not done yet: analog sticks (the Xperia Play has none; the Vita port
turns sticks into touch input) and on-screen layout for move/look.
Upstream reference for the Xperia Play build: github.com/v-atamanenko/deadspace-vita (needs your own Xperia Play v1.1.33 APK,
lib/armeabi/libEAMGameDeadSpace.so, sha1 0ed42b611415015807f759ec9b5457857143ce39; the repo does not ship the game).
