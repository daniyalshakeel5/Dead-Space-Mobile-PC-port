# Dead Space Mobile — Native original-menu preview

This is a **native Windows/Linux x86-64 build of the real reconstructed
Dead Space Mobile startup and original 3D menu renderer**.

The original game files are **not bundled**. The first launch requires
your own original game APK, and the renderer uses the assets extracted
from that APK.

## Windows

Place your APK beside `deadspace_native.exe`, open a terminal in that
directory, and run:

```powershell
.\deadspace_native.exe --boot-apk --apk ".\Deas.Space.Mobile.Fixes.apk" --assets-dir ".\installed-deadspace"
```

For subsequent launches, without reextracting the APK:

```powershell
.\deadspace_native.exe --boot-splash --assets-dir ".\installed-deadspace"
```

## Linux

Install the runtime packages: SDL2, OpenGL and zlib. On Ubuntu/Debian:

```sh
sudo apt-get install libsdl2-2.0-0 libgl1 zlib1g
```

Run the binary with your original APK:

```sh
chmod +x ./deadspace_native
./deadspace_native --boot-apk --apk ./Deas.Space.Mobile.Fixes.apk --assets-dir ./installed-deadspace
```

For subsequent launches:

```sh
./deadspace_native --boot-splash --assets-dir ./installed-deadspace
```

## Recovered native menu

The boot path now loads the real `layouts.xml.sb` frontend database,
317 original atlas sprites, and the original 480×320 menu UI canvas.
It renders the 3D background with genuine geometry/camera/13 external
scene textures and decodes 32 original M3G animation tracks.

The original UI_MAIN screen has six source-defined interactive controls.
The Profile, Options and Extras screens are composed from recovered
layout data, not fabricated full-screen atlas images.

**Controls:** Mouse clicks select original on-screen hit regions.
Tab/Shift+Tab and arrow keys cycle original clickable elements, Enter
activates the focused element, and Escape invokes an actual original
BTN_BACK entity when present. When the original JNI event queue is
unavailable, these actions use the reconstructed PC menu state machine;
they are not claimed to reproduce the entire Android UI controller.

The frontend keeps its native 3:2 aspect ratio on widescreen displays.
Clicks in pillarbox/letterbox margins are not treated as menu presses.

## Current limitations

Only menu transitions with recovered layout evidence are enabled.
Starting a profile or launching gameplay still requires additional
reconstructed engine callbacks. Original audio playback, gameplay
simulation and full menu interaction behavior remain incomplete.

The builds contain no game assets, ARM emulator or translation runtime.
Headless Mesa OpenGL and real-APK tests validate the native menu render
and UI layout; real hardware compatibility still requires testing.
