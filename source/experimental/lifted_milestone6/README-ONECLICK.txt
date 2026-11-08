Dead Space Mobile — Milestone 6 native host diagnostic update

1. Extract this ZIP anywhere OUTSIDE your existing dspc game project.
2. Double-click Install-Milestone6.cmd.
3. The installer finds your original dspc project, backs up old host code and
   generated tools/out files, then updates the host and translation tools.
4. If your existing Play Dead Space (WSL).cmd is present, it starts that.
   Otherwise use the Play configuration in VS Code Run and Debug.

No typed terminal commands are required.

The game assets, libDeadSpace.so, personal saves, and your existing VS Code
launch settings are not modified. No original EA files are contained in this
package. The diagnostic is not a verified game crash fix.

Log signatures on next run:
  [RESOURCE_ROOTS]      resolved asset and save roots
  [GUEST_RESOURCE_MISS] native failed resource reads
  [JAVA_ASSET_LIST]     requested asset directory (DS_ASSET_TRACE only)
  [TEXPACK_SOURCE_VECTOR] vector after original asset decoding
  [FATAL:TEXPACK_NULL_VECTOR] vector at the proven original fault site

Source development branch:
https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/tree/work/deadspace-milestone6-analysis-2026-11-04


EXISTING LINUX/WSL NATIVE CRASH DIAGNOSIS (NOV 4, 2026):
The archived Milestone 5 Linux build completed 208 native initializers,
opened a 1280x720 SDL/OpenGL window, and reached EAIO/JNI setup before
SIGSEGV at native PC 0x08627456 after Query.getTotalMemory. This trace
also contains an AssetManager.open(data.zip) FileNotFoundException and a
software llvmpipe renderer. The log alone cannot establish root cause.

Double-click "Diagnose WSL Crash.cmd" from this update package on the
Windows machine where the Milestone 5 build and wsl-game-*.log exist.
The diagnostic finds the latest log in Downloads, discovers the ORIGINAL
Linux i686 binary from the recorded APK pathname, and runs WSL addr2line
against the native fault PC and the whole 32-bit backtrace. It writes a
wsl-game-*.symbolized.txt file alongside the log and opens it in Notepad.
It does not change, replace, rebuild, or upload the old game files.
Use the symbolized report to identify the actual translated C function;
do not assume the last Java log message is the crashing instruction.


ORIGINAL MILESTONE 5 LINUX/TEXTUREPACK FAULT (2026-11-04):
Original Linux i686 SIGSEGV at 0x08627456 symbolized to f_0035b2e0
(im::TexturePack::load), called by f_0035c844 and f_000adfc4.
It is NOT proof that Query.getTotalMemory caused the fault.
The original TexturePack vector diagnostics already exist in Milestone 6.
This one-click script applies those diagnostics to your existing Linux
dspc_sdl binary while retaining the Milestone 5 host and game files.

1. Extract this ZIP outside the existing game folder.
2. Double-click "Diagnose TexturePack Linux.cmd" from the package.
3. It expects the WSL Milestone 5 project at:
   Downloads\dspc_milestone5_one_click_WSL_FIXED\dspc
4. It validates the exact translated source and backs up both the original
   native executable and original generated function before modifications.
   It recompiles only the translated TexturePack chunk and re-links the
   original Linux i686 SDL executable. It does not regenerate the guest.
5. Launch through the existing original Play Dead Space (WSL).cmd.
6. Send the next wsl-game-*.log containing TEXPACK_SOURCE_VECTOR or
   FATAL:TEXPACK_NULL_VECTOR so we can determine when vector becomes null.

Old executable: tools/out/dspc_sdl.pre-texturepack-diagnostic.bak
Old C source: tools/out/fn_NNN.c.pre-texturepack-probe.bak
This is a data-flow diagnostic, not a proven playable-game crash fix.
