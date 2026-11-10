# Community handoff and repository cleanup — 2026-11-11

## Verified release status

The owner reports approximately **two hours of gameplay on Linux** and **one hour of gameplay on Windows**, with excellent observed visuals and performance on Windows. These are owner-reported hands-on gameplay tests, not CI certification, full-campaign completion, or verification of every peripheral.

The native port uses ahead-of-time translation of the original game's ARM code. Contributors must supply their own supported original APK; do not commit proprietary game binaries or assets.

## Pull-request triage (snapshot 2026-11-11)

| PR | Head → base | Assessment | Next action |
| --- | --- | --- | --- |
| [#5](../pull/5) | owner-ds2-controls → main | Draft, GitHub mergeable; nine changed files include exact native ELF patching, controller remapping and accelerometer fallback | Validate patch against the owner's exact native binary and test controller/WSL startup; merge only after confirmation |
| [#4](../pull/4) | portable-runtime → real-baseline | Draft, GitHub mergeable; old runtime integration branch | Compare against finalized playable source; do not blindly merge older runtime |
| [#3](../pull/3) | real-baseline → milestone6-analysis | Draft, GitHub mergeable; historical first-boot diagnostics | Preserve findings, then close as superseded if no unique fixes remain |
| [#2](../pull/2) | windows-lifted-host → milestone6-analysis | Draft, not mergeable; older Windows installation/build effort | Extract any unique fixes, then close as superseded if appropriate |
| [#1](../pull/1) | remote-preflight → main | Draft, not mergeable; early native foundation | Review historical value; do not merge over current release |

## Branch triage

Keep `main` as the documented baseline. Preserve `work/deadspace-linux-touchscreen-2026-11-07` as the source of the finalized Linux release until reproducible release builds are confirmed on main.

`work/deadspace-owner-ds2-controls-2026-11-09` is a candidate for a carefully reviewed PR merge after real-binary validation.

`work/windows-build-download-2026-11-10` is ahead of main and contains Windows workflow changes **plus packaged ZIP/EXE binaries**. Review redistribution, size, provenance, and asset contents before integrating. Prefer reproducible build artifacts/releases over checking large binaries into source history.

Other historical branches (milestone5, milestone6, portable-runtime, real-baseline, remote-preflight, windows-lifted-host, xperia-arm-wrapper) should be retained until any unique source and findings are accounted for; archive or delete only after review.

## Contributor priorities

1. Reproduce clean Linux and Windows builds using a user-supplied legitimate APK, without distributing game assets.
2. Validate native Windows gameplay, input, sound, saves, and regression tests independently of synthetic fixtures.
3. Review controller patch and sensor fallback on machines without accelerometers.
4. Consolidate build documentation, known issues, and supported dependencies.
5. Merge verified improvements into main through small, reviewed PRs; avoid overwriting the working release with older branches.

## Merge policy

A green GitHub mergeability flag means there is no detected merge conflict, **not** that gameplay is safe. Keep draft PRs unmerged until tested. Do not delete branches or rewrite history during initial cleanup.
