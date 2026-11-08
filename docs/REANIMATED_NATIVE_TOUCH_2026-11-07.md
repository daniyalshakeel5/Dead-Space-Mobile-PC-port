# Reanimated v1.3 — Native Linux touch-target compatibility

> Historical investigation. HD loading, audio, save storage and the solver
> correction are now described in [the current technical guide](TECHNICAL_BUILD_GUIDE.md).

## Provenance and evidence

The user verified the original Linux touchscreen port launches and responds to touch, but on a large display the original 2011 UI/object interaction hit areas are too small. They requested the improvements from [Dead Space Reanimated](https://github.com/GINESTR0/DeadSpaceReanimated) by **GINESTR0**, specifically version **v1.3**.

Authoritative archive inspection: [run #42361073085](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42361073085).
Byte-for-byte comparison: [run #42361252511](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42361252511).
Verified donor extraction: [run #42362386417](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42362386417).

- Reanimated v1.3 is a 715,366,829-byte APK.
- Its embedded guest `assets/game/libDeadSpace32.so` is **byte-identical** to our Fixes APK's `lib/armeabi/libDeadSpace.so`: SHA1 `64a3323cd3cc0c483a645b62af8508624163ba9b`.
- All **1,729** published game files have the same filenames and CRCs in both APKs, with zero changes.
- Reanimated additionally supplies **222** KTX HD textures and an **ARM64 Android-only** custom runtime `lib/arm64-v8a/libdsr.so`. None of the changes to touch hitboxes reside in a modified copy of the 2011 ARM library. Do **not** mistake asset extraction for transferring the gameplay fixes, or claim the native Linux build uses the ARM64 runtime or HD KTX replacements.

Reanimated's README explains that interaction circles react across their visible area and tap/double-tap thresholds scale to high-density screens. Inspecting its distributed ARM64 binary (metadata only; no redistribution) found `dsr_hook_tap_tweaks` and diagnostic strings `tap tolerance 10 -> ... px`, `double-tap distance 30 -> ... px`. These are evidence of the *kind* of changes, not a full source-level disclosure of the closed implementation.

## Independent native x86 implementation

Original 2011 ARM binary function symbols were resolved and translated to C with authentic game assets; results in [run #42361406816](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42361406816) and [run #42361669768](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42361669768). Only the exact verified instruction locations are patched:

1. `InputForwarderTaps::getRecentReleasePointer(int,int)`, RVA `0x0007aefc`: scale the original 30-pixel distance read from config offset `+468`.
2. `InputForwarderTaps::onEvent(im::Event const&)`, RVA `0x0007b810`: scale the original 10-pixel threshold read at offset `+472`.
3. `Hud::doSpecialAction_InteractionWithObjects(...)`, RVA `0x0006a908`: scale the original HUD radial object interaction sampling radius (`V[23]`) so the search covers a proportionally larger area.

`touch_reanimated_patch.py` checks the guest ELF SHA1 and the *exact function bodies and call-site patterns* before patching generated C; mismatches terminate generation rather than guessing. The generated code uses `ds_reanimated_touch_scale()` from `rt.h` to scale by the **actual SDL drawable height / 480**, bounded to **1.0–3.0**, with the original pixels unchanged at 480p. This is a native reimplementation **informed by Reanimated**, not an extraction, incorporation or direct port of Reanimated's proprietary runtime implementation.

Linux launcher enables `DS_REANIMATED_TOUCH=1`. The user can A/B test the old exact engine behavior via:

```bash
DS_REANIMATED_TOUCH=0 ./Play-Dead-Space-Touch.sh --windowed
```

The launcher accepts either the user's Fixes APK or `DeadSpaceReanimated-1.3.apk`, extracting only the original `libDeadSpace32.so` and `assets/game/assets/published` tree from the latter. The HD KTX pack and closed ARM64 Android translator are deliberately not shipped or executed on x86 Linux.

## Acceptance tests

- Compile/link i686 native Linux from the genuine ELF, not mock game data.
- Run full genuine-APK 24,000-frame sequence with two overlapping SDL_FINGER contacts. Check logs for `REANIMATED_TOUCH_PATCH` entries at all three RVAs and `[REANIMATED_TOUCH]` viewport-scale report.
- **Hardware test still required**: At a visible door/object interaction circle, compare taps at centre and near outer edge at the same distance with remastered scaling on and off; confirm no unintended nearby object activations. Confirm tap/double tap and regular walking still work.
- Unresolved: skybox and audio; Reanimated HD KTX textures are not yet integrated into the Linux renderer.

No game libraries or copyrighted assets are distributed in this research.
