# Dead Space Mobile — native Windows/Linux port handoff

## Project target

Rebuild the Android Dead Space Mobile game as an x86-64 desktop program for
Windows and Linux, with full keyboard/mouse support and low-end hardware in mind.
The requested end state is a true native implementation, **not** ARM emulation,
a virtual Android device, or a runtime ARM instruction translator.

## Evidence and limits

- The user-owned APK and the research checkpoint were recovered in the ChatGPT
  Library on 2026-11-01, but **they have not been transferred to GitHub**.
- Earlier research indicated a native 32-bit ARM game library named
  `libDeadSpace.so`. Verify the exact package contents with the audit before
  treating any library/import assertion as a fact.
- This repository currently hosts the Android/mobile fixes code. Its existence
  does **not** demonstrate that a native Windows/Linux port works.
- The research workflow only inventories files and ELF imports. It does **not**
  decompile or translate game code, and produces no executable PC game.

## GitHub remote research workflow

`.github/workflows/deadspace-research.yml` runs on a GitHub-hosted Ubuntu
runner, uses synthetic tests, writes `report/deadspace-audit.json`, and uploads
only the metadata report.

On a manual **workflow_dispatch** run, an optional GitHub Actions secret
`DS_APK_URL` can point to a short-lived HTTPS download URL for a **private,
legitimately obtained** copy of the user's APK. The runner downloads the game
into its temporary directory, inventories its native libraries and imports,
and uploads **only JSON metadata**. Never commit, attach, or upload proprietary
game assets or native libraries to the public repository or workflow artifacts.
Avoid exposing URLs in logs. If no URL is configured, the job performs a
repository-only scan.

## Current verified desktop infrastructure (November 1, 2026)

- GitHub Actions C++17 compilation and CTest passed on Windows 2022 and
  Ubuntu 24.04 runners (four matrix jobs in run 41281887711).
- `ds_port_input` implements *provisional PC-style* keyboard/mouse events,
  not verified Android game input mappings.
- `ds_port_timing` implements bounded, nanosecond-accounted host update
  scheduling; 60Hz is a configurable host policy, **not a verified mobile
  simulation rate**.
- `ds_apk_probe` reads local user-supplied ZIP32 APK central-directory
  metadata, without unpacking or shipping game assets. Synthetic archive tests
  passed. **It has not been exercised against the user's actual APK**.
- Diagnostic artifact ZIPs are produced by the cross-platform workflow.
  They are utilities, not a playable game.
- The user requested that `dspc_milestone4.zip` be ignored. Do not consult
  or integrate it as an evidence source.

See `docs/NATIVE_HOST_AND_APK_PREFLIGHT.md` for tested interfaces and limits.

## Next engineering milestones (each needs evidence)

1. **Native dependency inventory:** Confirm ABI, imports, library
   dependencies, JNI boundary, and Android graphics/audio/input usage. Preserve
   audit JSON and hashes, not copyrighted binaries.
2. **Architecture map:** Identify actual game entrypoints, asset format readers,
   scene/update loop, renderer hooks, sound, and input from verified evidence.
   Label conjectures as unverified.
3. **Portable core:** The CMake/SDL2 input diagnostic and host timing core
   are now built and tested. Next integrate into a real reconstructed runtime
   only after the original engine's behavior has been established.
4. **Asset importer:** The new native APK CLI currently validates ZIP32
   archive metadata and prompts for a user-supplied package. Actual private
   asset extraction, format parsing, and loader integration remain undone.
   Never distribute original game assets.
5. **Keyboard/mouse:** Map Dead Space PC-style aiming, movement, weapon
   controls, and pause/menu behavior. Preserve original behavior first.
6. **Behavior tests:** Cross-check original game state, timings, collision,
   camera, and gameplay against captures or native observation; avoid invented
   behavior. Distinguish frame-rate-independent rendering from verified game
   simulation timing.
7. **Build and release:** Add headless unit tests and Windows/Linux CMake
   builds on GitHub Actions. Only publish the launcher and new code, not game
   data, after the build actually runs.

## Reliability policy

- Large archives, disassembly, asset extraction, decompilation, tests, and
  release builds execute on GitHub Actions runners, not the assistant VM.
- Use lightweight GitHub API operations for code updates and reviews.
- Upload compressed summaries or targeted logs only.
- Keep each engineering change reviewable on a branch/PR.
- Do not report a gameplay-capable port until an actual binary is built and
  tested on Windows and Linux.
