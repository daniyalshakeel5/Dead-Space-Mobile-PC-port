# PC controller controls

The portable SDL host now accepts analog stick input from mapped controllers, including the DualShock 4. This implementation uses SDL's controller mapping rather than hard-coded joystick axis numbers. Physical DualShock 4 USB and Bluetooth operation still needs hardware verification.

| Control | Mapping |
| --- | --- |
| Left stick | Proportional movement through the game's touchscreen movement path |
| Right stick | Camera look, without needing F1 mouse capture |
| D-pad | Original directional keys / menu navigation |
| Cross | Original Xperia confirm/interact key |
| Circle | Original Xperia back key |
| Square | Original Xperia square action |
| Triangle | Original Xperia triangle action |
| L1 or L2 | Original Xperia left-shoulder action |
| R1 or R2 | Original Xperia right-shoulder action |
| Options | Original Start key |
| Share | Original Select key |

Combat-specific action meanings are not yet verified. There is no rumble, gyro or touchpad gesture mapping yet. Keyboard/mouse remain available as fallback; F1 toggles captured mouse look.

Both sticks use a circular 18% dead zone, rescaled continuously to full range outside it. Diagonal magnitude is capped at one. The left stick takes priority over WASD while tilted. Returning the stick to center releases the movement touch. Focus loss or controller disconnect releases held actions and touches. One controller owns gameplay at a time; another connected mapped controller can take over when it disconnects.

## Verification

The native game uses the genuine released APK library and assets. `DS_CONTROLLER_AXES` optionally creates an SDL virtual controller for repeatable input testing. It exercises SDL controller state and the same polling path as physical hardware; it does not replace the game. Format: `start:end:leftX:leftY:rightX:rightY,...`, raw signed 16-bit axes.

The first controller test holds forward during frames 18500–19500, then tests strafe and simultaneous look. Screenshot comparison must distinguish actual character movement from animated tutorial graphics. A clean exit alone is not a movement pass.

The dead-zone math was checked for center, small drift, half tilt, full tilt, sign and diagonal normalization. Windows and Linux real-game controller captures are pending review. Walking/running response and movement reversal remain tuning targets; full campaign playability and hardware performance have not been established.
