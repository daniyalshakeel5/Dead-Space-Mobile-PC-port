# Xperia Linux QEMU: real user graphics failure and corrective patch

**2026-11-03 — from three device logs**:
`game-20261103-000055.log`,
`game-20261103-000226.log`,
`game-20261103-000431.log`.

All 3 logs agree:

- User's original library is found with correct 4,499,468-byte size.
- QEMU executes the original-free EapRules ARM Linux loader v1.2.8.
- Actual video backend: X11 on the user's Linux desktop.
- **First real failure:** SDL_CreateWindow cannot load `libGL.so.1`.
- EGL 1.5 independently loads and initializes (Mesa Project), all five
  EGL entry points are present, `libEGL.so.1` is 32-bit ARM.
- Because no valid current OpenGL/GLES1 window/context is created,
  the game's imported GL functions cannot be resolved. The loader
  counts **182 missing symbols**, terminates before starting engine
  constructors, and avoids silently calling invalid pointers.
- This is not evidence of a bad Xperia game ZIP or a Reanimated-HD problem.

The previous player archive included ARMHF EGL, GLES1 and Mesa DRI but
**omitted ARMHF libGL.so.1 and the GLX dispatch libraries**. The upstream
engine tries GLES1 first then falls back to desktop GL 2.1 compatibility
because software Mesa may lack a working native GLES1 provider.

## Fix and validation

The build now includes `libgl1:armhf`, `libglx0:armhf`,
`libglx-mesa0:armhf` and `libglvnd0:armhf`. Its
`linux-portable/graphics_probe_arm.c` compiles to an ARM32 ELF then
executes under the **same** bundled QEMU + ARMHF root as the game,
under an Xvfb X11 desktop. The probe requires the ARM guest to
`dlopen(libGL.so.1)`, `dlopen(libEGL.so.1)`, initialize SDL2,
create a real OpenGL SDL window and establish GLES 1.1 or desktop
compatibility GL context. The successful test is *original-free*;
it is not a claimed run of the proprietary game.

Verified CI build:
https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41507898076

A small graphics-only patch was added so Linux players with the
old 289 MB bundle do NOT need to redownload it:

`linux-portable/Apply-Graphics-Fix.sh` copies the four
required 32-bit ARM shared libraries into the existing guest sysroot
and runs the graphics probe on the user's own X11/Wayland desktop.
A failed probe leaves its diagnostics in `logs/graphics-probe-*.log`
and does not pretend the game is fully playable. No original EA
data is included.

Release pipeline publishes:
https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/releases/tag/xperia-linux-armhf-opengl-fix-2026-11-03

The user should extract the patch **into** the existing
`DeadSpaceMobile-Xperia-Linux` directory (where
`Run Dead Space.sh` already exists) and choose **Run as Program**
on `Apply-Graphics-Fix.sh`. Then launch the game as usual.
Do not delete or reimport `game-data/`.

The full game has **not** been verified as playable on the user's
system yet. The OpenGL failure is resolved in CI; future device logs
may reveal additional issues (GLES1 fallbacks, shaders/materials,
input, audio, save files or 3D content).
