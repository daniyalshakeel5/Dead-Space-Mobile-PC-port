# Native runtime status — November 5, 2026

The startup model-corruption race has been identified and repaired: JNI object monitors were no-ops, allowing font and model reads to overwrite the same shared Java byte array. The fixed Windows build completed 18,000 frames, reached the original 3D corridor/movement tutorial, and produced zero mismatches across 26,466 checked read/copy transfers. Linux also passed 18,000 frames without model-object trace wrappers, reached the same corridor/tutorial, and had zero mismatches across 26,466 checked transfers. Analog movement and camera look now work in real-game Windows and Linux tests using SDL virtual controller input. Physical PS4 hardware, combat actions, audio and hardware GPU behavior still require validation.

## Windows compile verified

Commit `63c6552afdf97887a7b8ac55035f65e5d0026501` on `work/deadspace-portable-runtime-2026-11-05` compiles the actual released APK's original library into a native Intel 80386 PE32 executable. Run: https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42043306457 . All 82 objects linked successfully. This is a build result, not a Windows boot result.

The previous run failed because the merged host defined `imp_readdir_r` twice and the JNI stream regression helper used POSIX `mkdtemp`. The fix removes the duplicate Windows directory shim in favor of the shared Bionic adapter and creates temporary directories with Win32 APIs on Windows. Linux retains `mkdtemp`.

Default Windows builds retain the original model decoder, without empty-model recovery or automatic replacement of the buffered reader.

## Actual Windows boot result

Run https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42098354191 tested the unchanged executable from run 42046961978 on Windows Server 2022 with the original APK assets and x86 Mesa llvmpipe 26.2.6. It completed all 208 initializers, returned JNI version 0x10004, created a 1280x720 OpenGL window, and rendered the EA splash at frame 30 (visually inspected). It did not complete the requested 3,600 frames or reach difficulty selection.

The process exited with -1073741819 / 0xc0000005, a null read. Runtime PC 0x014a9eda minus module base 0x00e50000 gives RVA 0x00659eda; adding the executable's preferred base 0x00400000 gives 0x00a59eda. GNU addr2line resolves that address to `f_00368efc`, `fn_060.c:47397` (the previously identified m3g::Object3D::duplicate routine). Logs show exploder.m3g reads and an implausible 1,294,810,446-byte skip clamped to EOF before the crash. This establishes the Windows failure site, not the original source of the invalid model state.

Evidence artifact: https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42098354191/artifacts/13449215243 . The first attempt never launched the game because its graphics setup selected an unsuitable Mesa package. The successful setup uses mmozeiko/build-mesa's dedicated mesa-llvmpipe-x86-26.2.6.7z package.

## Repeatability check

Run https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42098800665 at `a28f634` completed 3,600 frames with exit code 0. Its 120 captured frames were sampled visually: EA splash, difficulty selection, and IronMonkey Studios intro text; no corridor gameplay was verified. The log still contains a 288,957,320-byte skip while reading lurker.m3g, so successful process completion does not establish correct model decoding.

Run https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42100410002 reused that exact executable and requested 18,000 frames. It crashed during slasher.m3g loading with 0xc0000005 before the extended test. Stability remains unresolved.

Commit `10aa42e` adds read-only instrumentation around the original loadObject and BufferedInputStream::setPosition functions, recording cursor, cache origin/count and upcoming bytes. It does not replace the parser or skip invalid objects.

## Genuine-game Linux evidence

The baseline workflow verifies original library SHA-1 `64a3323cd3cc0c483a645b62af8508624163ba9b` and runs the original assets with SDL/OpenGL under llvmpipe.

- Earlier run 41899586522 completed 3,600 frames in its current-source job. Images showed hints and difficulty selection; no corridor gameplay was verified. Later runs still crashed, so this does not establish a stable fix.
- Run 41997231888 showed that serializing the M3G loader alone does not fix the crash. Asset tracing alone also failed.
- Debugger run 41998017860 stopped in `f_00325b04` (EASTL byte-vector insertion), called by `f_00326948` (Loader::loadObject3D), with a requested length of 303,117,103 bytes and a null allocation destination. The cause of that length is not yet established.
- A/B run 42043485140 at `7e65ae9` compared original and recovered buffered readers, both with the original model decoder and identical loader serialization. Both crashed at the object-reference table access in `f_00326948`. The original-reader run failed while loading slasher.m3g; the recovered-reader run reached lurker/lurker_projectile before failing while loading exploder.m3g. This does not justify enabling the replacement reader as a fix.

Further captures 42044380174 and 42045533809 still failed. The latter identifies the caller stream as DataInputStream (vtable read target `g_base + 0x324884`) wrapping another stream at offset +4. Before the slasher crash, Java received skip requests of -123,602,854 and +1,294,810,446 bytes. Those values are already implausible; simply permitting backward skips is not an established fix. The current capture does not dump the nested buffered stream's cursor/cache, which remains a useful next target. No reference clamping, fabricated models, or skipped decoder failures were introduced.

## Current next work

1. Verify analog controller movement and simultaneous camera look in the real game, then tune walking/running and reversal.
2. Test DualShock 4 USB/Bluetooth hardware, combat actions, audio and hardware GPU performance.
3. Continue beyond the first corridor and validate saves and level transitions.

The startup investigation below is historical; its shared-buffer cause and verified repair are recorded at the end.

