# Windows Grab Gestures Fix

[Download the updated Windows ZIP](DeadSpaceMobile-Windows-Gesture-Fix.zip?raw=true)

## Enemy Grabs

- **Cross/A:** press repeatedly for tap prompts. Each press supplies one native
  tap response, not a stream of automatic taps.
- **Right stick:** flick in the indicated swipe direction, then return to centre
  before the next flick.
- **R1/right shoulder:** contextual swipe response using the original game's
  controller-compatible grapple routine. It only responds during swipe phases.

Ordinary controller movement/look/combat is suspended during the grab. Native
game handlers retain direction, target, timing, animation and progression rules;
the fix does not set progress or skip the grab. It does not synthesize raw
touchscreen down/move/up, SDL mouse events or Android key codes.

Mouse capture releases automatically during grabs and menus, showing the cursor
for physical clicks/drags. It resumes afterward only if previously requested.
**F1** toggles capture intent; Escape or focus loss cancels automatic resume.
The original on-screen tap/swipe artwork remains unchanged. Shake/motion-only
campaign interactions are not remapped by this update.

Automatic native-resolution borderless fullscreen, F11 window switching, audio
timing/buffering, native controller controls, sensor startup and HD-texture
support from the previous build remain included.

Copy the package files into the existing installation, keeping `game` and
`userdata`. Back up `userdata` before campaign testing. New installations can
select the matching upstream Fixes or Reanimated APK. No APK, assets, generated
game C, saves or test-only Mesa libraries are bundled. The maintained source
patch, controls and coverage audit are included in the ZIP.

## Verification

**[Windows regression passed on rerun](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42770995281/attempts/2)**

- Native-resolution fullscreen startup, an F11 fullscreen/windowed round trip,
  existing-install relaunch and Reanimated HD startup passed.
- Each startup completed 1200 genuine-game frames with exit code 0 and no empty
  audio queues. The native-controller run completed 14000 frames with native
  menu/loading continuation, movement and stomp, and no legacy touch/key input.
- The long run measured 46638 PCM frames/sec over 312 seconds, with no empty
  audio queues. The one-second deadline test measured 1003 ms including 200 ms
  of processing work. All eleven captures passed nonblank/size checks.

The first software-rendered attempt completed gameplay but failed the strict
audio limit with nine empty writes (limit eight); the unchanged binary passed
the rerun with zero. The audio implementation is unchanged from the previous
working build. This records scheduling variability, not a claim of exhaustive
long-session or audible-hardware validation.

All 30 local regressions pass, including native grab tap/swipe payloads, phase
guards, held-button behavior, flick rearm, stale/disconnected input, absent
targets, mouse capture release/resume/failure and VFP/FPSCR preservation. SDL
and non-SDL Win32 host linking passes. The public ZIP was downloaded again and
its checksum matches the local package.

An additional isolated donor-code ABI check executes the original grab input
routines. It confirms coordinate-free tap acceptance and native timer choice,
directional swipe acceptance/rejection, and the original R1 swipe-phase guards.
World/animation callbacks are stubbed in that check; it is not a playthrough.

The focused fixtures cover the new grab and capture behavior. The Windows
startup/controller regression is not a full campaign or actual enemy-grab
playthrough. Campaign grabs, other enemy types and later motion interactions
still need gameplay verification. Windows CI uses software OpenGL and SDL dummy
audio; it does not establish audible WASAPI hardware quality.

ZIP SHA256: `c2a9ee9f8d49fc5bc6a71c344d52d4dad1f372a252dca8e5a695947ccd7a59f4`

EXE SHA256: `2dd823dcda28b99ac38c847cfe2dd3a13e5655d865327f82637a800530c40a10`

## Previous Builds

[Previous audio/fullscreen build](DeadSpaceMobile-Windows-Audio-Fix.zip?raw=true)
passed [Windows verification](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42766234928),
but predates the grab-response and automatic cursor-release fixes.

[Previous native-controller build](DeadSpaceMobile-Windows-Native-Controller.zip?raw=true)
predates the audio and fullscreen fixes. The older startup-fix ZIP retains the
old controller frontend and is not recommended for native-controller testing.
