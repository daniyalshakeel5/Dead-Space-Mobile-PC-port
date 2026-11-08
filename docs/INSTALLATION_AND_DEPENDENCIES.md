# Installation, dependencies and building

Updated 2026-11-08. This guide applies to the **Linux i686 touchscreen release**. For the exact packaged procedure, see [START-HERE](../source/packaging/linux-touchscreen/START-HERE.md). For implementation details, see [Technical Build Guide](TECHNICAL_BUILD_GUIDE.md).

## Supported and unverified platforms

The prebuilt binary is a **32-bit x86 Linux ELF**, not an ARM emulator, Android application, or native Windows executable. Debian 13 on an x86-64 touchscreen system is the tested environment. Ubuntu 24.04+ is expected to satisfy the baseline libc requirement but is not equivalently hardware-verified. Windows 11/WSL2/WSLg launchers are experimental and run the **Linux** executable. Windows PE32 experiments are separate; do not present them as a verified replacement for the Linux release. ARM Linux, macOS and native Windows cannot execute this Linux binary directly.

Runtime minimums: x86 with SSE2, glibc **2.38+** with 32-bit loader, SDL2 **2.0.18+** (32-bit), desktop OpenGL compatibility support and matching 32-bit graphics drivers. Plan for **2 GB free** for extraction and staging. A touchscreen is recommended; Linux IIO sensors are required for real accelerometer gestures.

## Debian 13 / Ubuntu 24.04+ runtime dependencies

```bash
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install libc6:i386 libgcc-s1:i386 libstdc++6:i386 \
  libsdl2-2.0-0:i386 libgl1:i386 libgl1-mesa-dri:i386 unzip
# Optional APK picker:
sudo apt install zenity
```

NVIDIA proprietary users also need the driver-matched **32-bit** OpenGL userspace. Confirm `ldd ./dspc_sdl` reports no missing libraries. Do not manually replace system libc to meet the minimum.

## Install the published build

1. Obtain the [Linux touchscreen release](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/releases/tag/Linux-touchScreen-build), extract **the entire ZIP** to a writable, executable directory.
2. Supply your own supported **Fixes** APK or **DeadSpaceReanimated v1.3** APK. The original game library must have SHA1 `64a3323cd3cc0c483a645b62af8508624163ba9b`; filenames alone are not proof of compatibility. No game assets are distributed in the source tree.
3. From the extracted package directory, run:

```bash
chmod +x dspc_sdl Play-Dead-Space-Touch.sh 'Play Dead Space Mobile.sh'
bash './Play Dead Space Mobile.sh' '/path/to/Deas.Space.Mobile.Fixes.apk'
# Or run without an APK argument to select one interactively:
bash './Play Dead Space Mobile.sh'
# Optional window:
bash './Play Dead Space Mobile.sh' --windowed
```

The launcher installs game files from the user's APK on first run. The game runs with original data; no synthetic substitute establishes success. Save data defaults to `~/.local/share/dead-space-mobile/var/`; logs to `~/.local/state/dead-space-mobile/logs/`. Back up saves before replacing the package. Overrides: `DS_DATA`, `DS_LOG_DIR`, `DS_WIDTH`, `DS_HEIGHT`, `DS_HD_TEXTURES=0`, `DS_AUDIO=0`, `DS_ACCEL_DIAG=1`, and `DS_AIM_DIAG=1`.

## Build from source (advanced)

Clone `main` and work in `source/experimental/lifted_milestone6/tools/`. The checked-in files are the translator, compatibility host, build scripts, tests and launchers; **generated game-derived C and original APK content are not checked in**. Supply your legally obtained supported donor APK and use the documented extraction/generation workflow in [Technical Build Guide](TECHNICAL_BUILD_GUIDE.md), then run `build.sh` in the generated working directory.

On Debian 13, install build-time dependencies:

```bash
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install build-essential gcc-multilib libc6-dev-i386 \
  libsdl2-dev:i386 libgl1-mesa-dri:i386 python3 python3-venv \
  unzip file xvfb xauth
python3 -m venv .venv
. .venv/bin/activate
pip install pyelftools Pillow
```

Build flags include `-m32 -O1 -msse2 -mfpmath=sse -ffp-contract=off`; linker uses `-m32 -no-pie`. `DS_BUILD_JOBS=4` enables bounded parallel compilation and `DS_DEBUG=1` enables debug symbols. Building generated translated C requires substantial RAM and disk; use CI or a suitable development machine. The source does **not** build a complete playable game without the original donor.

## Verification and limitations

The finalized Linux work reports user-confirmed 1366×768 touch/hit detection, opening skybox, audio and persistent saves on its tested device; these are device-specific reports, not universal guarantees. A later motion-gated level, previously affected combat flashing, native Windows gameplay, and physical controller operation remain incompletely verified. Passing CI compilation or a scripted 24,000-frame run is not proof of a completed playthrough.

For diagnostics, provide the exact release version, donor variant, `ldd` output, stdout/stderr, exit code, display resolution and input hardware. Do not upload copyrighted game files or personal saves publicly.

## Licensing and credits

Dead Space Mobile and original assets belong to their respective rights holders. This repository documents an independently built compatibility/translation host. See the existing source-tree third-party license notices and [technical guide](TECHNICAL_BUILD_GUIDE.md). Reanimated HD pack credit: GINESTR0.