## November 6: corridor reached on Windows

Run https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42179490573 reused the instrumented executable from 42099710126 in three independent Windows jobs. All three completed 18,000 frames with exit code zero. Frame f18000.png from attempt 2 was inspected: textured original corridor, player character and MOVEMENT CONTROLS tutorial are visible. This verifies corridor rendering, not keyboard/controller gameplay or a root-cause crash fix.

A remaining data discrepancy is now localized: slasher.m3g object at position 85328 has length 1129 and correctly ends at 86462 in both traces. In the earlier clean capture, bytes at 86462 begin `02 18 00 00 00`; in extended attempt 2 they begin `39 11 12 39 2f`. Cache origin and cursor agree (86016 + 446), so the error involves buffer contents across a refill rather than this object's length arithmetic. The latter run makes an invalid seek to 792355796 yet reaches the corridor. Do not treat clean exit as proof that all assets decoded correctly.

The next diagnostic extends actual stream-position logging and adds read/copy checksums to distinguish wrong file reads from JNI transfer corruption. It changes observation only.

## Verified shared-buffer cause and repair

Before the monitor fix, run https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42180292668 captured two read/copy checksum mismatches. Font loading (thread 1) and model loading (thread 8) wrote to the same Java byte array, handle 0x10040, data pointer 0x1e058560. The slasher read at end position 774144 had checksum f8395449, but its immediately following 4096-byte native copy had checksum 4f02868e. A font read/copy also differed. This directly identifies corruption between file read and JNI copy, rather than malformed original assets or object-length arithmetic.

Commit `01fec5a` replaces the no-op MonitorEnter/MonitorExit bindings with per-object recursive pthread mutexes. Allocation/publication is protected by the VM object lock, which is released before blocking on a monitor. Invalid handles, unowned exits and lock failures report JNI failure. `e3cf941` adds the explicit errno header for Linux portability.

After the fix, run https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42181159489 completed 18,000 frames with exit code zero. Its trace confirms successful MonitorEnter/Exit on the shared array, 26,466 comparable read/copy pairs with zero checksum differences, and no enormous skip requests. Frame f18000.png was visually inspected and shows the original textured corridor, player and movement tutorial. Evidence: https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42181159489/artifacts/13486410990 .

Linux run https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42182135271 passed 18,000 frames with exit code zero, zero model-object trace entries, successful JNI monitor calls, and zero mismatches across 26,466 read/copy checksum comparisons. The final frame f18000.png was visually inspected and shows the original corridor/player/movement tutorial. No enormous skip requests were present. Evidence: https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42182135271/artifacts/13486705060 . Windows run 42182135216 is an additional repeat of the same monitor implementation with the explicit portability header.


## November 6: input validation and controller implementation

Windows run 42225547379 completed two 24,000-frame tests with touch IDs 0 and 1. Both had exit code zero and zero mismatches across 26,466 comparable read/copy transfers. Visual inspection shows that the immediate fixed-offset WASD drag still did not move the player, even with touch-down separated by a frame. Captured mouse look did turn the camera. Linux run 42226169499 also completed 24,000 frames with the same clean checksums and the same movement failure. These successful process results are not movement passes.

Earlier Windows run 42184709979 provided a working reference: a gradual ordinary mouse drag moved the player through the corridor to the look-controls tutorial. The following experiments therefore send gradual swipes. Run 42227866086 compares swipe rates before controller integration.

Commit 5151cd6 adds normalized SDL controller polling, proportional left-stick movement, right-stick look independent of mouse capture, a radial dead zone, and device/focus release handling. Windows run 42228747637 and Linux run 42228867541 exercise it through an SDL virtual controller against the original game; screenshots have now been reviewed (results below). Physical PS4 hardware has not been tested. See PC_CONTROLLER_CONTROLS.md for mappings and limits.


## Verified analog movement and camera look

Windows run https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42228747637 passed both controller touch-ID variants. Default touch ID 1 evidence (artifact 13508695582) was downloaded and visually inspected: frame 18000 is the starting corridor; 18900 shows the player farther down the corridor; 19500 shows the first door and LOOK CONTROLS tutorial. Frames 20250 and 20700 show camera turning during overlapping left/right-stick input. The run completed 24,000 frames with exit code zero and zero mismatches across 26,466 comparable read/copy transfers.

Linux run https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42228867541 used the same controller implementation and default touch ID 1. Artifact 13506264905 shows the same corridor progression and camera response, also with 24,000 completed frames, exit code zero, and zero mismatches across 26,466 transfers. These tests use the real original game and assets with SDL's virtual-controller input device; they do not verify physical DualShock 4 connectivity.

The movement-only screenshot gate (frames 18000 to 19500, background crop excluding the tutorial/player) measured mean absolute differences of 49.339916 on Windows and 48.952775 on Linux. The previous failed fixed-offset movement measured 0.000010 and is rejected by the same check. Visual inspection establishes that the change is actual corridor traversal, rather than just an animated overlay. Both gradual keyboard swipe rates also passed in run 42227866086.

Keep movement touch ID 1 as default, ordinary mouse touches as ID 0, and camera touches as ID 2. Walk/run tuning, sustained movement/reversal, combat, level transitions and physical controller testing remain open. This is an early gameplay milestone, not a complete campaign stability claim.
