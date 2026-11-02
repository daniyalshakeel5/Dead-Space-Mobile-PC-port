# Windows-first runtime checkpoint — November 3, 2026

## Product decision

**Do all future development, packaging and hands-on player testing on
Windows 10/11 x86-64.** User explicitly prefers Windows to continued
Linux library and QEMU dependency troubleshooting.

- Existing WPF Diamond Rush-derived Dead Space launcher remains the GUI.
- Select original user-owned **Xperia Play deadspace.zip** and optional
  **Dead Space Reanimated APK** using native Windows file dialogs. Ship
  neither game's copyrighted files.
- Install a standalone Windows game **without WSL2, Docker, Linux
  distribution, Git, Python or a terminal**.
- DualShock 4 via Windows SDL2 controller mapping, L/R sticks for
  movement/camera, touchpad pointer/click for UI and interaction.
- Reanimated HD KTX resource sidecar is prepared; do not promise HD
  rendering until the original game actually uses those resources.
- Low-end Windows x86-64 performance matters.

## QEMU vs Windows

The Linux `qemu-arm` user-mode backend works because it translates
ARM **Linux syscalls** into host Linux syscalls. QEMU documents
supported user-mode OSes as Linux and BSD:
https://www.qemu.org/docs/master/user/main.html

On Windows, `qemu-system-arm` is technically available but would
require packaging and booting an entire ARM Linux OS including
32-bit SDL2, desktop GL 2.1/Mesa and the GPL ARM game loader, with
Windows->VM graphics/audio/controller forwarding. Full-system
emulation adds large complexity and overhead, and is not the
lightweight PC translation wrapper originally requested.

**Windows default: the existing 0BSD Dynarmic ARM32 JIT**, with
a Windows-native guest process/runtime: ELF relocations, real
Bionic/libc services, JNI, file I/O, OpenGL ES 1.1 to Windows
OpenGL/SDL, audio, timing and controller inputs. No QEMU dependence
in the player package. Keep Linux QEMU and the EapRules GPL port
as *reference evidence* and backup, but do not spend new development
cycles on user-facing Linux repair work. Do not assume a codebase
being GPL-3.0 grants us permission to merge into a differently
licensed binary without complying with its terms.

## Actual Windows engineering baseline

`experimental/xperia_armjit/` currently contains:
1. Native x86-64 ARM32 Dynarmic translator.
2. ARM ELF32 loader and relative relocation/application.
3. Guest memory and guest heap.
4. Bounded AAPCS host services for memcpy/memmove/memset/memcmp,
   strlen/strcmp/strncmp, malloc/calloc/free/realloc.
5. ARM guest-call runner, SVC callbacks and return trampoline,
   and original constructor address ordering.
6. Regression tests of real A32 instruction execution and guest calls.

**Windows-specific CI** is now added to
`.github/workflows/xperia-arm-jit-smoke.yml`; it compiles and runs
the exact same ARM32 translation/guest ABI tests on Windows x64.
Its results must be checked before labeling Windows JIT verified.

## Definition of actual Windows boot

The game must:
1. Load exactly the user's original Xperia Play ARM library from disk
   and bind every required import (no silent fake calls).
2. Complete real original ELF constructors and JNI_OnLoad in the
   translated A32 guest, with trace evidence.
3. Create a native Windows SDL2/GL window with the game's original
   3D main menu and graphics, not a mocked preview.
4. Load original scene data, submit real draw calls, produce audio
   and accept keyboard/mouse + DualShock 4.
5. Remain stable across scenes, saves and extended gameplay.

A successful synthetic A32 test or installed APK assets **does not**
count as game startup or a playable Windows port. Keep Play disabled
until at least real original menu rendering and event handling work.

## Next runtime components — in implementation order

- Native Windows library/import coverage instrumented against
  user-supplied original ARM ELF, without committing the .so.
- POSIX/Bionic file I/O and guest asset lookup constrained to
  the local installed game directory; log unresolved syscalls.
- JNI fake JavaVM state and runtime calls, modeled on observed actual
  original code and the licensed upstream research.
- Native Windows SDL2 OpenGL ES 1.1 (fixed-function) symbol dispatch
  and sound bridges; never invent unverified GL services.
- Real startup invocation, rendering and controller-driven interaction.

This is **one** Windows port; we are not shipping a separate Linux VM.
