DEAD SPACE MOBILE — FRESH WINDOWS INSTALLER
November 4, 2026

This is a SOURCE-ONLY installer. It contains NO copyrighted Dead Space Mobile
game files or compiled game logic. You must supply your own matching original
Dead Space Mobile APK (the version containing lib/armeabi/libDeadSpace.so).

NO TERMINAL COMMANDS:
1. Download the GitHub Actions artifact DeadSpaceMobile-FRESH-Windows-Installer.
2. Extract it anywhere, e.g. your Downloads folder.
3. Double-click Install Dead Space Mobile.cmd (or Build-DeadSpace-Windows.cmd).
4. Select the original .apk file in the Windows file picker.
5. The installer automatically creates Documents\DeadSpaceMobilePC,
   extracts the original APK, validates the ARM32 library, installs the full
   ARM-to-C translator and native Windows host, ensures build dependencies
   exist in WSL, regenerates the translated game source, builds a native
   x86 Windows SDL2/OpenGL executable, and launches the Windows build.
6. Your Windows game directory and logs will be:
   Documents\DeadSpaceMobilePC\windows-build\
   - DeadSpaceMobile-Win32.exe
   - Play Dead Space Windows.cmd
   - compile-windows.log
   - ../install.log (extraction/dependency/translation log)
   - logs\windows-run.log

Important: the Xperia Play release with libEAMGameDeadSpace.so will be
REJECTED. It uses a different native game library and asset profile. Use
the original libDeadSpace.so APK. A mismatched edition must not be silently
mixed with an existing translation.

The installer refuses to overwrite an existing nonempty installation folder.
Its default location is Documents\DeadSpaceMobilePC. The APK and generated
code are kept privately on your machine; nothing is uploaded to GitHub.

WSL is a build tool only. The resulting Windows PE32 game runs directly on
Windows, not inside WSL, Android, or an ARM instruction interpreter.

VERIFIED IN CI:
- Synthetic APK archive extraction, expected layout, ARM32 ELF header check
- Full native translator and Win32 host source installation
- Safe rejection of Xperia library, ZIP traversal and duplicate installation
- Windows PowerShell 5 syntax and Bash build-script syntax
- Actual lifted-host Windows PE32 compile/link using synthetic game tables

NOT YET VERIFIED:
- Full native Windows compilation of the user-owned 16,387 translated
  game functions, or successful original-engine startup/rendering/gameplay.
- Audio and advanced analog controls are still under development.

When testing, send install.log, compile-windows.log or logs\windows-run.log
if it fails. The installer stores the logs automatically. No need to type terminal commands.

Development branch / PR:
https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/pull/2

BUILD REPAIR (for the 2026-11-04 undefined import link failure):
If the original fresh installer extracted your APK and produced tools/out but failed
at the final Windows PE link, DO NOT delete your installation and do not reinstall.
Double-click "Repair Dead Space Windows.cmd" from the new kit. It patches only the
Windows cross-build script and guest import adapters, reuses existing compiled
function objects and creates windows-build/DeadSpaceMobile-Win32.exe on success.
This proves a native Win32 link, NOT that the game's runtime boots or plays.
On any error send windows-build/compile-windows.log. For a startup crash send
windows-build/logs/windows-run.log. Never upload original game assets.

CRASH RECOVERY / FIRST-RUN STORAGE (2026-11-04):
The Windows game passed all 208 static initializers, JNI_OnLoad, OpenGL
initialization and reached resource loading before a null-pointer read at
0x00A8ADA0. The first user log repeatedly failed to access apk/assets/var.
This kit creates the missing var/ directory before launch and adds a
host-side directory check. It does NOT manufacture empty binary save files.

Double-click "Repair Dead Space Windows.cmd" to apply these changes to an
existing private installation and rebuild only the native host and adapters.
Your original APK and translated game source are not overwritten.
If the game still exits after the splash, the Play launcher automatically
writes windows-build/logs/crash-symbolized.log containing the exact C
function and line (resolved through the locally installed MinGW addr2line).
"Diagnose Dead Space Crash.cmd" can also symbolize an existing previous
windows-run.log without requiring another game run; an older host log lacks
the loaded PE base, so the address mapping assumes the PE preferred base.
Please send only crash-symbolized.log and windows-run.log, not the APK or EXE.

FIRST-RUN FILE FLAGS FIX (2026-11-04):
The real Windows game crashed after the splash at translated function
f_000629b4 (fn_003.c:17449). The full private C code was not uploaded.
The host had been passing ARM Bionic open() flags directly to the Windows
CRT. Android O_CREAT=0x40, O_EXCL=0x80 and O_APPEND=0x400 differ from
Win32; new builds translate these instead of corrupting save creation.
A separate malformed #endif previously skipped Bionic stat timestamp
fields; this is repaired as well. /proc/cpuinfo now has a conservative
in-memory read-only Android CPU info response on Windows, because
the earlier log showed repeated ENOENT from that Android system path.
No original game functions or asset files have been changed.
On a repeat crash, the diagnostic report now includes x86 registers and
a 41-line excerpt from the exact private generated C function.

MODELCACHE CRASH SOURCE CONTEXT (2026-11-04):
After all previous environment fixes, the crash remained at the exact
native guest address corresponding to fn_003.c:17449. The instruction
is r7 = LD32(r3), where r3 = LD32(sp + 52u), and r3 was zero.
It occurs in ModelCache::loadNodeUncached after f_0027e644 runs.
This is a confirmed null stack-slot value, not evidence that a missing
settings save or /proc/cpuinfo caused the problem.
No speculative skip of a native engine destructor was added.

To investigate the underlying data flow WITHOUT rerunning or rebuilding:
double-click "Inspect Dead Space Model Cache.cmd" from the kit.
The script reads only the owner's private translated source and writes
windows-build/logs/modelcache-source-context.txt. It extracts the complete
crashing function, its immediately preceding helper f_0027e644 and relevant
stack references, keeping other game code out. The report is never
uploaded to GitHub or the build service.
Send modelcache-source-context.txt privately in chat for a source-level patch.
