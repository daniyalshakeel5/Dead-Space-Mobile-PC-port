# Technical build guide: native Linux Dead Space Mobile

This document describes the native i686 touchscreen build and its source layout.
The implementation lives under [`source/`](../source/README.md). Historical
Windows/Unicorn experiments remain available but are separate execution paths.

## 1. What is being built

The project is a compatibility host plus an ahead-of-time ARM-to-C translator.
The original 2011 ARM game implements gameplay, physics, animation, asset
loading and audio mixing. The translator emits C for its functions; GCC compiles
that C and the compatibility host into a native 32-bit x86 executable.
The shipped Linux executable does not interpret ARM instructions during play.
Unicorn is used by separate investigation tools as an original-ARM test oracle.

The original library is still required at runtime: the host maps its data,
relocations, global objects, tables and initialization information. An asset-free
executable alone is not the complete game. The APK, extracted game files,
generated game-derived C and private captured memory are local build inputs or
outputs; they are not maintained port source.

The verified donor library has SHA1
`64a3323cd3cc0c483a645b62af8508624163ba9b`. It appears as:

- Fixes APK: `lib/armeabi/libDeadSpace.so`, with resources in `assets/`.
- Reanimated v1.3: `assets/game/libDeadSpace32.so`, with resources in
  `assets/game/assets/` and additional HD files in `assets/texture_hd/`.

Previous donor comparison found the embedded original libraries byte-identical
and all 1,729 published resource filenames/CRCs equal. Reanimated's ARM64
Android runtime is not loaded by this Linux port. Compatibility features are
implemented in the native host and verified translation patches.

## 2. Source map

Paths below are relative to `source/experimental/lifted_milestone6/tools/`.

| File | Responsibility |
| --- | --- |
| `lift.py` | Read ELF symbols and ARM instructions, emit C functions and control flow |
| `gen_all.py` | Generate chunks, prototypes, dispatch/import tables, overrides and GL bridge; apply guarded patches |
| `rt.h` | Guest types, loads/stores, bit conversions, arithmetic helpers, VFP state declarations |
| `host.c` | Load and relocate the original ELF, dispatch guest calls, bridge libc, bootstrap JNI/game lifecycle |
| `jni_vm.c` | Small Java/JNI compatibility layer: objects, methods, strings, arrays, assets and delegates |
| `threads.c` | Map compact Bionic synchronization objects to native pthread objects |
| `libc_files.c` | File/stream/directory functions, guest errno and Bionic structure layouts |
| `gen_gl.py`, `gl_shim.c` | GLES 1.x calls and fixed-point arguments to desktop compatibility OpenGL |
| `front_sdl.c` | SDL window/context, drawable size, input events, frame presentation and screenshots |
| `audio_sdl.h` | Bounded streaming AudioTrack sink for the original native mixer |
| `linux_accel.h` | Read Linux IIO sensors and apply scale, offset and mounting matrix |
| `texture_ktx.h`, `texture_hd.h` | KTX validation, texture identification, decoding and optional HD replacement |
| `solver_init_fix.py` | Guarded correction of the original Bullet constraint-row initialization call |
| `touch_reanimated_patch.py` | Guarded original tap-tolerance and interaction-radius adjustments |
| `shot_hit_trace.py` | Opt-in traces of genuine raycasts, impacts and fuse callbacks |
| `build.sh` | Native i686 object compilation and linking |

`source/tests/` contains asset-free regression checks. `source/packaging/`
contains distribution launchers. `.github/workflows/` stays at the repository
root; checkout paths now explicitly begin with `source/`.

## 3. Translation and ABI

A generated function has this convention:

```c
u64 f_XXXXXXXX(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp);
```

The low/high halves of its return value represent ARM `r0/r1`. Arguments beyond
the first four words live on a guest stack. Generated locals represent other
integer registers and condition flags. Memory accesses use 32-bit guest
addresses, and the native process deliberately uses 32-bit pointers so guest
pointers can address mapped host memory directly. Compiling the same C as a
normal 64-bit program would violate this assumption.

