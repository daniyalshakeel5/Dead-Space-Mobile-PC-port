# Native Windows/Linux foundation — timing and local APK preflight

Status: **desktop infrastructure only**. These components are independently
testable C++17 code, not a port of Dead Space Mobile's ARM game logic.

## Fixed-step desktop host clock

The new `ds_port_timing` library uses integer nanoseconds to schedule
deterministic host updates independently of display refresh. Defaults:
60 updates per host second, at most 8 catch-up updates per rendered frame,
and a maximum of 250ms of elapsed time accounted for per iteration.

The 60Hz default is a **desktop host policy for diagnostic scheduling**,
not a claim about the original Dead Space Mobile engine's tick rate.
It does not implement physics, animation, AI, camera behavior or interpolation
of the original game. The SDL2 input probe displays its host update counts.

Synthetic tests cover 30Hz and 60Hz, higher display polling rates,
negative durations, clock reset, invalid configuration and long stalls.

## Local, native APK metadata probe

`ds_apk_probe` is an independent cross-platform C++17 command-line program
that takes the path to an **APK already owned by the user**. No Python or Java
runtime is needed for this tool. Without an argument it prompts for the path:

    ds_apk_probe /path/to/your/DeadSpace.apk

On Windows:

    ds_apk_probe.exe "D:\\Games\\DeadSpace.apk"

It reads ZIP32 *central directory metadata only*, reporting archive entry
counts, `assets/` item count, `lib/` SO count, DEX count, manifest presence,
and a filename hint for `libDeadSpace.so`. The actual Android game is not
launched, translated, decoded, unpacked, reproduced or redistributed.
No game assets are included in its output or executable.

Conservative validation rejects encrypted/multi-disk/ZIP64 formats,
ZIP symlinks, suspicious/traversal entry names, duplicate/case-insensitive
colliding paths, malformed ZIP bounds and archives declaring more than
16GiB unpacked. This tool intentionally **does not** verify ZIP CRCs,
local headers, APK signatures, or asset content; a positive result is
not proof that an APK is authentic or fully safe.

ZIP64 or unusually structured but legitimate APKs may be rejected by
this conservative preflight. The Python GitHub-hosted research audit remains
available for deeper inspection of authorized private APK input.

## GitHub builds and downloadable diagnostics

GitHub workflow `.github/workflows/desktop-smoke.yml` runs the native
core CTest suites and builds `ds_apk_probe` and `ds_input_probe` for
Windows x64 and Ubuntu. On successful completion it uploads two small
artifact ZIPs (Windows and Linux). They contain **diagnostic executables
only**, no original Dead Space game or APK.

The Linux artifact uses system SDL2 for the SDL probe; `ds_apk_probe`
itself has no SDL dependency. Windows artifact also includes an SDL2 DLL
if required by the input diagnostic.

## Critical next milestone

Neither the public `RAFALAMAO/Dead-Space-Mobile-Fixes` fork nor this
native desktop scaffolding includes the full proprietary Android game engine.
To reconstruct actual gameplay, the authorized APK must be audited privately
and its engine entrypoints, resource formats and game loop studied as evidence.
Do not mark engine behavior 'verified' based on ZIP filename metadata.

The user requested that `dspc_milestone4.zip` be ignored. The work in this
checkpoint neither opens nor relies on that archive.
