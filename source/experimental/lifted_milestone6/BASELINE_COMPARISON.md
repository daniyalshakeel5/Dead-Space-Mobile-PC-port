# Real APK baseline comparison

The November 4 handoff notes (`NOTE_for_other_agent_2026-11-04.md` and
`SYNC_2026-11-04_claude.md`) report 3,600 Linux software-rendered frames reaching
the corridor with milestone 6. That is a reported result, not yet independently
reproduced by this comparison. Windows builds passing does not establish Windows
gameplay, audio, or full keyboard/mouse support.

## Reproduction

Workflow: `.github/workflows/deadspace-real-baseline.yml`.
The matrix uses the archived milestone 6 tools and the source branch's current
tools. Both translate the same original ARM library, verified against SHA-1
`64a3323cd3cc0c483a645b62af8508624163ba9b`, then compile and run a native i686
SDL/OpenGL executable. Both use the extracted APK assets directory for DS_DATA
and DS_ASSETS, including a writable var directory. No generated-function
replacement probes are injected in either matrix job.

The first-boot sequence includes bottom-right difficulty confirmation clicks;
the earlier CI sequence used only center clicks. Coordinates assume the default
1280x720 window. Screenshots every 120 frames must be inspected to establish
actual progress: a zero exit code and “ran frames” alone do not prove gameplay.

The workflow extracts SDL i386 headers separately from its runtime packages
because installing libsdl2-dev:i386 on the current Ubuntu runner conflicts with
glib development tools. It uses libarchive/bsdtar for the RAR; unar failed to
decode build.sh and vcpkg.json from this archive.

## Evidence and next decisions

The earlier runtime job 126789461810 repeatedly read exploder.m3g at EOF and
crashed in translated EASTL vector<char>::DoInsertValues (f_00325b04).
This identifies the failure site, not its root cause. That run also enabled a
replacement buffered reader, model recovery and serialization, so it is not a
clean milestone 6 reproduction.

Local comparison of the archived and current source found identical lift.py,
overrides.py, gl_shim.c and threads.c. JNI stream and asset handling, libc file
handling, host diagnostics/TLS and the build driver differ.

- If archived milestone 6 reaches the corridor and current source crashes,
  isolate the JNI/libc changes before changing generated ARM semantics.
- If both reach the corridor, compare the optional probes and data-root setup
  against the failing CI runner.
- If both fail, compare the first failing frame and backtrace against the
  handoff's exact runtime/toolchain assumptions.
- After reproducing a stable baseline, validate Windows runtime and real
  keyboard/mouse input separately. Build success is insufficient.

Artifacts contain compact build/runtime logs, original library identity,
exit status, symbolized crash locations where available, and PNG frames.
No APK or extracted game resources are uploaded as comparison artifacts.
