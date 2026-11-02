# Windows field test regressions — November 2, 2026

GitHub Issues returns HTTP 410 ("Issues has been disabled in this repository").
These are two user-confirmed Windows native port bugs, tracked in git and PR #1.

## DS-WIN-001: both splash screens upside down

- **Observed:** EA splash and Dead Space splash show upside down in the
  user's Windows x86-64 build.
- **Cause:** original IM-M3G Image2D rows are presented unchanged by SDL's
  top-origin 2D rendering path. The source data is bottom-origin.
- **Implementation:** the shared helper `original_splash_render_upright`
  uses SDL_RenderCopyEx with SDL_FLIP_VERTICAL. The original decode bytes,
  menu OpenGL textures and external M3G parser remain untouched.
- **Regression:** `ds_original_splash_upright_rgba_rows` draws synthetic
  blue/yellow and red/green asymmetric rows through the production helper.
- **State:** code fix; require green CI and user's Windows visual retest.

## DS-WIN-002: 3D menu cycles all animations before button input

- **Observed:** 3D animation sequences play without activating their
  corresponding 2D UI controls.
- **Cause:** production set `preview_activate_controllers=true`.
  That bypasses JSR-184 controller active_start/active_end times.
- **Implementation:** default and production runtime both set this flag
  false. This restores original serialized timeline gating. Any
  legitimate ambient/auto-active controller continues to run.
- **Regression:** `ds_original_menu_native_keyframe_timeline` now verifies
  active ambient and inactive action controllers side by side.
- **Boundary:** UI 2D action-to-3D controller mapping is **not yet known**;
  do not invent mappings. Full activation requires original evidence.
- **State:** code fix; require green CI and user's Windows visual retest.

No original copyrighted game assets included.
