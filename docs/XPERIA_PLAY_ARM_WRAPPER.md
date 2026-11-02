# Xperia Play ARM-to-x86-64 wrapper — checkpoint 2026-11-02

Independent experimental branch: work/deadspace-xperia-arm-wrapper-2026-11-02.
The original native reconstruction is still intact on its original branch.

## Verified user's Xperia Play executable

- Game: Xperia Play Dead Space Mobile v1.1.33
- Filename: libEAMGameDeadSpace.so
- ELF32 ARM ET_DYN, little-endian, EABI5.
- Bytes: 4,499,468.
- SHA1: 0ed42b611415015807f759ec9b5457857143ce39
- SHA256: 1ff0a34a2ef35f50c0031b383d8fd7b14369dca0f41969c5d4da8fabe569ed38

The SHA1 **exactly** matches the required Vita port executable from
https://github.com/v-atamanenko/deadspace-vita.

The user's newer/Fixes / Reanimated original ARM library is a DIFFERENT
edition: no Vita binary patch offsets may be assumed to match it.

## Static guest runtime facts

ELF imports libc.so, libstdc++.so, libm.so, liblog.so and libGLESv1_CM.so.
Relocations: 22677 R_ARM_RELATIVE, 890 R_ARM_ABS32, 294 R_ARM_JUMP_SLOT,
52 R_ARM_GLOB_DAT = 23913 entries, plus 836 bytes of init_array.
JNI exports include JNI_OnLoad, NativeOnCreate, NativeOnSurfaceCreated,
NativeOnDrawFrame, NativeOnPointerEvent, NativeOnKeyDown, NativeOnKeyUp and
NativeOnAcceleration. These original ABI requirements must be preserved.

## Code now added to this branch

- tools/xperia_play_elf_preflight.py:
  bounded static ELF32 header, PT_LOAD and dynamic section parsing,
  hash-based symbol bounds, relocation scan and original SHA1 validation.
  Refuses an unmatched donor when run as a CLI; never executes ARM code.
- experimental/xperia_armjit:
  Dynarmic JIT source pinned to 0BSD-licensed upstream commit
  a41c380246d3d9f9874f0f792d234dc0cc17c180.
  Genuine ARM32 instructions execute in a bounded guest-memory
  test and store the computed value. These are SYNTHETIC instructions
  only, not the game's binary.
- Synthetic tests for ARM ELF, and isolated CI for JIT build.

Local preflight:

    python -m tools.xperia_play_elf_preflight --so /private/libEAMGameDeadSpace.so

JIT smoke test:

    cmake -S experimental/xperia_armjit -B build-xperia -DCMAKE_BUILD_TYPE=Release
    cmake --build build-xperia --target xperia_arm32_jit_smoke
    ctest --test-dir build-xperia --output-on-failure

## Frankenstein sources, respecting licences

Vita controls and startup sequence (MIT):
https://github.com/v-atamanenko/deadspace-vita

EapRules' Linux/ARM game loader (GPL-3.0):
https://github.com/EapRules/deadspace-native-arm

Dynarmic (0BSD):
https://github.com/lioncash/dynarmic

EapRules' implementation has *actually executed* the SAME Xperia SHA1
library on Linux ARM with Bionic/JNI glue, PVRTC decompression, audio,
OpenGL and gamepad input. It is the best functional reference for
service calls, asset paths and game startup, but its ARM-native
machine-code loader cannot run ARM code on x86-64 without a guest JIT.
Copying GPL-3.0 code into the production runtime would require
following GPL-3.0 conditions and retaining attribution.

The Vita code supplies the verified JNI order:
module load -> relocations/imports -> constructors -> JNI_OnLoad ->
audio startup -> NativeOnCreate -> NativeOnSurfaceCreated ->
visibility callback -> NativeOnDrawFrame main loop.

The DS4 bridge already added to our main development branch provides
keyboard, touch and virtual-stick mapping. The actual ARM game
requires native-to-guest ABI dispatch (softfp), runtime queue and
callbacks; it cannot consume the bridge until the JIT services exist.

## Verified integration checkpoints

- Exact binary identity of the user-provided Xperia Play ELF checked locally:
  SHA1 0ed42b611415015807f759ec9b5457857143ce39,
  matching the Vita port exactly.
- Original-free ELF preflight synthetic suite passes both Linux/Windows:
  https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41463000643
- Real Dynarmic A32-to-x86-64 instruction execution passed Linux:
  https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41462563178
- Native C++ ELF PT_LOAD mapper and R_ARM_RELATIVE relocation tests
  passed Linux and Windows:
  https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41463391390
- Joined proof: a relocated synthetic ELF32 ARM code section executes
  through Dynarmic on x86-64 and writes the expected value into the
  loader's sparse ELF guest memory. Linux JIT run passed:
  https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41463670448

The **actual** user's ARM ELF was statically inspected (392 undefined
dynamic imports) but has NOT been run in the JIT. Three components
are connected: ELF address mapping, relative relocations, and A32 JIT.
The ~1,236 non-relative relocation sites (ABS32, GLOB_DAT, JUMP_SLOT)
need symbol resolution, followed by guest-to-host thunking for Bionic,
C++, JNI, GLES, VFP, audio and files.

## Pragmatic playable-route option

EapRules reports its Xperia Play loader reaching an interactive 3D
game through qemu-arm user mode on Linux x86-64 with llvmpipe.
That is a potentially faster short-term playability proof than rewriting
all Android ABI thunks for Dynarmic. It does NOT establish good gaming
performance, nor a working native Windows executable. A Windows
WSL2/Wine-style adaptation would need its own compatibility testing.
The independent Dynarmic path above avoids copying the GPL ARM-specific
loader while establishing a portable x86 JIT boundary.

## Known non-working parts

The original Xperia game has NOT been executed on x86-64.
Missing: guest ELF load/relocation into Dynarmic memory, 23913
relocations, host symbol thunks, Android JNI, constructor sequencing,
GLES 1.x rendering, PVRTC/texture support, audio, files, saves and
frame execution. These must be integrated before playable results
can be claimed.

The user's Library provided the exact .so, not a complete
verified Xperia assets/published tree. Existing Google Play/Fixes
assets are a separate game edition and are not guaranteed compatible.
EapRules observed black models on an ETC1 "full" donor lacking
appropriate PVRTC assets.

No game binaries, copyrighted audio or assets are committed to GitHub.
