# Dead Space Mobile lifted engine — Windows x86 build handoff

Date: 2026-11-04

## Goal

Deliver a **genuine Windows x86 PE32** game runtime built from the user-owned original
`libDeadSpace.so` and 16,387 ahead-of-time translated C functions. **No runtime ARM
emulation** is used. The original EA game library is loaded as relocated **data** by
a Windows native x86 host. Executable code is compiled to x86 from C using MinGW.

All original APK files, lifted `fn_*.c`, audio and models remain private, local and
excluded from the public GitHub repository and Actions artifacts.

## Implemented (GitHub source)

- `experimental/windows_native_host/win32_substrate.h/.c`: real Win32
  VirtualAlloc/VirtualFree/VirtualProtect, recursive CRITICAL_SECTION, CONDITION_VARIABLE,
  semaphore, TLS and native worker threads.
- `experimental/lifted_milestone6/tools/windows_lifted_compat.h`: actual lifted
  host's Win32 mappings, Win32 fatal-exception address logging, _mkdir/_fullpath,
  Win32 fsync and POSIX lrand48 compatibility.
- `experimental/lifted_milestone6/tools/host.c`: common Linux+Windows lifted
  ELF data loader; Windows-specific crash handler without removing keyboard/controller
  callbacks, original bootstrap routines, JNI, SDL2 or GLES.
- `experimental/lifted_milestone6/tools/libc_files.c`: Win32 bionic stat/dirent
  layout bridging and binary-mode resource opens.
- `experimental/lifted_milestone6/tools/build_win32_lifted.sh`: uses the
  existing owner's generated `tools/out/fn_*.c` private source, native
  `i686-w64-mingw32-gcc-posix`, pinned official SDL2 2.32.10 MinGW x86 SDK,
  SSE2 softfp flags and `-Wl,--large-address-aware`.
- `experimental/windows_native_host/Build-DeadSpace-Windows.cmd` + `.ps1`:
  graphical one-click Windows build orchestrator. WSL is used only as the
  **compiler** and to retrieve public dependencies. It DOES NOT run the Windows
  game through WSL or emulation.

## VERIFIED in GitHub CI

1. [Native Windows substrate x86 executable test](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41799588593):
   Compiled and executed a real Win32 PE32 in MSVC with eight synchronized worker
   threads, including memory, semaphore, condvar and TLS checks.
2. [Lifted host + SDL2/OpenGL cross-compile](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41802249351):
   The real milestone 6 host, JNI, file and threading source and SDL2 front end
   **compiled and linked** to native Win32 PE32 using MinGW with **synthetic guest
   function tables only**. Zero original EA code or data is present in CI.
3. [One-click kit validation and artifact](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41803176682):
   Windows PowerShell parsed the graphical launcher, Linux Bash validated the
   private cross-build script, and GitHub packaged an asset-free ZIP.

## NOT VERIFIED YET

- Compiling *all* the user's actual 16,387 translated game functions on Windows;
  the source is intentionally not available to GitHub CI.
- Windows original ELF data load, 208 constructors, JNI_OnLoad, actual boot /
  input, SDL2 real OpenGL driver, textures, frame loop, saves or gameplay.
- Audio is not yet implemented in milestone6; Claude owns VFP short-vector DSP
  and AudioTrack support.
- Analog-stick/mouse-look gameplay remapping remains Claude's work.

## User test (no terminal typing)

Download the source-only GitHub Actions artifact named
`DeadSpaceMobile-Windows-x86-PrivateGame-Builder`.

Extract outside the existing dspc project and double-click
`Build-DeadSpace-Windows.cmd`. The script finds the existing project
(including `tools/out/protos.h`, `apk/lib/armeabi/libDeadSpace.so`, and
`apk/assets`) and, if necessary, uses WSL's root account to install the
native Windows x86 cross compiler. It stages the Windows host in
`windows-build/host-src` and leaves Claude's original WSL game untouched.
It compiles all private generated C functions into
`windows-build/DeadSpaceMobile-Win32.exe`, copies the Windows 32-bit SDL2 and
winpthread DLLs, then creates/launches
`windows-build/Play Dead Space Windows.cmd`.

Launcher environment preserves Claude's **known working resource layout** by
using `DS_ASSETS` and `DS_DATA` at the original APK's extracted `assets`
directory, `DS_STEPS=create,surface,frame`, `DS_WINDOW=1`, `DS_FRAMES=0`,
and Xperia Play input spoofing (`R800i`).
Windows crash and engine logs:
`windows-build/logs/windows-run.log`; build errors:
`windows-build/compile-windows.log`.

**If either of those logs is returned by the user**, fix the exact observed
Windows compiler, OS shim, game load or runtime issue rather than inventing a
new renderer.

**Privacy:** The generated game C, original `.so`, assets and locally compiled
game executable are not uploaded to GitHub Actions. The public artifact is an
installer and independently written host tooling only.

**Status:** Windows **host native cross-link** VERIFIED; Windows **game boot**
UNVERIFIED. This test is intended to close that boundary with the owner's own
machine and licensed APK.
