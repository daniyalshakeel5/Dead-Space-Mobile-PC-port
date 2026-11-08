# Dead Space Mobile — Linux release

This package contains the prebuilt native **32-bit x86 Linux** port. You do not
need to compile it or install Python, GCC, Java, Android, or Unicorn to play.
Keep the launchers beside `dspc_sdl`; do not launch the executable directly.

## 1. Requirements

- An Intel/AMD x86 or x86-64 Linux computer with SSE2 and a working desktop
  OpenGL compatibility driver. ARM Linux, macOS and native Windows execution
  are not supported by this executable.
- **glibc 2.38 or newer**, including the corresponding **32-bit** runtime.
  Debian 13 is the tested system. Ubuntu 24.04 or newer meets the libc version
  requirement, but has not had the same physical-device testing. Debian 12 and
  Ubuntu 22.04 are too old for this prebuilt binary; use a newer OS or rebuild
  from source for the older system. Do not replace system libc manually.
- 32-bit SDL2 **2.0.18 or newer**, 32-bit OpenGL/Mesa drivers and the packages
  listed below. A 64-bit SDL installation alone is insufficient.
- A touchscreen is recommended. Mouse input is also supported. Motion input
  requires a Linux IIO accelerometer exposed by the device; ordinary desktop
  PCs and WSL usually do not provide one.
- Allow at least **2 GB free space** on the extraction drive for first-run
  extraction/staging, plus space for your APK. Keep the extracted package on
  a writable, executable filesystem. SD cards are fine if mounted normally;
  `noexec` mounts cannot run the game. The tested machine has 4 GB RAM; HD
  textures need more memory than the original assets.

## 2. Install runtime dependencies once

On Debian 13 or Ubuntu 24.04+, run these in a terminal:

```bash
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install libc6:i386 libgcc-s1:i386 libstdc++6:i386 \
  libsdl2-2.0-0:i386 libgl1:i386 libgl1-mesa-dri:i386 unzip
```

Optional graphical APK file picker:

```bash
sudo apt install zenity
```

For NVIDIA proprietary graphics, install the distribution's matching 32-bit
OpenGL driver as well. On other distributions use their equivalent multilib
runtime packages; the apt commands above are not portable to them.

From the extracted release directory, check dependencies with:

```bash
ldd ./dspc_sdl
```

No entry should say `not found`. `GLIBC_2.38 not found` means the installed
32-bit libc is too old. A “No such file or directory” error for an executable
that clearly exists often means the 32-bit ELF loader is missing.

## 3. Supply ONE compatible APK

No APK, original game library, HD textures, saves, or personal game data are
included. You supply your own compatible APK. Choose **one** of these:

| APK | Result |
| --- | --- |
| `DeadSpaceReanimated-1.3.apk` by GINESTR0 | Original game data **plus the 222-file HD texture pack**. Recommended for HD. |
| Original supported Dead Space Mobile **Fixes** APK (the tested file is named `Deas.Space.Mobile.Fixes.apk`) | Original game data/textures; works without the HD pack. |

The filename alone does not determine compatibility. The included original
ARM library must have SHA1:

```text
64a3323cd3cc0c483a645b62af8508624163ba9b
```

The launcher validates that hash. Expected locations inside the APK:

- Fixes: `lib/armeabi/libDeadSpace.so` and `assets/published/`.
- Reanimated v1.3: `assets/game/libDeadSpace32.so`,
  `assets/game/assets/published/`, and `assets/texture_hd/*.ktx`.

