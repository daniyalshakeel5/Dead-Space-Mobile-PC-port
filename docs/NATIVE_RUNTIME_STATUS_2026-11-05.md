# Native runtime status — November 5, 2026

The game is not yet independently verified as playable. Full native Windows compilation now passes; Linux model loading remains unstable. Keyboard/mouse gameplay, PS4 sticks, audio and hardware GPU behavior still require runtime validation.

## Windows compile verified

Commit `63c6552afdf97887a7b8ac55035f65e5d0026501` on `work/deadspace-portable-runtime-2026-11-05` compiles the actual released APK's original library into a native Intel 80386 PE32 executable. Run: https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42043306457 . All 82 objects linked successfully. This is a build result, not a Windows boot result.

The previous run failed because the merged host defined `imp_readdir_r` twice and the JNI stream regression helper used POSIX `mkdtemp`. The fix removes the duplicate Windows directory shim in favor of the shared Bionic adapter and creates temporary directories with Win32 APIs on Windows. Linux retains `mkdtemp`.

Default Windows builds retain the original model decoder, without empty-model recovery or automatic replacement of the buffered reader.

## Genuine-game Linux evidence

The baseline workflow verifies original library SHA-1 `64a3323cd3cc0c483a645b62af8508624163ba9b` and runs the original assets with SDL/OpenGL under llvmpipe.

- Earlier run 41899586522 completed 3,600 frames in its current-source job. Images showed hints and difficulty selection; no corridor gameplay was verified. Later runs still crashed, so this does not establish a stable fix.
- Run 41997231888 showed that serializing the M3G loader alone does not fix the crash. Asset tracing alone also failed.
- Debugger run 41998017860 stopped in `f_00325b04` (EASTL byte-vector insertion), called by `f_00326948` (Loader::loadObject3D), with a requested length of 303,117,103 bytes and a null allocation destination. The cause of that length is not yet established.
- A/B run 42043485140 at `7e65ae9` compared original and recovered buffered readers, both with the original model decoder and identical loader serialization. Both crashed at the object-reference table access in `f_00326948`. The original-reader run failed while loading slasher.m3g; the recovered-reader run reached lurker/lurker_projectile before failing while loading exploder.m3g. This does not justify enabling the replacement reader as a fix.

The next capture (commit `4b551e7`, run 42044380174) records the actual stream object's vtable, loader memory, and instruction-to-C context. Its purpose is to identify the stream layer producing the bad reference. No reference clamping, fabricated models, or skipped decoder failures are introduced.

## Next work

1. Inspect the stream/vtable capture and trace the first divergence from genuine model bytes.
2. Fix the proven source of incorrect reads and rerun first boot through difficulty, intro and corridor. Verify screenshots, not just frame counts.
3. Run the native Windows executable, then validate keyboard/mouse and the Xperia controller path with a physical PS4 controller.

The archived agent's corridor result remains reported evidence until reproduced. Keep that distinction in subsequent handoffs.
