# Native runtime status — November 5, 2026

The Windows diagnostic executable has now independently rendered the original 3D corridor and movement tutorial. Stability and controls are not yet fully verified. Full native Windows compilation passes. One Windows run completed 3,600 frames through difficulty selection and the developer intro; a repeat of that same executable crashed during model loading. Linux model loading also remains unstable. Keyboard/mouse gameplay, PS4 sticks, audio and hardware GPU behavior still require runtime validation.

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

## Next work

1. Inspect the stream/vtable capture and trace the first divergence from genuine model bytes.
2. Fix the proven source of incorrect reads and rerun first boot through difficulty, intro and corridor. Verify screenshots, not just frame counts.
3. Run the native Windows executable, then validate keyboard/mouse and the Xperia controller path with a physical PS4 controller.

The archived agent's corridor result remains reported evidence until reproduced. Keep that distinction in subsequent handoffs.

## November 6: corridor reached on Windows

Run https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42179490573 reused the instrumented executable from 42099710126 in three independent Windows jobs. All three completed 18,000 frames with exit code zero. Frame f18000.png from attempt 2 was inspected: textured original corridor, player character and MOVEMENT CONTROLS tutorial are visible. This verifies corridor rendering, not keyboard/controller gameplay or a root-cause crash fix.

A remaining data discrepancy is now localized: slasher.m3g object at position 85328 has length 1129 and correctly ends at 86462 in both traces. In the earlier clean capture, bytes at 86462 begin `02 18 00 00 00`; in extended attempt 2 they begin `39 11 12 39 2f`. Cache origin and cursor agree (86016 + 446), so the error involves buffer contents across a refill rather than this object's length arithmetic. The latter run makes an invalid seek to 792355796 yet reaches the corridor. Do not treat clean exit as proof that all assets decoded correctly.

The next diagnostic extends actual stream-position logging and adds read/copy checksums to distinguish wrong file reads from JNI transfer corruption. It changes observation only.