PC-relative addresses are based on `g_base + original_virtual_address`.
Direct branches to known functions become C calls. Indirect calls use a
function dispatch table or an import-stub table. Unsupported routines remain
explicit stubs; a successful build does not establish that every possible
code path is supported.

VFP registers and FPSCR are **thread-local architectural state**, shared across
translated calls on that guest thread. Each function must not get a fresh
zeroed float register array. Existing translated push/pop instructions preserve
callee-saved registers. This distinction fixed the raycast bug: the optimized
`btRayAabb2` helper received the maximum ray length in `s0`, which the old
per-function array silently replaced with zero.

The release compiler flags are:

```text
-m32 -O1 -msse2 -mfpmath=sse -ffp-contract=off
```

SSE math and disabled contraction matter when comparing native results with
ARM. An x87 build can produce different intermediate rounding. `DS_DEBUG=1`
adds debug symbols and frame pointers; the installed small release was built
with `DS_DEBUG=0`. Linking uses `-m32 -no-pie` and the host's math, pthread,
dynamic-loader and 32-bit SDL2 libraries.

For the verified donor, generation reported 16,387 lifted functions, 58
unlifted stubs, 390 imports and 75 C chunks, about 72 MB of generated C.
Counts describe that generator/donor revision, not a universal ARM translator.

## 4. Runtime startup and platform services

`host.c` maps ELF `PT_LOAD` segments into writable native memory, zero-fills
remaining mapped storage, resolves supported ARM relocations and records
constructors and exported Java native entry points. It builds fake JNIEnv and
JavaVM tables, runs the original constructors and calls `JNI_OnLoad`.

The host creates Java activity/asset-manager/renderer handles, opens the SDL
window and invokes the original activity and renderer lifecycle. The normal
path includes create, audio initialization, surface creation/size, resume,
frame drawing and pause before teardown. The Android Java activity itself is
not executed, so required Java-side behavior must be recreated explicitly.

The JNI layer is not a complete Java VM. It implements the methods this game
uses. Handles refer to stable object-table entries; JNI exception state is
thread-local. Argument packing honors ARM word alignment for 64-bit values.
Asset streams have independent positions and lifetimes. Read/skip and array
bounds are checked, and asset directory lists use deterministic ordering.

Native file shims translate host structures into the layouts the ARM game
expects. Host `struct stat` or `struct dirent` pointers cannot simply be passed
to guest code. Guest pthread objects also have different sizes, so an
address-to-native-object table supplies mutexes, conditions and semaphores.

## 5. Graphics, 768p and input

The graphics bridge targets desktop OpenGL's compatibility pipeline for the
GLES 1.x fixed-function calls used by the original game. Generated shims unpack
register/stack arguments; handwritten shims handle fixed-point conversion,
ETC1 decoding, extension mappings and diagnostic/projection adjustments.

The touchscreen launcher requests **1366×768** on the tested device. Fullscreen
uses the actual SDL drawable. The Java display-size delegate reports that same
drawable from startup, and the surface callback and touch conversion use it too.
This removes the old initial 1280×720 versus 1366×768 mismatch. The 256×256
viewport appearing in traces is an offscreen pass, not the main display.

SDL finger IDs map to stable guest contact indices. Coordinates are scaled to
the drawable; synthetic touch-generated mouse events are suppressed. Focus
loss releases contacts. The default Linux identity is `Generic/LinuxTouch`,
avoiding unintended Xperia Play controller mode.

`DS_REANIMATED_TOUCH=1` scales verified original tap tolerances and the HUD
interaction radius by drawable height / 480, bounded to 1–3. It is an independent
implementation informed by Reanimated's behavior. The default far-plane setting
`DS_RENDER_MIN_FAR=1200` remains a separate projection adjustment.

