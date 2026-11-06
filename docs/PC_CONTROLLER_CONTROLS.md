# PC controller controls

The portable SDL host now accepts analog stick input from mapped controllers, including the DualShock 4. This implementation uses SDL's controller mapping rather than hard-coded joystick axis numbers. The user confirmed working physical DualShock 4 left-stick movement on Windows on November 7, 2026. Other action bindings still need gameplay inspection.

| Control | Mapping |
| --- | --- |
| Left stick | Proportional movement through the game's touchscreen movement path |
| Right stick | Camera look, without needing F1 mouse capture |
| D-pad | Original directional keys / menu navigation |
| Cross | Original DPAD_CENTER plus touchscreen centre tap for doors/objects (provisional) |
| Circle | Original Xperia back key |
| Square | Original Xperia square action |
| Triangle | Original Xperia triangle action |
| L2 (hold) | Original Xperia L1: aim |
| R2 while L2 held | Original Xperia R1: weapon fire |
| R1 outside aim | Original Xperia D-pad Up: plasma saw melee |
| R2 outside aim | Touchscreen downward swipe: stomp (provisional) |
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
