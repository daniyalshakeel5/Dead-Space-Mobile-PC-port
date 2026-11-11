# Windows Desktop Fix Candidate

[Download the desktop-fix Windows ZIP](DeadSpaceMobile-Windows-Desktop-Fix.zip?raw=true)

This candidate addresses the reported fullscreen cursor, recording and
click-away problems while retaining the campaign-tested native controls,
grab gestures and audio implementation.

- The Windows system arrow is drawn into the game framebuffer, including
  screenshots and captures, whenever mouse-look capture is off.
- Native-resolution fullscreen uses a normal composited borderless window,
  without exclusive fullscreen ownership or automatic minimize on focus loss.
- Tracked inputs are released before pause; unmatched pointer releases and
  input into a paused guest are suppressed. Background resize is applied on
  resume, and paused loops no longer consume the requested frame budget.
- The game is a GUI executable, not a console application. The launcher runs
  hidden and reports errors graphically while retaining session logs.

Copy the files into the existing installation, keeping `game` and `userdata`.
Back up `userdata` before testing. F11 switches fullscreen/windowed; F1 remains
the mouse-look capture toggle. The native-controller/gesture mappings are
unchanged. No APK, game assets, generated game C or saves are bundled.

## Verification

**[Full Windows regression is queued](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/43096098131). It has not passed yet.**

All 31 local regressions pass, including input release before pause, duplicate
focus handling, controller/gesture semantics, capture intent, sensors, audio
pacing and VFP preservation. The native GUI game and synthetic GUI/non-GUI
Win32 hosts compile/link; GUI subsystem 2 is verified. The public ZIP checksum
matches the local package.

A preceding candidate passed Windows fresh/existing/HD startups and produced
an external desktop screenshot containing the game image and in-frame cursor.
Its integration probe stopped at the outside-window OS-cursor assertion.
The revised probe establishes a headless OS-cursor baseline, independently
requires visible in-frame cursor pixels, and exercises actual click-away/
resume cycles, background resize and desktop captures. Its full result is
still pending; none of these notes claim complete Windows runtime validation.

The owner reports completing the campaign on the prior gesture build. That
does not certify a full playthrough of this new desktop-only rebuild.

## Capture Scope

Use the actual Dead Space Mobile process/window, not a launcher console.
For OpenGL, use OBS Game Capture or modern Windows Graphics Capture. Legacy
BitBlt-only window capture can still be incompatible with OpenGL. ShareX
desktop/region capture uses the composited desktop path. Actual OBS hooks,
encoder configurations, multi-monitor/DPI hardware and the reported click-away
problem still require verification on the owner's PC.

ZIP SHA256: `b65ac9ec0211fb7202ce33661569af8924fc9311d6d390dd259e1746e9a7dec6`

EXE SHA256: `91987664942a335364e40a1497ef7bd904737096ddd229b95aed40fc27c9a0b0`

## Previous Build

[Campaign-tested gesture build](DeadSpaceMobile-Windows-Gesture-Fix.zip?raw=true)
is retained as a fallback. It predates the desktop fixes in this candidate.
