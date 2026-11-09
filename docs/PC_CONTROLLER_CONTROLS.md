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


## 2026-11-09 — Owner-requested controller overrides (current branch)

**These owner-selected mappings supersede conflicting historical descriptions above.**
Branch: `work/deadspace-owner-ds2-controls-2026-11-09`. This is a
**source-only change**, not yet merged into a packaged release or verified on
physical gamepad hardware. The ZIP of the latest Linux build was deliberately
not unpacked in ChatGPT's local environment, at the owner's request.

| Physical control | Owner-selected behavior | Status |
| --- | --- | --- |
| Left stick | Move, existing virtual joystick | Existing implementation |
| Right stick | Look, existing continuous drag | Existing implementation |
| L1 | Larger virtual-joystick deflection intended to sprint | Added, gait not yet validated |
| L2 | Aim; native game enforces Plasma Cutter availability | Existing input, pickup gate needs hardware verification |
| R1 without L2 | Plasma Saw melee (original upper-left swipe up) | Remapped |
| R1 with L2 | **Existing original Triangle (Android key 100) alternate-fire path** | Remapped; no new accelerometer/gesture bridge |
| R2 without L2 | Stomp (original upper-left swipe down) | Restored |
| R2 with L2 | Weapon fire (Android key 103) | Existing |
| L3 without L2 | 180-degree quick turn (existing Android key 100) | Remapped |
| R3 | Locator (same native key as D-pad Down) | Remapped, needs game verification |
| Cross | Interact/confirm, including center-screen touch | Existing provisional implementation |
| Square | Reload (Android key 99) | Existing |
| Triangle | Not mapped to key 100: that would duplicate R1 alternate fire | Needs a verified standalone Stasis input before assigning |
| Circle | Unmapped; Android BACK would pause instead of using Kinesis | Existing limitation |
| Options | Pause | Existing |
| Share | Select | Existing |
| D-pad | Existing navigation; Down also triggers Locator | Existing |

**Before weapon pickup**: the host does **not** globally disable
combat controls. Melee, stomp, movement, interaction and L3 quick turn
are forwarded immediately; only aiming is subject to the native game's
weapon/progression logic. No claim is made here that the original game's
progression gating has been modified or verified. The Plasma Saw is used
before the Plasma Cutter is collected in the tutorial.

**Alternate fire**: the user's confirmed working alternate-fire action was
Triangle *while aiming*. Its genuine JNI input action (Android keycode 100)
is preserved, now triggered by R1 while aiming. R1 without aiming remains
the touchscreen Plasma Saw swipe. There is no synthesized acceleration
gesture or replacement gameplay logic.

**Remaining validation**: confirm pre-cutter Plasma Saw and stomp, L3 180
turn, R1 aimed alternate fire, L1 movement speed, R3 Locator, focus-loss
release and whether the game's original aim gate exactly matches the
requested Plasma Cutter pickup point. Triangle/Stasis and Circle/Kinesis
are not claimed complete until their distinct native actions are found.
