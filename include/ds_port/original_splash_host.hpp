#pragma once
#include <filesystem>
namespace ds_port {
enum class OriginalSplashResult {
    Completed, MissingOriginalAssets, ImageDecodeFailed, GraphicsUnavailable,
    UserAborted
};
// Present the actual EA and Dead Space texturepacks from the user's
// installed original APK. This host graphics path does not assert the
// original game's timer/menu transition routines are complete.
OriginalSplashResult present_original_splashes(
    const std::filesystem::path& installed_asset_root);
}
