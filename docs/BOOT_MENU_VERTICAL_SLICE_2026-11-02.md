# Dead Space Mobile — Original splash and main-menu integration milestone

**November 2, 2026. Target: actual original images and interactive menu, not a placeholder.**

## Acceptance criteria

The Windows/Linux native PC build must open a real graphics window, display the original EA splash followed by the original Dead Space splash using the user's locally installed assets, construct the original 3D main-menu scene with correct textures, camera and animation, and make original menu selections respond to input. New Game/Continue must eventually reach the original level-loader path. This milestone proves essential shared engine systems, not automatically the entire gameplay engine.

## Exact original game resources

Evidence: original APK [asset inventory run 41343765404](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41343765404), [native texture tests 41343946997](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41343946997), [IM-M3G menu geometry run 41344354016](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41344354016).

| Original path under assets/ | Native status |
| --- | --- |
| published/texturepacks_ui/splash_ea.m3g | Valid proprietary IM-M3G with one type-10 image object. Image still undecoded |
| published/texturepacks_ui/splash_deadspace.m3g | Valid proprietary IM-M3G with one type-10 image object. Image still undecoded |
| published/texturepacks_ui/ui_frontend.m3g | Valid proprietary IM-M3G with two type-10 image objects. UI atlas still undecoded |
| published/models/main_menu.m3g | Existing original-asset-tested native parser decodes 52 meshes, 55 submeshes, 18,211 vertices, 9,750 triangles; 32 animation tracks are in the object table |
| published/models/main_menu.bin | Present (1,599 bytes); semantics need original code verification |
| published/textures/texture_env_main_menu_01.m3g | Original 1024×1024 RGBA pixels decode natively |
| published/textures/texture_env_main_menu_objects.m3g | Original 1024×1024 RGBA pixels decode natively |
| published/textures/texture_env_main_menu_objects_02.m3g | Original 1024×1024 ETC1 pixels decode natively |
| published/textures/texture_env_main_menu_starfield.m3g | Original 1024×1024 ETC1 pixels decode natively |

The two splash files and frontend pack use IM-M3G object framing, NOT the standard JSR-184-compatible texture resource layout. Do not apply the regular-texture Image2D offset 0x60 without proof. The 52 successfully parsed menu meshes currently have only positions and triangle indices; UVs, material links, scene transforms, animation rigs and camera are not recovered.

## Development order

1. **Recover proprietary splash/UI Image2D.** Trace the original native texture-pack loader from the APK. Implement bounded original-image decode in portable C++ and validate against the actual asset on the GitHub runner without distributing decoded images.
2. **Display real original splash images.** Integrate user-owned, freshly decoded splash images into a real SDL2/OpenGL window and original startup sequencing. A generic splash image, screenshot or diagnostic substitute does not count.
3. **Reconstruct main-menu scene presentation.** Use actual decoded geometry and menu textures. Recover original UV/Appearance references, transforms, camera, materials, animation timing and render order. Then render the recognizable original menu.
4. **Add original interactive frontend and state transitions.** Decode the frontend atlas, wire original menu selection and input functions, prove navigation and the original Start/Continue-to-level-loader transition. Gameplay comes afterward.

Keep the actual engine source, not another model viewer. A completed milestone requires BOTH Windows and Linux showing the original two splashes and a recognizable, properly textured and interactive original main menu. Nothing in the current code does this yet.

## Guardrails

Development branch: work/deadspace-remote-preflight-2026-11-01. Existing PR #1. Heavy APK work stays on GitHub Actions; no ARM emulator or translation; use only the user's own original assets; never commit game assets. Do not assert that decoding model geometry equals game boot.
