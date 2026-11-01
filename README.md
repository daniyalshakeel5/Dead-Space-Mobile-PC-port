# Dead Space Mobile — Native Windows/Linux rebuild (research stage)

> **Not a playable PC game yet.** This repository currently contains
> original PC-port research tools, a C++17 input/timing foundation, and a
> local, user-owned APK asset installer. No working reconstructed game
> engine, graphics, mission logic, audio, or animation has been verified.
> The original Android ARM code is **not** executable natively on x86-64.

## Project goals

Reconstruct Dead Space Mobile for native x86-64 Windows and Linux with
full keyboard/mouse support, performance suitable for modest hardware, and
**no ARM emulator or runtime instruction translation**. Use the user's
own legitimately obtained APK as the source of game resources. Do not
bundle, commit, or redistribute proprietary game binaries or assets.

## What works today

| Component | Scope | Verification |
| --- | --- | --- |
| `ds_input_probe` | SDL2 keyboard/mouse diagnostic | CI: Windows + Linux |
| `ds_port_input` | Provisional desktop action handling | Synthetic CTest |
| `ds_port_timing` | Bounded fixed-step host timing | Synthetic CTest |
| `ds_apk_probe` | Read-only ZIP32 APK metadata inspector | Synthetic CTest |
| `ds_asset_install` | Installs a user's APK `assets/` into a *new local folder* | Synthetic CTest on both OSes |
| `deadspace_audit.py` | Private APK native-library/ELF summary | Synthetic tests |
| `deadspace_asset_recon.py` | Redacted texture/audio/container signature reconnaissance | Synthetic tests |

Passing the tests does **not** mean the original game APK has been
verified or that its asset formats have been decoded. Original game code
and data are not included in any GitHub Actions build artifact.

## Download the native utilities

Open [GitHub Actions — native asset installer](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/workflows/asset-installer.yml).
Select a successful run and download its Windows or Linux artifact.
Those archives contain only independently written native tools.

To install resources locally (the destination must not exist):

Windows PowerShell:

```powershell
.\ds_asset_install.exe "D:\Games\DeadSpaceMobile.apk" "D:\Games\DeadSpaceAssets"
```

Linux:

```bash
./ds_asset_install "$HOME/Games/DeadSpaceMobile.apk" "$HOME/Games/DeadSpaceAssets"
```

The installer reads ZIP32 entries and writes **only `assets/`** with
bounded stored/DEFLATE extraction and CRC checks. It does not run or
decode the game. See [local installation and limitations](docs/LOCAL_ASSET_INSTALL.md).

## Research the game resources privately

The asset scanner can analyze the user's APK without uploading it:

```bash
python3 tools/deadspace_asset_recon.py --apk "/path/to/owned.apk" --output "resource-recon.json"
```

Only aggregate metadata and the APK SHA-256 are written to JSON; original
filenames and file bytes are not emitted. See
[private APK research](docs/PRIVATE_APK_RECON.md) and the
[technical handoff](docs/NATIVE_PORT_HANDOFF.md).
A private GitHub Actions workflow also supports short-lived authorized
APK access with the `DS_APK_URL` secret.

## Build the native utilities

Requires CMake 3.20+, a C++17 toolchain, and zlib development libraries
for the optional asset installer.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDS_BUILD_INPUT_PROBE=OFF -DDS_BUILD_ASSET_INSTALLER=ON
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

Enable `DS_BUILD_INPUT_PROBE=ON` when SDL2 development libraries are
available. All heavyweight project builds run on GitHub-hosted
Windows/Linux runners.

## Next evidence gate

Collect actual APK resource/ABI metadata privately, identify real loader
code and resource formats, and then implement evidence-backed parsers and
a renderer. **There is currently no verified native PC gameplay.**

## Upstream credit and provenance

This repository originated as a fork of
[RAFALAMAO/Dead-Space-Mobile-Fixes](https://github.com/RAFALAMAO/Dead-Space-Mobile-Fixes),
which documents the Android brightness/aspect-ratio fixes.
Original [XDA thread](https://xdaforums.com/t/dead-space-android-13-100-brightness-fix-almost-21-9-full-screen-decompiled-apktool.4664045/).
Credit for that upstream work belongs to its original contributors.
The independent desktop research/code does not confer rights to
distribute the original game's copyrighted content.

![Original upstream cover](front.jpg)
