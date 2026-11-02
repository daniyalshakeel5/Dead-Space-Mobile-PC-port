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

## Latest actual-source and synthetic runtime progress

The user's private deadspace.zip was verified directly:

- Archive length: 145903794 bytes; SHA256
  04a71b975d0a9aa81b0388aee080f927903507cfbebddc8b57b8959f3eae2d31.
- Embedded exact Xperia 1.1.33 libEAMGameDeadSpace.so and campaign assets.
- EapRules' known vita-rip-atc profile: 1745 asset files, 254747912
  uncompressed asset bytes, 22 map scene files. ATC, not original full PVRTC.
- The data remains private. No source archive, EA library, textures, font
  bytes, maps or audio were uploaded to or committed in GitHub.

Changes implemented on this experimental branch:

1. tools/xperia_vita_donor_install.py — integrity-checked,
   no-overwrite transactional ZIP installer. Checks complete exact ZIP
   SHA256, ARM library SHA256, seven critical original asset hashes,
   original asset count/size, symlink/traversal/collision/size bounds.
   Synthetic Windows/Linux tests passed:
   https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41465861700
2. Corrected actual ARM32 guest base memory mapping. Previously ARM
   RELATIVE pointers were rebased but the JIT still addressed raw
   ELF virtual addresses. A joined guest-JIT test now actually
   dereferences the rebased pointer. ELF Windows/Linux tests passed:
   https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41466189278
   and the joined JIT Linux run passed:
   https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41466189392
3. A32 ELF dynamic-symbol binding: understands the original SysV
   DT_HASH, DT_SYMTAB, DT_STRTAB, DT_STRSZ plus ELF32 REL
   R_ARM_ABS32/GLOB_DAT/JUMP_SLOT. Internal/weak symbols are
   handled separately. Mandatory external symbols bind
   **atomically**, refusing missing guest thunks instead of
   pretending an Android runtime exists.
   Windows/Linux tests passed:
   https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41466707515
   The user's actual ELF has all 1236 non-relative relocations
   referencing undefined symbols (five weak), so the remaining
   mandatory symbols need real guest service implementations.
4. independent ATC_RGB and ATC_RGBA_EXPLICIT_ALPHA block decoder:
   experimental/xperia_atc/. Portable RGBA8 output for GPUs
   without ATC support. Asymmetric synthetic block + alpha/crop
   tests passed on Windows/Linux:
   https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41467220961
   **Not yet connected to IM-M3G texture extraction, GLES upload
   interception, or actual game rendering.**

Next real runtime integrations are:
- Convert the bound ARM import addresses into callable, bounded
  guest-to-host thunks, respecting AAPCS softfp and avoiding
  untrusted guest pointers.
- Implement the actual Bionic/libstdc++/libm, JNI, GLES1, audio
  and file-system services and original constructor sequencing.
- Prove original Xperia ELF executing at least JNI_OnLoad and
  NativeOnCreate with the verified donor, then frame rendering.
- Wire the existing PS4 DualShock 4 input bridge to the active
  game's JNI pointer and key callbacks and test gameplay interaction.
- Only after successful visual/functional tests claim a playable
  Windows/Linux translation build.

## Native ARM service and entrypoint reconstruction — 2026-11-02

The original Xperia ELF's actual undefined ARM symbol table contains
`memcpy`, `memmove`, `memset`, `memcmp`, `strlen`, `strcmp`,
`strncmp`, `malloc`, `calloc`, `free` and `realloc`.

This branch now has an original-free C++ implementation of **all eleven**
of those Bionic-compatible memory/string services:

- `experimental/xperia_guest_loader/guest_libc_bridge.{hpp,cpp}`:
  genuine AAPCS `r0`-`r3` exchange, bounded guest pointer validation,
  and synthetic ARM32 `SVC #slot; BX LR` veneer addresses at
  `0x70000000`; unknown imports and traps are rejected.
- `experimental/xperia_guest_loader/guest_heap.{hpp,cpp}`:
  bounded sparse 32-bit heap at `0x50000000` with genuine
  malloc/calloc/free/realloc memory semantics, allocation caps,
  invalid-pointer and double-free rejection, no raw host pointers.
- Native Windows/Linux unit tests:
  [41480116856](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41480116856)
  passed the memory/string and guest heap suites.
- Actual x86-64 Dynarmic **ARM guest BLX -> SVC -> host memcpy -> BX LR**
  roundtrip, using synthetic ARM machine code and the guest
  ELF memory map:
  [41479175462](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41479175462),
  green.
- Original ELF dynamic metadata parser now exposes ARM JNI exported
  function names plus the ordered `DT_INIT` and relocated
  `DT_INIT_ARRAY` pointers. Synthetic startup tests (including
  Thumb-tagged pointers and malformed-array rejection) passed
  Windows/Linux:
  [41480235614](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41480235614).
  Static inspection of the user's exact original ELF confirms
  **836 bytes / 209 entries** in its constructor pointer array,
  plus JNI_OnLoad and MainActivity/renderer lifecycle exports.
- `experimental/xperia_armjit/guest_arm_call_runner.{hpp,cpp}`:
  a reusable bounded AAPCS/softfp function call boundary with an
  owned 256 KiB guest stack, JIT callback/dispatch loop and explicit
  constructor ordering. Runs synthetic code; its separate
  cross-platform validation is still a requirement before using any
  original game code.

No proprietary game code or assets were added to the repository.
**Passing synthetic ARM code is not the same as running the original ELF.**

## Known non-working parts

The original Xperia game has **NOT** been executed on x86-64.
Now implemented: ELF PT_LOAD guest mapping, R_ARM_RELATIVE relocations,
a dynamic ARM import-table binder, a real ARM32-to-host service trap
roundtrip, eleven bounded libc/heap functions, and discovery of original
constructor/JNI addresses. The parser knows original constructor order,
but **original constructors have not been executed**.

Still missing: enough of the original 392 undefined symbols to link
successfully; Bionic services outside the eleven allowlisted functions,
pthreads, libstdc++ exceptions/ABI, real Android JNI, GLES 1.x rendering,
ATC/KTX texture upload integration, audio, asset/file-system services,
game saves and lifecycle/frame execution. The linked ARM guest runtime
must also be tested against the user's exact legally held Xperia ELF
before any claim of an actual boot or playable Windows/Linux build.

The user has now provided a complete verified **Vita-ready main campaign
donor** (reduced ATC version), in addition to the exact Xperia ARM ELF.
See docs/XPERIA_PLAY_DONOR_2026-11-02.md. The full PVRTC edition and
optional Survival/Burst Rifle assets are not included, but the known Vita
donor has a full campaign asset tree. Do not use a PVRTC-only decoder on
this donor or mix unrelated Google Play/Fixes assets.
EapRules observed black models on an ETC1 "full" donor lacking
appropriate PVRTC assets.

No game binaries, copyrighted audio or assets are committed to GitHub.
