# Dead Space Mobile lifted x86 — exact TexturePack null-vector fault handoff

## Proven on user's WSL build (2026-11-03)

- Original guest ELF: `libDeadSpace.so`, 6,672,749 bytes, SHA-256 `2fc87e9a5afa1028d91a9770d23ffd04b45fb5dbc8d3800ed6b31e34223a490b`.
- Engine start passed 208/208 constructors, `JNI_OnLoad`, NativeOnCreate, NativeOnSurfaceCreated and NativeOnSurfaceChanged.
- Application startup called `LayerBackground::LayerBackground(char const*)` -> `im::TexturePack::load(eastl::basic_string<wchar_t> const&)` at `f_0035c844` -> `im::TexturePack::load(vector<intrusive_ptr<m3g::Object3D>> const&)` at `f_0035b2e0`.
- WSL SIGSEGV PC `0x08627456` lies inside `f_0035b2e0+0x552`, source `fn_059.c:41995`; x86 instruction `push DWORD PTR [edi]`, register `EDI=0`.
- The lifted ARM code at label `L_35b564` loads `r3 = LD32(r10)` (the resource vector's begin pointer). Soon afterward it executes `r1 = LD32(r3)` before calling `f_00368338`. A zero begin pointer thus causes the crash **before `f_00368338` enters**. No evidence establishes that `f_00368338` itself is buggy.
- The JNI boot trace reports an attempted `assets/data.zip` open returning FileNotFoundException followed by asset-directory listing. The APK may legitimately use the directory listing fallback. **Do not assert that the absent data.zip caused the crash.**
- The user replaced assets with Xperia Play assets at least once. An Android donor supplying `libEAMGameDeadSpace.so` is a **different executable/profile** from this lifted 6.67MB `libDeadSpace.so`; neither source can safely substitute for the other's matching resources.
- A full rebuild with debug symbols still crashed at the same callsite, so blindly regenerating lifted C is not a demonstrated fix.

## Implementation in this branch

`tools/texturepack_fault_guard.py` is a **fail-closed diagnostic**, not a playable-game fix.

It matches the exact generated C for `f_0035b2e0` and, when explicitly instrumented, logs the input vector's start/end/capacity plus the original function arguments before the null dereference. It additionally traces the vector **at the point `f_00365a2c` creates it** inside `f_0035c844`. This provides a definitive two-point comparison:

| Source vector after decoding | At crash in TexturePack::load(vector) | What to investigate |
|---|---|---|
| begin = end = 0 | begin = 0 | Source string resolved to zero Object3D objects; check exact pack resource and matching asset profile |
| begin != 0 | begin = 0 | Vector mutated or corrupted inside TexturePack loading; inspect original translated call boundaries, allocator, and destructor paths |
| begin = 0, end != 0 | begin = 0 | Invalid vector invariants; investigate memory corruption, ARM translation, and lifetime management |

The script keeps a backup before instrumenting and refuses to patch unfamiliar generated structures. It **does not** return fake textures, skip guest functions, mask a null pointer, modify any EA game files, or claim a successful load. Pure-synthetic guard matching/idempotence tests pass in GitHub Actions.

## What the next live game test should do (other agent)

Integrate `texturepack_fault_guard.py --instrument` automatically **after the original `gen_all.py` translation and before compiling `fn_059.c`**, as part of the user's existing VS Code F5/one-click path, not as terminal steps to the user. The generated `fn_059.c` is private/copyright-sensitive: do not upload it to the public GitHub repository.

Capture the output of `[TEXPACK_SOURCE_VECTOR]` and `[FATAL:TEXPACK_NULL_VECTOR]`; also log the asset tree's relative filenames and any missing `fopen/open` requests around LayerBackground creation. Use those observations to repair the correct resource path or mismatched original asset set. If the vector is only cleared after decoding, investigate translated C object ownership instead. Run the same original 6.67MB `libDeadSpace.so` donor rather than switching to the Xperia `libEAMGameDeadSpace.so` port.

**Current boundary:** The fault is localized and a safe two-point instrumentation tool is committed/tested. Neither the root cause nor a working game launch has been verified for this WSL setup.
