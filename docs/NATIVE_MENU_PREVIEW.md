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

## Current limitations

The original EA and Dead Space splash screens and recovered **3D menu
background** load using the game's genuine geometry, camera, 13 external
texture resources and 17 appearance bindings.

The original 2D frontend layout, animations, engine-owned input event
queue, audio playback and full game simulation are **not implemented**.
Menu clicks and keys are forwarded to reconstructed JNI interfaces, but
without the original runtime event services they cannot change game state.

Closing the window is supported. The process may exit with code 20 to
indicate that full gameplay startup is still unavailable.

The builds have no game assets, ARM emulator, or translation runtime.
The recovered native 3D menu draw was verified headlessly with Mesa GL
in GitHub Actions; real PC graphics compatibility still needs testing.
