# Feeding the APK into GitHub-hosted research

## What the fork provides
The public fork `daniyalshakeel5/Dead-Space-Mobile-PC-port` is based on
`RAFALAMAO/Dead-Space-Mobile-Fixes`. Its README links to the XDA mobile fixes
project. Forking the repository does **not** automatically transfer the
separate original APK held in the user's ChatGPT Library.

The research branch has a successful GitHub-hosted **repository-only**
test/inventory run (run 41236282588, 2026-10-31).
Its APK analysis step was skipped. No game ABI or behavior is verified by this run.

## Supply an APK privately

1. Do **not** commit, push, or attach the copyrighted APK, OBB, extracted
   libraries, or game assets to this public repository, issues, pull requests,
   actions artifacts, or public releases.
2. From storage under your control, obtain an **HTTPS, time-limited,
   hard-to-guess, download URL** for your legitimately obtained APK. The current
   workflow uses `curl`; it does not authenticate to private GitHub releases.
3. In the fork's **Settings > Secrets and variables > Actions**, add an
   *repository secret* named `DS_APK_URL` containing that URL. Keep it out of
   commits, comments and workflow log messages. Rotate/revoke it afterward.
4. Open **Actions > Dead Space Mobile - remote port research > Run workflow**,
   selecting branch `work/deadspace-remote-preflight-2026-11-01`. The manual
   job uses the GitHub runner's temporary filesystem, not the local assistant VM.
5. Inspect the generated `deadspace-port-audit` artifact. The report contains
   APK hash, native libraries' architecture, dynamic dependencies and import
   *hints*. It does **not** contain the original binary.
6. Verify that `Analyze private APK if configured` actually **ran**; a green
   workflow with that step skipped confirms only synthetic fixture tests and
   repository inventory.

## Next steps after a real APK audit
- Establish binary hash, ABI, library dependencies and the JNI boundary.
- Reverse-engineer relevant game entrypoints and data formats using isolated,
  non-distributed evidence, then document confidence levels.
- Build a native CMake/SDL desktop harness for Windows/Linux only after evidence
  makes its interfaces useful. It will load the user's original assets locally.
- Test actual rendering, control and gameplay behavior before describing the
  project as a working port.

**Important:** Android ARM32 code cannot be compiled into an x86-64 game merely
by forking apktool output or installing a compatibility layer. The native game
logic requires a genuine reimplementation or a separately authorized source
port.
