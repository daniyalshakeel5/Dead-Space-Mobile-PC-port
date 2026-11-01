# Dead Space Mobile — Native engine initialization status
Updated 2026-11-01. This is engineering status, not a playable build.

## Ground-truth original ARM control flow recovered

On the user-confirmed APK, original `NativeOnDrawFrame` dispatches a
stage-zero context at runtime state global `0x0049CAA8` + 8 to
`0x0027F870`. It calls **`0x0027F790`**, then tail-branches to
**`0x00280520`**, and only after the full sequence returns does
the frame dispatcher publish stage **1**.

- `src/original_startup_first.cpp` translates first routine
  `0x27F790` including nested `0x27F124`, registry query
  `0x27E930`, seven original virtual dispatch sites and the
  `0x2713B8`/`0xF895C` wrappers.
- `src/original_stage_zero_allocating.cpp` translates
  `0x280520` through the original record allocation,
  `0x28CA24` type-two setup, clock and three-word followup.
- `src/original_startup_second_tail.cpp` translates
  remaining `0x280580..0x2806A0` including original resource
  fallback, three more allocation paths (sizes `0x34`, `0x24`,
  `0x58`), object setup and final virtual calls.
- `src/original_startup_object34.cpp` implements complete
  `0x287A94` (52-byte constructor).
- `src/original_startup_object58.cpp` implements complete
  `0x280A34` (88-byte constructor). Three successive
  `0x27E560` calls pass previous returned r0, not original object.
- `src/original_static_singleton.cpp` reconstructs guarded
  `0x27E560` statically allocated object. Original APK verified
  guard `0x0049C540`, object `0x0049C544`, GOT entries
  `0x0049096C` (source), `0x00490AE4` (table),
  `0x00490A54` (destructor), `0x00490840` (DSO).
  Bionic imports are `__cxa_guard_acquire`,
  `__cxa_guard_release`, `__aeabi_atexit`.
  A verified GOT binder is available, but destructor handling
  still depends on an original-compatible native implementation.
- `src/original_engine_startup.cpp` runs the two recovered
  routines and tail in original order; it **refuses to write
  frame stage 1** when the tail/any required original service
  is unavailable.

## Test result versus a real boot

Cross-platform CI at
https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41320064349
passed **all four** Windows and Linux core/SDL2 jobs. The
startup-tail integration tests use controlled, synthetic callbacks
for unresolved original engine methods. Tests establish the
control-flow reconstruction and native memory writes, **not**
a successful original-engine boot.

`deadspace_native` can import a private user-owned APK and
check its asset layout, but still exits explicitly with engine
initialization incomplete. No ARM emulator or compatibility game
runtime is introduced.

## Real engine initialization blockers (not to be faked)

1. **Registry implementation**: original `0x27E930` virtual
   method +`0x18` and the service objects/vtables constructed
   before stage-zero code can execute on genuine game state.
2. **Allocator**: original pool and object allocator virtual
   method +`0x0C` across `0x281D54`, `0x2849E0`, and
   requests for `0x34` and `0x58` objects. Original pointer
   identity, descriptor semantics, lifetime and alignment must be
   recovered before implementing host-native allocations.
3. **Additional original service virtuals**: first-stage slots
   +`0x18`, +`0x1C4`, +`0x84`, +`0x8C`, +`0x238`
   plus the nested `0x2068A8` and final system setup methods.
4. **Second-stage direct services**:
   `0x27FD7C`, `0x2803E4`, `0x36FD38` and the
   specific constructed-object virtual slots.
5. Only after the above are genuinely backed by native
   reconstructed behavior can stage zero return and the
   original frame-state word transition to **1**.
   The active-frame update, world simulation, renderer,
   loading and player control still need integration after boot.

The Vita port is helpful as ABI/context research, but its
ARMv6 loader, Android API translation and hardcoded addresses
are not used for this x86-64 port; the APK editions are not
binary identical. Do not ship original game files.

## Next concrete work

Resolve the **actual native virtual method implementations** rather
than adding further synthetic setup callbacks. Ground their
vtable targets in the original APK's statically initialized objects
and Ghidra call/dependency graphs, then integrate verified
method bodies with the startup coordinator. Keep using GitHub
runners for heavy analysis, not the assistant local VM.
