DEAD SPACE MOBILE - WINDOWS CONTROLLER TEST - 2026-11-06

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
Left stick: movement (18% radial dead zone, proportional input)
Right stick: camera look; works without capturing the desktop mouse
D-pad: original menu/navigation keys
Cross: original confirm/interact key
Circle: back; Options: original Start/Pause key; Share: Select
Square/Triangle: original Xperia face actions
L1/L2: original left-shoulder action; R1/R2: right-shoulder action
Combat actions and walk/run tuning have not been fully verified.

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
This is an early test build, not a completed game port.

Binary source commit: 5151cd670a0645d1ea150fcf50637c38c749c309
Windows test: https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42228747637
Development: https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/pull/4
