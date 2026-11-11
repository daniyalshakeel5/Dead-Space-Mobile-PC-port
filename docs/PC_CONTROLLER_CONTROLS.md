# PC controller controls

The portable SDL host now accepts analog stick input from mapped controllers, including the DualShock 4. This implementation uses SDL's controller mapping rather than hard-coded joystick axis numbers. The user confirmed working physical DualShock 4 left-stick movement on Windows on November 7, 2026. Other action bindings still need gameplay inspection.

| Control | Mapping |
| --- | --- |
| Left stick | Proportional movement through the game's touchscreen movement path |
| Right stick | Camera via *TouchPad* module, never action touchscreen (pending hardware verification) |
| D-pad | Original directional keys / menu navigation |
| Cross | Original DPAD_CENTER plus touchscreen centre tap for doors/objects (provisional) |
| Circle | Original Xperia back key |
| Square | Original Xperia square action |
| Triangle | Original Xperia triangle action |
| L2 (hold) | Original Xperia L1: aim |
| R2 while L2 held | Original Xperia R1: weapon fire |
| R1 outside aim | Native upper-left touchscreen swipe **up** (plasma saw) |
| R2 outside aim | Native upper-left touchscreen swipe **down** (stomp) |
| Options | Android Back: pause (observed on physical controller) |
| Circle | No pause, reserved pending verified Kinesis input |
| Share | Original Select key |

Combat-specific action meanings are not yet verified. There is no rumble, gyro or touchpad gesture mapping yet. Keyboard/mouse remain available as fallback; F1 toggles captured mouse look.

Both sticks use a circular 18% dead zone, rescaled continuously to full range outside it. Diagonal magnitude is capped at one. The left stick takes priority over WASD while tilted. Returning the stick to center releases the movement touch. Focus loss or controller disconnect releases held actions and touches. One controller owns gameplay at a time; another connected mapped controller can take over when it disconnects.

## Verification

The native game uses the genuine released APK library and assets. `DS_CONTROLLER_AXES` optionally creates an SDL virtual controller for repeatable input testing. It exercises SDL controller state and the same polling path as physical hardware; it does not replace the game. Format: `start:end:leftX:leftY:rightX:rightY,...`, raw signed 16-bit axes.

The first controller test holds forward during frames 18500–19500, then tests strafe and simultaneous look. Screenshot comparison must distinguish actual character movement from animated tutorial graphics. A clean exit alone is not a movement pass.

The dead-zone math was checked for center, small drift, half tilt, full tilt, sign and diagonal normalization. Windows run [42228747637](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42228747637) and Linux run [42228867541](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42228867541) each completed 24,000 frames without crashing. Reviewed captures show left-stick traversal to the first door and right-stick camera movement during overlapping stick input. Each had zero mismatches across 26,466 checked read/copy transfers. Walking/running response and movement reversal remain tuning targets; full campaign playability and hardware performance have not been established.

## 2026-11-07 control and renderer follow-up

Physical left-stick movement is confirmed by the user's Windows hardware test.
Circle's old BACK mapping paused the game. Options now receives BACK, and
Circle does not send it. Cross synthesizes an independent finger-3 tap at the
screen centre plus the original DPAD_CENTER; actual door activation still needs
an in-game hardware check. Xperia L1/R1 are routed to L2 aim / R2 fire only
while aiming; R1 outside aim sends original DPAD_UP plasma-saw, and R2 outside
aim sends a downward swipe with finger 4 for stomp. These latter actions are
**not** claimed verified just because the frame loop completes.

Genuine-APK automation includes DS_CONTROLLER_BUTTONS and
DS_CONTROLLER_TRIGGERS for input-path exercise without synthetic game assets.
The renderer's earlier glFrustumf/x A/B tests did not report frustum calls.
The new native OpenGL path instruments glLoadMatrixf/x and glMultMatrixf/x,
which can receive a prebuilt projection matrix; DS_RENDER_NO_CULL is a
separate back-face test. Neither matrix-far nor cull-off is a verified fix
without comparing the original cutscene screenshots.

## November 7 physical-controller follow-up

The user's previous real-Windows test confirmed left-stick movement, but found
that right-stick camera panning fired the weapon and that the previous
D-pad-Up melee mapping did not work. The underlying bug was that the camera
emitted pointer events with the game's TouchScreen module. It now uses the
separate TouchPad module, as the original Vita implementation does.

The physical DualShock 4 touchpad now listens to SDL
`SDL_CONTROLLERTOUCHPADDOWN/MOTION/UP` events. A deliberate primarily
vertical swipe (at least 16% touchpad height) produces the game's *native*
touchscreen swipe in the upper-left quadrant:

- swipe up: plasma saw
- swipe down: stomp

