# Windows native rendering and physical DualShock 4 — 2026-11-06

## Hardware report and genuine-APK evidence

The user's Windows build `5151cd670a0645d1ea150fcf50637c38c749c309` launched successfully on an NVIDIA GTX 1650, rendered the opening cutscene and gameplay tutorial, and exited normally. Reported defects: extremely short apparent exterior-scene draw distance, physical DualShock 4 left stick not moving the character, and no audio. **Audio is explicitly deferred.**

The uploaded logs confirm game `JNI_OnLoad`, SDL2/GL initialization, frame-loop execution and `[CONTROLLER] connected: PS4 Controller id=0`. They do **not** confirm analog axis values or actual movement. The genuine-APK Windows controller-id0 evidence in Actions run `42228747637` shows the first-boot difficulty screen at frame 900, fragmented exterior ship against a black background at frame 7200, the movement tutorial by frame 18000, and the character advancing through the corridor to the first door between frames 18600–19350. These are real game assets, *not synthetic stand-ins*.

## Input correction

Previous `front_sdl.c` used finger ID 1 by default for movement. The successful automated controller-id0 case explicitly set `DS_PC_MOVE_FINGER=0`. The physical test used the default ID 1. This discrepancy is a plausible cause of the physical controller failure, but it remains to be confirmed on hardware.

The runtime now uses pointer 0 by default, retaining `DS_PC_MOVE_FINGER=1` for a comparison. It also replaces a tiny accumulating 0.25 px/frame move with a stable proportional displacement around the virtual joystick's touch anchor. On first actuation it sends a down at the anchor; from the next frame onward it sends move events to a stick-dependent target. The release event is sent on stick neutral/focus loss/disconnect. `DS_INPUT_DIAG=1` reports sampled left/right axes in stderr. None of these changes fake gameplay; inputs still flow into `NativeOnPointerEvent`.

## Renderer investigation (do not call this fixed yet)

The original windowed build uses GLES 1.x -> desktop OpenGL compatibility. `glFrustumf` and `glFrustumx` now use an explicit forwarding function, which leaves the original frustum untouched by default and can log its near/far values with `DS_RENDER_DIAG=1`.

Two isolated tests are available:

- `DS_RENDER_MIN_FAR=1200`: if a valid frustum's far plane is below 1200, extend the desktop clipping plane to 1200; no engine-side model/scene culling changes. This is **diagnostic**, not a verified final renderer fix.
- `DS_RENDER_NO_FOG=1`: suppress GL_FOG enable to see whether heavy fog rather than clipping explains the black void. This is diagnostic, not a normal graphics setting.

The genuine-game CI workflow now captures baseline, far-clip and fog-off screenshots with identical inputs. Inspect the *same cutscene frames* and the `[RENDER_FRUSTUM]` lines before deciding on a permanent rendering change. Avoid globally increasing the far plane blindly (may reduce depth precision and cause z-fighting).

## Windows test launcher

From PowerShell inside the package, call `./Launch.ps1 -InputDiagnostics -RenderDiagnostics` to capture controller axes and projection numbers. For comparison, `./Launch.ps1 -RenderMinFar 1200` or `./Launch.ps1 -DisableFog`. The one-click CMD launcher keeps original default settings. The package needs rebuilding against the new genuine-APK PE32 run; older packages still contain commit `5151cd6`.

## Acceptance criteria

1. Fresh native Windows PE32 compile with original released APK. Runtime exits without error or works interactively without crash.
2. On real DualShock 4, full left-stick tilt changes character position in tutorial, not merely logs input events; stick neutral stops movement. Right stick remains usable.
3. At the same first-run ship cutscene frames, compare screenshots against baseline and reference gameplay. Resolve missing distant geometry without z-fighting or disappearance of UI/near objects.
4. Leave the audio subsystem untouched until (2) and (3) are verified.