The original solver also contains a bad call at `0x26325c`: a 136-byte constraint
row is passed to `EA::StdC::Memchr` rather than cleared. The new generation patch
changes only this verified call to `memset`. It checks the donor SHA1 and exact
ARM instruction sequence and fails on mismatches. Global `Memchr` semantics
remain intact. This is the same specific correction identified in Reanimated;
it addresses stale physics constraint data rather than hiding large triangles.

## 6. Audio

The skipped Java `DeadSpaceActivity` normally calls EAAudioCore startup after
`MainActivity.onCreate`. The native host now reproduces that step by calling the
original exported `AndroidEAAudioCore.Init` with an AudioTrack-compatible object,
4096-frame buffer, two channels and 48000 Hz.

The original engine decodes and mixes the actual game sounds. Its AudioTrack
`play/stop/write` calls reach SDL as signed 16-bit PCM. Writes block when the
4096-frame software queue is full, pacing the producer and bounding memory.
Stop/close interrupts blocked writes. JNI validates short-array offset/count
before reading PCM. SDL converts to the system hardware format if necessary.

`[AUDIO_OPEN]`, `[AUDIO_PCM]` and `[AUDIO_CLOSE]` distinguish device creation,
first nonzero samples and sample totals. `DS_AUDIO=0` is a diagnostic disable.
This supplies original-game audio; it does not claim all Reanimated-specific
voice-allocation or reload-timing changes.

## 7. Motion

The Linux bridge finds readable IIO accelerometers, preferring the display/lid
sensor of a convertible. It applies `(raw + offset) * scale`, then the kernel or
udev mounting matrix, and delivers m/s² through the original exported
`NativeOnAcceleration(FFF)` callback. The game performs gravity normalization
and gesture interpretation.

The game's enable requests and Android delay categories control polling.
`DS_ACCEL_DEVICE` and `DS_ACCEL_MATRIX` are optional explicit overrides;
`DS_ACCEL_DIAG=1` records discovery, readings and delivery. No fabricated
sensor readings or system calibration changes are used.

## 8. Saves and HD textures

The Linux launcher places saves under
`${XDG_DATA_HOME:-$HOME/.local/share}/dead-space-mobile/var/` by default.
`DS_DATA` overrides the parent directory. Read-only game resources continue to
use `DS_ASSETS`. Logs use the XDG state directory, overridable with `DS_LOG_DIR`.
The launcher verifies file creation and rename in the save directory and copies
legacy `game/assets/var` files without overwriting existing saves. Original
files remain available as a recovery copy. Crucially, the JNI AssetManager
excludes legacy `var` folders: otherwise the engine discovers old saves as
immutable APK resources, shadows the writable files and stops updating them. Normal pause/close must deliver the
original lifecycle event so queued state can be flushed before process exit.

Reanimated v1.3's 222 KTX files are named by a 64-bit FNV-1a key over little-endian
width, height, format and the original level-zero upload bytes. The optional
HD bridge computes that identity at the GLES boundary and uploads the matching
KTX mip chain. ETC1 and ATC formats are decoded to ordinary RGBA for desktop
OpenGL. File bounds, dimensions, mip lengths and formats are validated before
replacement. Missing or rejected replacements retain original textures.

A subimage update to a replaced texture restores its original-size image and
marks it dynamic, preserving the game's update coordinates. Texture deletion
releases the saved original bytes. `DS_HD_TEXTURES` selects the extracted pack;
`DS_HD_TEXTURES=0` disables it and `DS_HD_DIAG=1` logs lookups. Supplying a
Reanimated APK to the launcher can add its HD pack to an existing installation.
The pack and Android runtime are not distributed in this repository.

## 9. Reproduce a Linux release

On Debian-style x86-64 systems, install a 32-bit compiler/runtime and SDL2
headers. Package installation requires the machine owner's administrator access:

```bash
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install gcc-multilib libc6-dev-i386 python3-venv unzip \
  libsdl2-dev:i386 libsdl2-2.0-0:i386 libgl1:i386 libgl1-mesa-dri:i386
```

