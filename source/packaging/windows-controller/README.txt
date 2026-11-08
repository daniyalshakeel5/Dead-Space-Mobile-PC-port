DEAD SPACE MOBILE - WINDOWS CONTROLLER + RENDERING DIAGNOSTICS - 2026-11-06

1. Extract the entire ZIP into a writable folder (e.g. Documents).
2. Connect your PS4 controller, preferably by USB for the first test.
3. Double-click Play Dead Space.cmd.
4. On first launch, select your matching Dead Space Mobile Fixes APK.
   The tested edition contains lib/armeabi/libDeadSpace.so with SHA1:
   64a3323cd3cc0c483a645b62af8508624163ba9b
   The Xperia APK containing libEAMGameDeadSpace.so is NOT interchangeable.
5. Use the mouse for the opening menus/difficulty selection. After the intro,
   use the controller in the corridor.

No compiler, WSL, Android emulator or administrator installation is needed.
The game assets are not included; first setup extracts them from your APK.
Windows 10/11, an x86-compatible PC and desktop OpenGL 2.1 are expected.

CONTROLS
Left stick: movement (confirmed on physical PS4 controller)
Right stick: sustained native touchscreen CAMERA DRAG (not repetitive action taps).
Options: pause (Android BACK, verified on previous Circle mapping)
Circle: intentionally not bound to pause; future Kinesis context mapping
Cross: centre-screen touchscreen tap and DPAD_CENTER for doors/objects
R1: upward upper-left screen swipe for plasma saw (needs physical verification)
L2 held: aim via original Xperia L1
R2 while aiming: fire via original Xperia R1
R2 without aiming: downward upper-left screen swipe for stomp (needs verification)
Physical DualShock 4 touchpad: swipe UP for plasma saw, DOWN for stomp.
Touchpad contact alone does not fire or tap the screen.
Square: reload; Triangle: original Xperia triangle; Share: Select
D-pad: Xperia directional keys (up overlaps melee key)
L1: reserved, no longer aliases L2 aim.

Keyboard/mouse fallback: WASD movement; F1 toggles captured mouse look.
Escape releases mouse capture and sends Back. Close the window to exit.
Controller disconnect and window focus loss release held input.

SAVES AND LOGS
Saves stay inside game/assets/var, matching the tested game's data layout.
Keep the whole folder when moving the game. Do not delete game to update
just the executable. Setup refuses to overwrite an existing game folder.
Logs are stored beside the launcher in logs, one pair per session.
If it fails, send the latest stdout/stderr/result files and launcher-error.txt
(if present). Also report your GPU, Windows version and USB/Bluetooth mode.

TEST STATUS
This exact executable completed 24,000-frame Windows and Linux-controller
code-path tests using genuine game assets and SDL virtual input. Captures
show movement to the first door and camera look during overlapping sticks.
The Windows executable was tested with Mesa software OpenGL; your physical
GPU and PS4 USB/Bluetooth connection still need testing. Audio, combat,
long sessions, level transitions and campaign completion are not verified.
The previous TouchPad module 1100 mapping disabled right-stick look on the user's
PS4 controller; that route must not be treated as verified. The camera now
uses continuous TouchScreen dragging: one DOWN, repeated MOVEs, one UP.
The diagnostic modes are selectable in PowerShell:
  .\\Launch.ps1 -CameraMode drag         (default, intended safe look)
  .\\Launch.ps1 -CameraMode move-only    (MOVE with no tap; experimental)
  .\\Launch.ps1 -CameraMode pulse        (old look, MAY FIRE while looking)
The same genuine-APK build is tested with all three camera modes.
The game reads texture_env_generic_starfield.m3g from the original APK, but
its background is still missing in captured opening-cutscene frames. Do NOT
consider the skybox fixed by the expanded far plane. Enable SkyDiagnostics to
record texture/cubemap and draw activity.
The original game passes a perspective projection matrix with near 0.05 and far
approximately 30. The new Windows renderer's 1200-unit far plane dramatically
restores exterior ship geometry in the genuine-game opening cutscene captures.
The launcher now enables this projection correction by default. Hardware
performance and later scenes should still be checked for depth-buffer issues.
For controlled A/B rendering tests, open PowerShell in the extracted folder:
  powershell -NoProfile -ExecutionPolicy Bypass -File .\\Launch.ps1 -RenderDiagnostics -InputDiagnostics -SkyDiagnostics
  powershell -NoProfile -ExecutionPolicy Bypass -File .\\Launch.ps1 -RenderMinFar 1200
  powershell -NoProfile -ExecutionPolicy Bypass -File .\\Launch.ps1 -DisableFog
  powershell -NoProfile -ExecutionPolicy Bypass -File .\\Launch.ps1 -DisableCull
  powershell -NoProfile -ExecutionPolicy Bypass -File .\\Launch.ps1 -OriginalFarPlane
Do not run different test switches simultaneously when comparing screenshots.
Audio is intentionally not addressed in this build.
This is an early test build, not a completed game port.

Binary source commit: 765a16fc6d0e91d436511a2834b637cf511deedd
Windows test: https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42356168351
Development: https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/pull/4
