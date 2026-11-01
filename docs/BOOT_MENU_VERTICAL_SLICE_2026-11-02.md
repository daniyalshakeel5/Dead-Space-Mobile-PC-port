# Dead Space Mobile — Original splash and main-menu integration milestone

**November 2, 2026. Target: actual original images and interactive menu, not a placeholder.**

## Acceptance criteria

The Windows/Linux native PC build must open a real graphics window, display the original EA splash followed by the original Dead Space splash using the user's locally installed assets, construct the original 3D main-menu scene with correct textures, camera and animation, and make original menu selections respond to input. New Game/Continue must eventually reach the original level-loader path. This milestone proves essential shared engine systems, not automatically the entire gameplay engine.

## Exact original game resources

Evidence: original APK [asset inventory run 41343765404](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41343765404), [native texture tests 41343946997](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41343946997), [IM-M3G menu geometry run 41344354016](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41344354016).

| Original path under assets/ | Native status |
| --- | --- |
| published/texturepacks_ui/splash_ea.m3g | Original native RGBA Image2D decode succeeds (256×256) and is presented by native SDL2 splash stage |
| published/texturepacks_ui/splash_deadspace.m3g | Original native RGBA Image2D decode succeeds (512×128) and is presented by native SDL2 splash stage |
| published/texturepacks_ui/ui_frontend.m3g | Both original 1024×1024 RGBA frontend images decode natively; UI positioning/behavior not yet mapped |
| published/models/main_menu.m3g | Original native scene buffers now decode 52 meshes, 55 surfaces, 18,211 XYZ+UV vertices, 9,750 triangles, and Appearance references; camera, scene transforms, materials and 32 animation tracks still need reconstruction |
| published/models/main_menu.bin | Present (1,599 bytes); semantics need original code verification |
| published/textures/texture_env_main_menu_01.m3g | Original 1024×1024 RGBA pixels decode natively |
| published/textures/texture_env_main_menu_objects.m3g | Original 1024×1024 RGBA pixels decode natively |
| published/textures/texture_env_main_menu_objects_02.m3g | Original 1024×1024 ETC1 pixels decode natively |
| published/textures/texture_env_main_menu_starfield.m3g | Original 1024×1024 ETC1 pixels decode natively |

The splash and frontend packs use original IM-M3G object framing rather than standard JSR-184 container framing, but their type-10 Image2D objects have a verified RGBA payload after variable-length user metadata. The 52 meshes have XYZ and UV arrays as well as the original Appearance index for each of 55 surfaces. Their scene transforms, material-to-texture bindings, original camera, animation rigs, and menu behavior are not yet reconstructed.


## Actual native boot checkpoint (November 2)

The original EA (256×256) and Dead Space (512×128) splash packs
decode as owned RGBA pixels through independent portable
`src/im_ui_image.cpp`. The original frontend UI file yields
**two 1024×1024 RGBA images**. The native decoder was run on
the exact APK in [CI 41345210133](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41345210133).

`deadspace_native --boot-splash --assets-dir INSTALLED_DIR`
now opens a real native **SDL2 window** with the original EA
then original Dead Space splash textures, then exits at the
explicit incomplete-main-menu boundary (exit code **20**).
It never shows fabricated logo graphics or a replacement
game menu. The current host display duration/fades are
provisional, not claimed original engine timing. Escape
exits, and Enter, Space or mouse click skips to the next image.

Native automated original-APK [headless SDL2 boot CI 41345686409](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41345686409)
has verified the actual splash images load through SDL2 and
the unfinished original menu gate is reached.
This headless pass does **not** verify visual orientation on a
physical desktop. Windows/Linux compile and unit-test
checks are separate.

The original `models/main_menu.m3g` was also decoded
as a typed native scene buffer by `src/im_menu_scene.cpp`:
**52 meshes, 55 surfaces, 18,211 XYZ+UV vertices, 9,750
triangles**, original Appearance references retained. All
52 referenced UV arrays use 2D signed-int16 coordinates.
[Actual APK geometry+UV run 41346451467](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41346451467).
This is **not yet a rendered menu**: the original camera,
world transforms and Appearance→Texture2D bindings need
verification. No main-menu inputs have been wired yet.

### Local native boot test

Install the original APK assets locally with the project's
`ds_asset_install` program (separate asset-installer CI).
The resulting directory must contain
`assets/published/texturepacks_ui/splash_ea.m3g` and
`assets/published/texturepacks_ui/splash_deadspace.m3g`.
Then use the **SDL2-enabled native executable** from
the desktop CI, not the SDL-free core variant:

```sh
./deadspace_native --boot-splash --assets-dir /path/to/installed-assets
```

On Windows:
```powershell
.\deadspace_native.exe --boot-splash --assets-dir "D:\Games\DeadSpaceAssets"
```

SDL2.dll is packaged beside the Windows executable;
Linux requires SDL2 system runtime. Exit status 20
after the two splashes means **the original game menu
engine is not implemented yet**, not a successful game
boot. Missing assets or unsupported images return a
distinct error.

## Development order

1. **Completed — decode proprietary splash/UI Image2D.** The original images decode in portable C++ and are verified on the APK runner; no image data is distributed.
2. **Completed — show original splash images through SDL2.** Original user-owned EA and Dead Space pixels display in the native executable; original startup timing is not yet verified.
3. **In progress — main-menu scene presentation.** Original geometry/UV/Appearance references are decoded. Recover original scene-node transforms, camera, material-to-Texture2D bindings, animations and render order, then render the recognizable original menu.
4. **Add original interactive frontend and state transitions.** Decode the frontend atlas, wire original menu selection and input functions, prove navigation and the original Start/Continue-to-level-loader transition. Gameplay comes afterward.

Keep the actual engine source, not another model viewer. A completed milestone requires BOTH Windows and Linux showing the original two splashes and a recognizable, properly textured and interactive original main menu. Nothing in the current code does this yet.

## Guardrails

Development branch: work/deadspace-remote-preflight-2026-11-01. Existing PR #1. Heavy APK work stays on GitHub Actions; no ARM emulator or translation; use only the user's own original assets; never commit game assets. Do not assert that decoding model geometry equals game boot.