Touchpad taps and horizontal camera movements do **not** produce screen taps
or fire. R1 invokes the same up swipe; R2 outside L2 invokes down swipe.
L2+R2 remains the original aimed weapon-fire path. Pointer 0 left-stick
movement stays unchanged.

`DS_PAD_SWIPES=start:direction,...` (direction -1 up, 1 down)
only injects controller **input events** in genuine-APK CI, not synthetic
game assets. `DS_INPUT_DIAG=1` prints `[CAMERA_TOUCHPAD]`, `[PAD_TOUCH]`,
`[PAD_SWIPE]`, `[PAD_ACTION]` and available hardware touchpads.

The original starfield file `texture_env_generic_starfield.m3g` is present
in genuine APK reads, but the intro's background is still black in prior
captures. `DS_SKY_DIAG=1` traces OpenGL cube-map binding/texture uploads/
draw counters. This is research instrumentation, **not a skybox fix**.

## 2026-11-07 right-stick regression and real-game camera A/B

The physical PS4 controller test found that changing right-stick gestures
from Android module TouchScreen (1000) to TouchPad (1100) **disabled look**
on this exact `libDeadSpace.so` APK, although another game's Vita port
used TouchPad. The CI logs only confirmed bridge delivery, not camera motion.
That inference was wrong; this regression is confirmed by the user.

The new default is `DS_CAMERA_MODE=drag`: the right stick uses the
formerly responsive TouchScreen module and a **continuous gesture** (one
DOWN, repeated MOVE, one UP at neutral). The old code issued repeated
DOWN/MOVE/UP every frame; those small taps could be interpreted as firing.
No changes to the user's verified left-stick movement or physical touchpad
melee/stomp paths are made.

For diagnosis the same executable supports:
- `DS_CAMERA_MODE=drag` (default; sustained touchscreen look)
- `DS_CAMERA_MODE=move-only` (MOVE without touchscreen DOWN; no tap)
- `DS_CAMERA_MODE=pulse` (old responsive look, but may cause repeated firing,
  use ONLY for controlled A/B tests)
- `DS_CAMERA_MODE=touchpad` (nonresponsive regression baseline).

The genuine-APK Windows CI captures the *same* cutscene/gameplay scene at
fixed frames with stationary player and scripted right-stick motion for
drag/move-only/pulse. A successful frame loop or input event alone does not
establish actual camera movement; compare screenshots around 20,100–20,900
and check that aimed/non-aimed panning does not accidentally shoot. Real
hardware behavior needs confirmation.


## 2026-11-09 — Owner's tested-binary native-controller patch

**This section supersedes the historical SDL touchscreen/keyboard mappings
above.** The latest uploaded Linux package already contained a *native*
controller integration. Its main executable was not generated by the earlier
`front_sdl.c` source in `main`; that legacy source has therefore been
restored unchanged.

**Actual native-controller test build:** the user's ZIP contains a symbolized
ELF32 `dspc_sdl` with SHA256
`fc1257844846043f6df78d01193d30b75a76e6022d025cd90cf17c7f4312bfa9`.
It has been patched using three SHA-verified x86 instruction edits and a
separate 32-bit SDL button remapper, with source in
`source/tools/native-owner-ds2-controls/`.

| Physical button | Behavior in owner's testing patch |
| --- | --- |
| L2 | Aim only when original native game sees an equipped weapon |
| R1 outside aim | Original native Plasma Saw/melee |
| R1 while L2 held | Original native alternate fire (previously Triangle) |
| R2 outside aim | Original native stomp |
| R2 during aim | Original native fire |
| L3 outside aim | Original 180-degree turn |
| L1 | Native run/sprint |
| Triangle while aiming | Original native Stasis |
| R3 | Locator |
| Cross, Square, Circle | Native interact, reload, Kinesis |
| D-pad Left/Right | Previous/next weapon |
| D-pad Up | Zero-G Jump (native action `0x352fb9f`; formerly D-pad Down) |
| D-pad Down | Unassigned |
| R3 | Locator (native action `0x352fb95`; replaces D-pad Up) |
| Options, Share | Pause/back, Rig |

The previous executable checked `getCurrentWeapon` and skipped other
gameplay action dispatch when unarmed. Its weapon-pointer check for the
unsafe `setAiming` call was retained, while unarmed actions are now routed
through native action dispatch and stomp is permitted. No action is fabricated
through touch swipes, physical keyboard events, or accelerometer sequences.

**Verification:** static exact-byte patch checks, ELF disassembly, archive
integrity and independent SDL remap checks passed. The 32-bit game was not
launched or tested with the original game's assets here. Real DualShock 4
hardware verification is still required, particularly for all pre-cutter
tutorial actions and Triangle Stasis. The game ZIP remains a local user-owned
artifact, not committed as source or game assets.