An Xperia Play APK, an arbitrary APK with a different game library, or a GitHub
“Source code” ZIP is not a substitute. Later Reanimated versions are not
certified here; the tested donor is v1.3. Reanimated project and release:
[github.com/GINESTR0/DeadSpaceReanimated](https://github.com/GINESTR0/DeadSpaceReanimated/releases/tag/v1.3).
You do not need both APKs or an Android installation. The Linux runtime does
not execute Reanimated's ARM64 Android runtime or claim all of its features.

## 4. Extract and launch on Linux

Extract the **entire** release ZIP to a writable directory, then open a terminal
there. If the file manager does not preserve execute permissions:

```bash
chmod +x dspc_sdl Play-Dead-Space-Touch.sh 'Play Dead Space Mobile.sh'
```

First launch, with your chosen APK path (quote paths containing spaces):

```bash
bash './Play Dead Space Mobile.sh' '/path/to/DeadSpaceReanimated-1.3.apk'
```

Or run the same launcher without an APK argument to use the file picker or
terminal prompt. It extracts your files into `game/` beside the executable.
Wait for first-run extraction to finish. Later launches reuse those files:

```bash
bash './Play Dead Space Mobile.sh'
```

Windowed mode:

```bash
bash './Play Dead Space Mobile.sh' --windowed
```

To add HD textures to an installation initially made with the Fixes APK,
run the launcher once with the Reanimated v1.3 APK path. It adds the HD pack
without replacing saves. Keep `game/` for future launches; the APK itself
need not stay in its original location after extraction.

The default requested size is **1366×768**. Fullscreen follows the actual
display drawable, and the game and touch coordinates use matching dimensions.
On another display, requested window size can be overridden:

```bash
DS_WIDTH=1366 DS_HEIGHT=768 bash './Play Dead Space Mobile.sh' --windowed
```

## 5. Windows launcher included for WSLg

`Play Dead Space Mobile.cmd` now starts the **bundled executable** through
`DeadSpaceMobile-WSL.ps1`; it does not download an old source branch or rebuild.
This is an optional WSL route, **not a native Windows executable**. It has not
had the same end-to-end testing as the native Linux/device build.

Use Windows 11 with WSL2, WSLg and an Ubuntu 24.04 distribution. Install WSL
from an administrator PowerShell if needed:

```powershell
wsl --install -d Ubuntu-24.04
```

Complete Ubuntu's first-run username/password setup. Run the dependency commands
in section 2 **inside Ubuntu**, and make it your default WSL distribution if
another older distribution is currently default:

```powershell
wsl --set-default Ubuntu-24.04
```

Extract the whole release ZIP on a drive accessible to WSL. Double-click
`Play Dead Space Mobile.cmd` and select your APK when asked. Later launches
reuse `game/`. To add the HD donor or choose windowed mode, run from PowerShell:

```powershell
& '.\Play Dead Space Mobile.cmd' -ChangeApk -Windowed
```

Saves and logs are inside that WSL distribution's Linux home, not Windows
Documents. Access them through `\\wsl.localhost\Ubuntu-24.04\home\YOUR_LINUX_USERNAME\`.
WSL may not expose touchscreen/motion hardware like native Linux does.

## 6. Saves, logs, backup and updates

Default Linux save files:

```text
~/.local/share/dead-space-mobile/var/
```

This includes `settings.sb`, `AchievementsSave.sb`, `00_LevelSave.sb` and
`00_CheckpointSave.sb` when the game creates them. Save creation and loading
are owned by the game; exiting does not guarantee a new checkpoint at any
arbitrary position. The fixed build's save/reopen behavior was user-confirmed.

If `XDG_DATA_HOME` is set, the location is
`$XDG_DATA_HOME/dead-space-mobile/var/`. To choose another writable parent:

```bash
DS_DATA='/path/to/my-save-directory' bash './Play Dead Space Mobile.sh'
```

Saves will be under `/path/to/my-save-directory/var/`. Use the same override
every time. Back up the entire `var` folder while the game is closed. The
launcher copies legacy `game/assets/var` files once, without overwriting newer
files in the new location. It never needs to erase your original saves.

Logs:

```text
~/.local/state/dead-space-mobile/logs/
```

`XDG_STATE_HOME` changes the state root; `DS_LOG_DIR` overrides the log directory.
The launcher prints the exact save and log paths at every start. Report the
latest stdout/stderr logs and exit-code file when reporting a problem.

For an update, close the game and back up saves. Extract the new release into
its own folder. Reuse your APK or move/copy the existing `game/` folder into
that folder. The default XDG save directory survives package replacement and
moving the game between drives. Do not delete it unless intentionally resetting
progress. Uninstalling the package folder alone leaves saves intact.

## 7. Optional troubleshooting

- Original textures for comparison:
  `DS_HD_TEXTURES=0 bash './Play Dead Space Mobile.sh'`
- Audio diagnostics: normal logs contain `[AUDIO_OPEN]` and `[AUDIO_PCM]`.
  Check the system's selected output and mute state. `DS_AUDIO=0` disables sound.
- Motion diagnostics:
  `DS_ACCEL_DIAG=1 bash './Play Dead Space Mobile.sh'`
- First load from a slow SD card can take time, especially with HD textures.
  Avoid launching a second copy while the first is still loading.
- If the wrong ELF hash is reported, use a supported donor; renaming an APK
  cannot change its contents.
- If saving fails, check the printed save path, disk space and permissions.
  Do not point `DS_DATA` at the immutable resource directory.
- The known solver initialization correction is included for stretched polygons.
  The exact previously affected combat scene still needs user confirmation;
  stop playing if severe flashing reappears and report the logs.

## 8. Source, checksums and attribution

`BUILD_INFO.txt` identifies the binary and source revision. `SHA256SUMS.txt`
lets you check the extracted package:

```bash
sha256sum -c SHA256SUMS.txt
```

See `TECHNICAL_BUILD_GUIDE.md` for architecture and rebuilding. Port source:
[Dead-Space-Mobile-PC-port](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/tree/work/deadspace-linux-touchscreen-2026-11-07/source).
ATC decoder attribution is in `THIRD_PARTY_ASSETSTUDIO_LICENSE.txt`. SDL2 and
other runtime libraries are installed through your distribution, not bundled.
Reanimated's HD pack is by GINESTR0. Dead Space and original game content belong
to Electronic Arts; this package contains the port runtime and tooling only.
