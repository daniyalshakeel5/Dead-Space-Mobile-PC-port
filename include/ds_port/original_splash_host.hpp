#pragma once
#include <filesystem>
// SDL types intentionally forward-declared here. Including SDL.h in
// the native game's public header redefines main to SDL_main on
// Windows and prevents the executable's actual entry point from linking.
struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Rect;
namespace ds_port {
enum class OriginalSplashResult {
    Completed, MissingOriginalAssets, ImageDecodeFailed, GraphicsUnavailable,
    UserAborted
};
// Shared SDL presentation operation; tested with asymmetric RGBA rows.
bool original_splash_render_upright(
    SDL_Renderer* renderer,SDL_Texture* texture,
    const SDL_Rect& destination) noexcept;
// Present the actual EA and Dead Space texturepacks from the user's
// installed original APK. This host graphics path does not assert the
// original game's timer/menu transition routines are complete.
OriginalSplashResult present_original_splashes(
    const std::filesystem::path& installed_asset_root);
}
