# Windows Startup Fix

[Download the replacement Windows ZIP](DeadSpaceMobile-Windows-Startup-Fix.zip?raw=true)

Fixes the reported `EA::core::GetAccelerometer()` crash by carrying the Linux stationary-sensor fallback into native Windows. Also restores Fixes/Reanimated APK and optional HD-pack setup. Keep your `game` and `userdata` folders when updating.

The ZIP includes the source changes and release-fix audit. The newer Linux native controller source remains unavailable, so the older controller frontend is still used.

Local VFP, solver, KTX, sensor and genuine-APK setup checks passed. The Windows startup workflow checks first launch, relaunch and Reanimated-HD startup; consult its run result before treating those runtime checks as passed.

ZIP SHA256: `835cfb62cdf8b05861e773ef6165db4b5b90de8a9699792f481a24db5cbc561c`

EXE SHA256: `3f5f465093948abb00360ff709955888fc55d138db6e8d25333fab29cb4767ef`
