# Native subsystem reuse from the Dead Space Vita loader

Source: [v-atamanenko/deadspace-vita](https://github.com/v-atamanenko/deadspace-vita), `loader/main.c` at `3eaa3c4`, MIT license. Original attribution: Andy Nguyen, Rinnegatamante, Volodymyr Atamanenko. Original upstream MIT copyright notice and license remain upstream; this independent native adaptation credits the authors.

The Vita project's **platform-independent lifecycle order** is reused in `NativeLifecycle`: JNI init -> audio startup -> activity created -> surface created -> keyboard visibility (module 600, hidden 1) -> first draw; initialize graphics/input on second frame; poll input and present from third frame. The Vita runner also performs platform-dependent ARM shared-library loading, native Vita GL patching, hardware clocks, controls and audio output. **None of those platform-specific implementations is copied into the PC port**, and the original game logic is not in the Vita repo.

`NativeLifecycleCallbacks` requires actual native implementations for every startup/frame operation. A missing callback fails immediately. This module does not spoof JNI, translate ARM, or invent game initialization. It is a reusable callback-order coordinator, not proof of full game boot, sound playback or 3D rendering. On shutdown, graphics and audio are stopped only if they were successfully initialized.

Other candidates:
- `loader/android/java.io.InputStream.c` has unsafe globals, an incomplete `skip` result and Vita-specific paths. Use our existing secure portable asset installer/resource-path adapter instead of copying it.
- `loader/android/EAAudioCore.c` uses Vita audio APIs and stubbed `write/play/stop`; do **not** copy those as working audio.
- `loader/reimpl/controls.c` uses Vita polling and touchscreen coordinates; use original event IDs and mapping research, adapting the native SDL2 input system, not Vita device bindings.

The original MIT license requires preservation of copyright and permission notices when distributing substantial reused source. The code here is a fresh platform-neutral translation of the observable lifecycle ordering rather than a verbatim copy of the entire file.