Some multiarch setups conflict over development packages. The existing WSL build
helper shows how to extract `libsdl2-dev:i386` headers into a local prefix and
link `/usr/lib/i386-linux-gnu/libSDL2-2.0.so.0` directly. This project's local
machine build used locally extracted development packages for the same reason.

From the repository root, using the Fixes donor as the example:

```bash
repo="$PWD"
python3 -m venv "$repo/build/venv"
"$repo/build/venv/bin/pip" install pyelftools
mkdir -p "$repo/build/game"
unzip -qn /path/to/original.apk 'lib/armeabi/libDeadSpace.so' 'assets/*' \
  -d "$repo/build/game"
export DS_SO="$repo/build/game/lib/armeabi/libDeadSpace.so"
sha1sum "$DS_SO"   # must match the verified SHA1 above
cd "$repo/source/experimental/lifted_milestone6/tools"
DS_TEXTUREPACK_DIAG=0 "$repo/build/venv/bin/python" gen_all.py "$repo/build/generated"
cd "$repo/build/generated"
DS_BUILD_JOBS=2 DS_DEBUG=0 SDL_CFLAGS="$(sdl2-config --cflags)" \
  SDL_LIBS="$(sdl2-config --libs)" sh build.sh
mkdir -p "$repo/build/package"
cp dspc_sdl "$repo/build/package/"
cp "$repo/source/packaging/linux-touchscreen/"* "$repo/build/package/"
chmod +x "$repo/build/package/Play-Dead-Space-Touch.sh"
"$repo/build/package/Play-Dead-Space-Touch.sh" /path/to/original.apk
```

For Reanimated, extract `assets/game/libDeadSpace32.so` into the same local
`lib/armeabi/libDeadSpace.so` location for generation, or point `DS_SO` at its
actual extracted path. The distribution launcher handles both donor layouts.
Use the matching 32-bit SDL installation; a 64-bit `sdl2-config` library search
path can cause linker errors. No Unicorn dependency is needed for this build.

`gen_all.py` deletes and recreates its output directory. Use a dedicated build
directory, never the source tree or an asset/save directory. `build.sh` caches
chunk objects by source timestamps; clear stale `.o` files if compiler flags
change. Two compiler jobs are a reasonable starting point on the tested 4 GB
machine. HD textures and extracted assets need additional disk/GPU memory;
keep them on a suitable data drive when root storage is constrained.

## 10. Testing and evidence

Run from repository root with the compiler dependencies above:

```bash
build/venv/bin/python source/tests/test_vfp_call_state.py
python3 source/tests/test_linux_accel.py
python3 source/tests/test_solver_init_fix.py
gcc -O1 -Isource/experimental/lifted_milestone6/tools source/tests/test_texture_ktx.c -o /tmp/ds-ktx-test
/tmp/ds-ktx-test
python3 source/tests/check_m6_host_c_syntax.py
CFLAGS=-m32 python3 source/tests/test_audio_sdl.py
python3 -m unittest discover -s source/tests -p 'test_*probe.py'
```

The host check exercises the actual JNI worker/stream code using synthetic
inputs. Audio tests use SDL's dummy driver to test queue limits and blocked-write
cancellation. Neither substitutes for real gameplay. Original-asset runs should
check the main viewport, native hit callbacks, audio sample delivery, save writes
and reload, and actual HD replacement messages. Rendering screenshots alone do
not prove physics correctness or persistent saves.

Previous confirmed results: captured native ray callback words matched original
ARM after the VFP fix; the user confirmed hit detection, opening skybox and audio.
Real IIO values reached the original game's accelerometer handler. Later
motion-gated gameplay remains unverified. Subsequent reports of missing saves
and exploding vertices are tracked as distinct issues; earlier successful
checks must not be interpreted as a complete-game certification.

