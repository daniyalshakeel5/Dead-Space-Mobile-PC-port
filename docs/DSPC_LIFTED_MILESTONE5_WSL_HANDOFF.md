# Dead Space Mobile lifted x86 — milestone 5 / WSL + VS Code handoff

**Status:** The user's second agent reports the lifted x86 runtime already renders the original game's Select Difficulty menu under Linux/WSL. That result is not independently verified by this agent. Do not replace this working path with the separate Dynarmic Xperia Play wrapper.

## Runtime identity and provenance

- Input: user-owned `dspc_milestone5.zip` from the conversation.
- This runtime translates **`libDeadSpace.so`** into 32-bit native x86 C; it does **not** use the other wrapper's `libEAMGameDeadSpace.so`.
- The integration patch and full source archive were created and linked in the user's conversation as `dspc_milestone5_wsl_vs_code.patch` and `dspc_milestone5_wsl_vs_code.zip`. They are not yet committed to this GitHub branch in full; import the source from the archive before building it from GitHub.
- Never commit the original APK, .so game library, extracted proprietary game resources, or generated ARM-lifted `fn_*.c` to a public repository.

## Implemented in source archive

- SDL2 PS4 controller, keyboard, mouse and touchpad forwarding to **original JNI input exports**, using an independently unit-tested virtual-finger state machine. Disconnect/focus-loss cleanup is included.
- Android AudioTrack PCM output on SDL with format/queue safeguards.
- Separate writeable game data directory, plus initial i686 MinGW/Windows source portability.
- `tools/wsl_run.sh` commands: `--doctor`, `--test`, `--extract`, `--build`, `--smoke`, `--run`.
- WSL project-local Python venv for `unicorn==2.1.4`, `pyelftools==0.32`, Pillow.
- VS Code tasks and WSL gdb launch configuration; build script can emit debug symbols if `DS_DEBUG=1`.
- Separate `README_WSL_VSCODE.md` plus `.gitignore` to exclude user game data.

## User workstation

The user explicitly confirmed WSL and VS Code are installed. The next reproducible test should happen **inside the user's own WSL distro**, because this agent does not have their original game assets or WSLg display.

```bash
cd ~/dspc
bash tools/wsl_run.sh --doctor
bash tools/wsl_run.sh --extract "/mnt/c/Users/<WindowsUser>/Downloads/<original game>.apk"
bash tools/wsl_run.sh --test
bash tools/wsl_run.sh --build
bash tools/wsl_run.sh --smoke
bash tools/wsl_run.sh --run
```

Run `--extract` only if the APK has not already been extracted. The APK must provide `apk/lib/armeabi/libDeadSpace.so` and `apk/assets`.

`--smoke` saves `logs/wsl-frame5.png` and a boot log; inspect them against the second agent's Select Difficulty screenshot. Use WSLg. If the DualShock 4 is not visible to SDL inside WSL, test it under native Windows or configure USB passthrough; Windows controllers are not necessarily available in WSL.

## Verified here

- `cc -std=c11 -Wall -Wextra -Werror -pedantic tests/test_ds_input.c tools/ds_input.c -lm`: PASS.
- Bash syntax for WSL and Win32 scripts: PASS.
- VS Code tasks and debugger JSON: valid.
- New ZIP archive integrity: PASS; full patch applies cleanly to unmodified supplied milestone 5.
- **Not verified:** original game boot, 32-bit WSL game build (no user APK or SDL i386 in this container), original audio/graphics, native Win32 game executable.

## Immediate next work

Use the new WSL log and screenshot to confirm that the second agent's lifted source is indeed reproducible. Then fix only the issues seen in that working path: game input behavior, audio correctness, PVRTC/ATC rendering, and finally the native 32-bit MinGW build. The runtime's `tools/build_win32.sh` is experimental and should not be marketed as a verified Windows executable.
