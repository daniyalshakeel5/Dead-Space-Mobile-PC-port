# Windows-native Xperia Play ARM32 runtime — November 3, 2026

This checkpoint is on `work/deadspace-xperia-arm-wrapper-2026-11-02`.
**Windows is the primary product and development target.** No WSL2,
Docker, Android x86 image, user-managed VM, or ARM-to-x86 machine
translation shim inside the game distribution. The CPU translator is
Dynarmic (0BSD) hosted directly in a Windows x86-64 process.

## Native Windows JIT now verified

Prior Windows CI failed because the pinned Dynarmic source requires
Boost.Variant **and** Boost.ICL; adding only Boost.Variant was
insufficient. The CI now installs all required Boost packages and
checks the three headers before building.

First Windows-native Windows 2022/MSVC x86-64 successful run:
https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41516764921

The test suite executes original-free synthetic A32 instructions,
ELF32 address mapping/relocations, guest->host SVC calls and return,
and ordered constructor calls. This establishes a **real ARM32
translation engine on Windows**, but not original game execution.

## Sandboxed game asset I/O

`experimental/xperia_guest_loader/guest_filesystem.hpp/.cpp` is a
Windows/Linux backend for **read-only** access to the user's installed
Xperia `assets/` tree. It uses canonical path containment,
rejects traversal, absolute host file paths, symlink escapes,
write/create flags and excessive reads. It maintains independent
guest integer file handles; original host FILE pointers never
leak into translated ARM memory.

ARM32 guest service trampolines now include:

- libc memory/string: memcpy, memmove, memset, memcmp, strlen,
  strcmp, strncmp
- bounded guest heap: malloc, calloc, free, realloc
- guest POSIX assets: open, read, lseek, close
- guest stdio assets: fopen, fread, fseek, ftell, fclose

The implementation respects AAPCS r0-r3 arguments and return values;
failure on unknown or unmounted services is **not** silently replaced
with made-up successful results. Writes to copyrighted original
game data are not implemented. Save files will need a separate
explicit sandbox and matching ABI behavior.

- Real Windows and Linux sandbox unit tests check legitimate game
  asset reads, EOF, repositioning, file-handle lifecycle and
  traversal/symlink rejection.
- Guest ABI tests check the same file subsystem via ARM call
  argument registers.
- Dynarmic integration test executes **synthetic** A32 BLX to the
  file-service veneers, then validates the guest read buffer and
  denies an escaping path. It is not the original game engine.

## User-owned Xperia ARM import diagnostic

`experimental/xperia_armjit/xperia_guest_import_report.cpp` is
built into `xperia_guest_import_report.exe` in the Windows-native
JIT workflow artifact. It loads the **local** original Xperia
`libEAMGameDeadSpace.so`, applies the supported ELF relocations,
lists original symbol import counts, names missing runtime services,
and identifies JNI exports and constructor count.

Example in PowerShell (developer-only; end users should not have
to use a terminal in the final release):

```powershell
.\xperia_guest_import_report.exe "C:\Games\DeadSpaceMobile\lib\armeabi\libEAMGameDeadSpace.so" > original-imports.json
```

Exit code `4` means missing imports — an honest
**unplayable** result, not a crash. The file stays on the user's PC;
the game ARM library and assets are NEVER uploaded to GitHub.
The importer checks the expected 4,499,468-byte Xperia original
library size. It does **not** execute EA ARM instructions.

## Still missing — not a playable Windows game

Actual original Xperia ARM constructors and `JNI_OnLoad` have NOT
executed within the Windows guest. Remaining Bionic/Android logging,
threading, time, JNI/JavaVM, OpenGL ES 1.x, audio, input events,
save files and original ARM runtime structure initialization must
be implemented and validated. The existing Diamond Rush-derived
WPF launcher still has Play disabled.

The next subsystem should be selected from the **original game's
import report**, not assumed from generic Android documentation.
Real original-game startup evidence is the acceptance test.
