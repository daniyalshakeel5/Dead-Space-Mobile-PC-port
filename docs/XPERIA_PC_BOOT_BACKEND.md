# Actual Xperia original-game boot backend for PC

The project can now invoke the independently verified
[EapRules ARM/Linux game loader](https://github.com/EapRules/deadspace-native-arm)
under Linux x86-64 `qemu-arm` in Docker Linux containers using the
user-owned Xperia Play v1.1.33 ZIP after installation.

`tools/xperia_qemu_boot.py` checks the SHA1 of the original ARM ELF,
uses the exact pinned EapRules GPL-3.0 source revision
`cc8fd2c8a16530936aa3c2ce205ef4cffc2814e9`, builds only
GPL code in the Docker image, and mounts original game data
READ-ONLY when running the game. The user must opt in to
executing the external GPL source.

```sh
python -m tools.xperia_qemu_boot --game-dir /local/installed-xperia --dry-run
python -m tools.xperia_qemu_boot --game-dir /local/installed-xperia --allow-external-build
```

Requirements: Docker Engine or Docker Desktop in **Linux container**
mode, Git, and Linux/WSL2 as needed. Local boot logs and a JSON report
are stored in `xperia-boot-diagnostics/`. No EA binaries/assets or
Reanimated APKs are committed, embedded in Docker build, or uploaded.

The runtime checks actual original game:
M2 original ARM ELF loaded, M3 JNI_OnLoad and NativeOnCreate returned,
M4 GLES 1 surface initialized, M5 >=60 original frames, M6 >=50
content opens / >=10 textures / >=100 draw calls / nonblack
framebuffer, M7 input-driven scene changes. The synthetic GitHub
unit tests *only* validate the adapter and classifier; they are NOT
a new demonstration of a game boot. EapRules upstream separately
reports M7/7 on x86-64 qemu-arm.

This compatibility backend is separate from the future native Windows
Dynarmic JIT runtime. Neither Intel's opaque Android-x86
libhoudini blobs nor the IDA uEmu plugin is included. The existing
Windows WPF launcher will need runtime detection and a user-facing
external GPL consent/settings view before it can launch the Linux
container backend without terminal commands.
