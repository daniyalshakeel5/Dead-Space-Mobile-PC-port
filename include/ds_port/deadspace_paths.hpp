#pragma once
#include <filesystem>
#include <string>
#include <string_view>
namespace ds_port {
// Portable file-layout adapter based on path conventions reported by
// v-atamanenko/deadspace-vita (MIT). Reimplementation, not Vita loader code.
enum class GamePathStatus { Resolved, UnsupportedPath, UnsafePath, MissingFile };
struct GamePathResult {
    GamePathStatus status = GamePathStatus::UnsupportedPath;
    std::filesystem::path path;
};
// root is the output of install_apk_assets, containing assets/.
// Accepts legacy Android/appbundle prefixes and assets/published data.
// Never permits traversal, host-absolute paths or symlink traversal.
GamePathResult resolve_game_asset_path(const std::filesystem::path& root,
    std::string_view game_path, bool require_regular_file = false);
struct DeadSpaceAssetLayout {
    bool root_found = false;
    bool assets_found = false;
    bool published_found = false;
    bool achievements_found = false;
    std::string diagnostic;
};
// The marker is established for the Xperia Play edition only, so
// its absence does not mean another edition's APK is invalid.
DeadSpaceAssetLayout inspect_deadspace_assets(const std::filesystem::path& root);
// Check the exact original resources required to enter the existing
// reconstructed EA splash -> 3D menu boot path. Never use just an
// unrelated APK marker to claim the installation is reusable.
bool has_original_native_menu_boot_assets(const std::filesystem::path& root);
}
