DEAD SPACE MOBILE - NATIVE LINUX TOUCHSCREEN TEST
=================================================

This is the actual ARM->native x86 translation of Dead Space Mobile, built as
a 32-bit Linux ELF using your own original APK's game library and assets.
There is NO Android emulator, ARM runtime, or synthetic game-file substitute.
The package deliberately DOES NOT contain any copyrighted game files.

SUPPORTED SYSTEM
An Intel/AMD x86-64 Linux laptop, tablet, or convertible, including Debian
13 and Ubuntu systems with SDL2 touch events. ARM Linux is NOT supported
by this particular build. The executable is 32-bit (i686), so a 64-bit OS
requires the corresponding 32-bit runtime libraries.

DEBIAN 13 / UBUNTU PREPARATION (once, in a terminal):
  sudo dpkg --add-architecture i386
  sudo apt update
  sudo apt install libsdl2-2.0-0:i386 libgl1:i386 libgl1-mesa-dri:i386 \
      libc6:i386 libstdc++6:i386 libgcc-s1:i386 unzip

HOW TO LAUNCH
1. Extract this ZIP to a folder you can write to.
2. Run ./Play-Dead-Space-Touch.sh or open a terminal there and run:
     chmod +x Play-Dead-Space-Touch.sh
     ./Play-Dead-Space-Touch.sh "/path/to/Deas.Space.Mobile.Fixes.apk"
   If no APK path is supplied, the launcher uses a file picker (if Zenity
   exists) or asks for its path in the terminal.
3. First launch extracts your *own* original game's libDeadSpace.so and
   assets. Later launches reuse the installation and saves.
4. Use the device's touchscreen, not the PS4 controller. Drag with one
   finger for movement and a second finger for looking around. Tap the
   original game's on-screen controls for actions, doors, melee and stomp.
5. For a normal resizable window instead of full screen, run:
     ./Play-Dead-Space-Touch.sh --windowed

TOUCH INPUT IMPLEMENTATION
SDL_FINGERDOWN / SDL_FINGERMOTION / SDL_FINGERUP -> original
Java_com_ea_blast_TouchSurfaceAndroid_NativeOnPointerEvent
Each physical contact receives a stable individual finger index (0..9)
until release, using the *actual SDL drawable resolution* including HiDPI.
SDL's emulated touch->mouse events are disabled so an action does not get
fired twice. Lost focus or window closure releases all contacts.

If your Linux desktop converts touch into *mouse only* without emitting
SDL_FINGER events, check the latest logs for [LINUX_TOUCH] and report
the compositor, distro, touchscreen model, and whether you use Wayland
or X11. A mouse remains a fallback but has a separate pointer index.

OPTIONAL DEBUG:
  DS_INPUT_DIAG=1 ./Play-Dead-Space-Touch.sh --windowed
  DS_RENDER_DIAG=1 DS_SKY_DIAG=1 ./Play-Dead-Space-Touch.sh --windowed

Known limitations: skybox/background rendering and audio remain under
investigation. No gameplay, touchscreen, or physical hardware success is
claimed until visual/device tests confirm it. The default 1,200-unit
projection adjustment remains for the opening cutscene.

For any crash, send the newest logs/*.log and logs/*-exit-code.txt.
Do not redistribute the original APK, extracted files, or game assets.
