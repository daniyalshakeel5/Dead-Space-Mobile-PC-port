# Windows Startup Fix

[Download the corrected Windows ZIP](DeadSpaceMobile-Windows-Startup-Fix.zip?raw=true)

Fixes the reported `EA::core::GetAccelerometer()` crash by carrying the later Linux stationary-sensor fallback into native Windows. Also restores Fixes/Reanimated APK setup and optional HD-pack installation. Copy the files into the existing installation and keep `game` and `userdata`.

[Native Windows verification passed](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42690400367):

- Fresh start: 1,200 genuine-game frames, exit code 0.
- Existing-install relaunch: 1,200 frames, exit code 0.
- Reanimated HD pack enabled: 1,200 frames, exit code 0, with logged HD texture replacements.

All three captured 1366x768 nonblank framebuffers and produced nonzero original-game mixer samples. Tests used software OpenGL and SDL dummy audio; hardware audio playback, long sessions and campaign completion remain unverified. Test-only Mesa and original game assets are not in the ZIP.

Local VFP, solver, KTX and sensor regressions also passed. Both genuine APK layouts were prepared successfully; all 1,729 original published resources matched, and adding the 222-file HD pack preserved userdata.

The ZIP includes `RELEASE_AUDIT.txt` and the maintained source changes. The newer Linux native-controller implementation is still missing from the source tag, later release branch and RAR. This remains the older touchscreen/keyboard controller frontend, not a full DS2-controller port.

ZIP SHA256: `undefined`

EXE SHA256: `undefined`
