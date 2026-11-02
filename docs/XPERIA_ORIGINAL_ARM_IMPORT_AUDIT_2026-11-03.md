# Xperia Play 1.1.33 — actual original ARM ELF import audit

2026-11-03. Locally analyzed the user-supplied
`libEAMGameDeadSpace.so` (4,499,468 bytes; SHA-256
`1ff0a34a2ef35f50c0031b383d8fd7b14369dca0f41969c5d4da8fabe569ed38`).
The original proprietary library remains **private**; this file
contains only findings and symbol-family totals. No game code or
copyrighted assets have been committed.

## Native ELF facts (readelf -W)

| Item | Actual Xperia Play binary |
| --- | ---: |
| Undefined dynamic symbol entries, including one unnamed | 393 |
| GL / OpenGL ES imports (symbols beginning `gl`) | 190 |
| Pthread and semaphore imports (`pthread*` or `sem_*`) | 39 |
| Other runtime/OS/C/C++ entries (including one unnamed) | 144 |
| ARM32 `R_ARM_RELATIVE` relocations | 22,677 |
| ARM32 `R_ARM_ABS32` relocations | 890 |
| ARM32 `R_ARM_JUMP_SLOT` relocations | 294 |
| ARM32 `R_ARM_GLOB_DAT` relocations | 52 |
| Total ARM relocations | 23,913 |
| `DT_INIT_ARRAYSZ` | 836 bytes (209 4-byte slots) |
| Exported JNI_OnLoad/Java_* symbol names | 69 |

The `__cxa_pure_virtual` import has **887 separate relocation sites**,
primarily C++ vtable references. That does **not** imply the original
game invokes pure-virtual calls 887 times; it is a static
linking count, not runtime telemetry.

## Verified actual guest imports: existing native Windows coverage

The module really imports `memcpy`, `memmove`, `memset`,
`memcmp`, `strlen`, `strcmp`, `strncmp`, `malloc`,
`calloc`, `free`, `realloc`, `open`, `read`,
`lseek`, `close`, `fopen`, `fread`, `fseek`,
`ftell` and `fclose`. These 20 names are supported in our
sandboxed Windows Dynarmic guest ABI with original-free tests.

The original additionally imports `__android_log_write`,
`__android_log_print`, `__android_log_vprint`,
`gettimeofday`, `clock_gettime`, and `time`.
The first, `gettimeofday`, `clock_gettime` and `time`
now have bounded Windows-native ARM guest service implementations.
The variadic Android log functions are still **unimplemented**
to avoid returning fabricated success from an incorrect varargs ABI.

## Next critical subsystems, based on the actual ELF

1. Bionic and C++ process/ABI support, including
   `__cxa_pure_virtual`, `__cxa_guard_acquire`,
   `__cxa_guard_release`, `__aeabi_atexit`, `__errno`,
   and CPU atomics.
2. Threading: 39 pthread/semaphore functions; state,
   worker scheduling, timers and callbacks must work.
3. OpenGL ES 1.x host dispatch: 190 imported names, including fixed
   function, textures, draw calls, FBO extension and vendor
   PVRTC/ATC compressed texture handling.
4. Android JNI VM and Java class/method surface for the 69 exported
   native entry points and original startup.
5. Original platform audio, save and input services.
6. Enable the existing DualShock 4 touchpad/sticks mapping only
   against a live translated game.

**Critical distinction:** symbols existing in the ELF do not prove
they are exercised by the first splash or menu. Use the program's
real startup path, original-only test traces, and unresolved import
reports to decide invocation order. Avoid stubs that return success
without implementing behavior.

The next user-local Windows tool is
`xperia_guest_import_report.exe`, which reads the user's private
game library, reports missing imported names and JNI address
metadata, and exits non-zero when required services remain. This
must not be treated as a bootable original-game executable.
