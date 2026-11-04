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
