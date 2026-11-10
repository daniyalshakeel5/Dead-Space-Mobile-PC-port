# Windows Audio And Fullscreen Build

[Download the updated Windows ZIP](DeadSpaceMobile-Windows-Audio-Fix.zip?raw=true)

Windows now detects the display under the pointer and starts in borderless
fullscreen at its desktop resolution. Per-monitor DPI awareness keeps rendering
and mouse coordinates aligned. F11 toggles fullscreen/windowed; `Launch.ps1
-Windowed` starts in a window.

The audio submission loop uses high-resolution Windows deadlines that account
for processing time, with higher-priority mixer/submission threads, prebuffering
and prompt low-water refilling. The queue starts at 4096 frames; underruns add
bounded headroom up to 12288 frames (256 ms at 48 kHz), which decays after stable
playback. Resizing never discards PCM. The original 48 kHz stereo mixer and
decoders are unchanged.

Native controller movement, look, combat events and live menu callbacks remain.
The controller path does not generate screen taps, swipes or Android key codes.
The original game's touchscreen tutorial artwork remains unchanged.

Copy the package files into the existing installation. Keep `game` and
`userdata`. New installations can select the upstream Fixes or Reanimated APK.
The sensor startup, VFP/physics and optional HD-texture fixes remain included.
No APK, assets, generated game C, saves or test-only Mesa libraries are bundled.
The maintained source patch and coverage audit are included in the ZIP.

## Verification

**[Windows verification passed](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42766234928)**

- Default startup detected the runner's 1024x768 desktop and opened native-size
  borderless fullscreen. The existing-install case passed an F11 round trip.
- Fresh start, existing-install relaunch and Reanimated HD startup each completed
  1200 genuine-game frames with exit code 0 and no empty audio queues.
- The native-controller run completed 14000 frames with exit code 0, no legacy
  touch/key input, native menu/loading continuation and gameplay movement/stomp.
  PCM delivery measured 46647 frames/sec with no empty writes over 330 seconds.
- The one-second deadline test, including 200 ms of simulated processing work,
  measured 1002 ms. Two injected 45 ms stalls caused no empty audio queues.
- All eleven captures passed nonblank framebuffer checks at their expected sizes.

All 29 local regressions pass, including native controller semantics, donor-hook
guards, deadline pacing, bounded audio-buffer growth/decay, queued-PCM
preservation, sensor behavior and VFP state. The SDL and non-SDL Win32 hosts link.
The public download was fetched back and its SHA256 matches the local package.

Windows Actions uses test-only software OpenGL and SDL dummy audio. It can check
PCM delivery timing and queue health, not audible WASAPI hardware quality.
Physical audio, multi-monitor/high-DPI hardware, armed combat, campaign completion
and long-session behavior still need hardware gameplay testing.

ZIP SHA256: `21da356dc206ea97f85dd96414cdf4c91c75f7690ccde70d4a9601c8d762f500`

EXE SHA256: `f51152e3108b0293907cdaf551e16a216359d51981bc1d9f65c159834dcbc27b`

## Previous Builds

[Previous native-controller build](DeadSpaceMobile-Windows-Native-Controller.zip?raw=true):
[Windows startup/controller verification](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42756082739)
passed, but this build predates the audio pacing and automatic fullscreen changes.

[Original startup-fix build](DeadSpaceMobile-Windows-Startup-Fix.zip?raw=true)
retains the old controller frontend. Do not use it for native-controller testing.