Useful diagnostics: `DS_AIM_DIAG`, `DS_ASSET_TRACE`, `DS_ACCEL_DIAG`,
`DS_HD_DIAG`, `DS_SHOT=frame:/absolute/file.ppm`, and `DS_WATCHDOG=seconds`.
Keep private game-derived files and saves out of commits. Record compiler flags,
donor hash, source revision and runtime logs alongside local test results.

## References and attribution

- [Reanimated by GINESTR0](https://github.com/GINESTR0/DeadSpaceReanimated)
- [KTX 1.0 specification](https://registry.khronos.org/KTX/specs/1.0/ktxspec.v1.html)
- [SDL queued audio](https://wiki.libsdl.org/SDL2/SDL_QueueAudio)
- [SDL audio device format negotiation](https://wiki.libsdl.org/SDL2/SDL_OpenAudioDevice)
- ATC color interpolation is adapted from AssetStudio's MIT-licensed decoder;
  its license is retained with the runtime source.

## Latest local validation, 2026-11-08

- The donor-specific solver patch passed its exact ARM instruction guard. A
  poisoned-memory regression verified that exactly 136 bytes are cleared,
  adjacent bytes are preserved, and unrelated Memchr calls are unchanged.
- All 222 supplied Reanimated v1.3 KTX files passed the parser: 116 ETC1 and
  106 ATC interpolated-alpha files. ASan/UBSan fixture checks covered malformed
  dimensions/truncation and ATC color/alpha decoding.
- An offscreen 6000-frame original-game run loaded 69 HD replacements, including
  1024-to-4096 UI textures. It created settings, achievements, level and
  checkpoint files in an initially empty separate data directory.
- A second original-game run reopened the new settings, achievements and level
  save through native file I/O. All four saved-file SHA256 hashes remained
  unchanged. Both runs delivered resume/pause and exited normally.
- The real JNI fixture verifies legacy save files cannot be opened/listed as
  APK assets while native filesystem writes to those files still work.
- VFP, IIO and existing host/JNI worker regressions pass after relocation.

The exact previously flashing combat scene has not yet been replayed by the
user on this revision. The solver correction and HD delivery are verified;
this is not a claim of a full playthrough or every Reanimated runtime feature.

The relocated historical Object3D diagnostic test had drifted from its tool:
it expected an obsolete trapping/report API, while the current tool deliberately
logs and retains the original fault behavior. Its tests now exercise the actual
read-only/instrument/backup interface and preservation of the native call.
All nine generator-probe regressions pass under the relocated source layout.

## Packaging the tested executable

The release launchers now run the binary beside them. The Windows `.cmd` /
PowerShell pair uses WSLg and no longer fetches the old baseline source branch.
Native Linux is the tested target; Windows/WSLg is an optional unverified path.
The tested prebuilt binary imports GLIBC_2.38 symbols, so the release requires
32-bit glibc 2.38+ even though rebuilding on an older toolchain may differ.

From the repository root:

```bash
python3 source/scripts/package-linux-release.py \
  --binary /path/to/tested/dspc_sdl \
  --runtime-commit fd72452253a86e74f0c5c929026090658f3f5586 \
  --version 2026.11.08 --output /path/to/release-staging
```

The script accepts only an ELF32 x86 binary, copies an explicit source/document
allowlist, records runtime and package source revisions, preserves executable
modes in a ZIP, checks its integrity, and creates internal/external SHA256 files
plus release notes and publishing instructions. It does not create a GitHub
release or bundle the donor APK, game library, HD textures, saves, or logs.
The user confirmed saves survived reopening and HD textures look sharp.

## Extended gameplay verification — user report, 2026-11-09

The user reports **approximately two hours of hands-on gameplay on the finalized Linux build**. This establishes a **user-verified playable** status for that build, beyond automated frame progression and startup checks. It is a user-reported session, not an independently instrumented two-hour CI run. The report does not establish campaign completion, exhaustive interaction coverage, universal hardware compatibility, or native Windows validation. Earlier notes about specific scenes not yet reproduced remain scoped to those scenes; they do not negate the extended gameplay report.
