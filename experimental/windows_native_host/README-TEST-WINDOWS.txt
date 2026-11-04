Dead Space Mobile - native Windows x86 experimental game build

There are NO copyrighted EA assets, binaries, or generated ARM-to-C code in
this archive. All game content remains in your own existing WSL project.

No terminal commands:
1. Extract this archive outside your current dspc folder.
2. Double-click Build-DeadSpace-Windows.cmd.
3. It locates your already translated WSL project or opens a folder picker.
4. The one-click workflow compiles game functions locally to an x86 Windows
   SDL2/OpenGL executable, using WSL only as a cross-compiler.
5. It launches the new executable directly on Windows.
6. On crash, send windows-build/logs/windows-run.log.

Nothing overwrites the original game, current translated C, saves, or WSL
runtime. New Windows-specific build results are in windows-build/.

The actual host/SDL Windows PE32 link is verified in GitHub CI with synthetic
guest functions. Actual native Windows Dead Space gameplay remains UNVERIFIED.
