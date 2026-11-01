#include "ds_port/deadspace_paths.hpp"
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
namespace ds_port {
namespace {
namespace fs=std::filesystem;
bool begins(std::string_view s,std::string_view p) noexcept {
    return s.size()>=p.size() && s.substr(0,p.size())==p;
}
bool safe_part(std::string_view v) noexcept {
    if(v.empty()||v=="."||v=="..")return false;
    for(const unsigned char c:v)
        if(c<0x20||c==0x7f||c=='\\'||c==':')return false;
    return true;
}
bool no_symlink_ancestors(const fs::path& root,
                          const fs::path& candidate) {
    std::error_code ec;
    if(fs::is_symlink(fs::symlink_status(root,ec)))return false;
    auto current=root;
    auto relative=candidate.lexically_relative(root);
    for(const auto& element:relative) {
        current/=element;
        ec.clear();
        if(fs::is_symlink(fs::symlink_status(current,ec)))return false;
    }
    return true;
}
}
GamePathResult resolve_game_asset_path(
    const fs::path& root,std::string_view input,bool check_file) {
    GamePathResult r{};
    if(root.empty()||input.empty()||input[0]=='/' ||
       input.find('\\')!=std::string_view::npos ||
       input.find('\0')!=std::string_view::npos) {
        r.status=GamePathStatus::UnsafePath;return r;
    }
    // Case-sensitive path conventions recovered from the Vita source.
    // Do not carry over the ARM binaries, loader or Vita path rewriting.
    if(begins(input,"appbundle:/"))input.remove_prefix(sizeof("appbundle:/")-1);
    else if(begins(input,"Android/data/com.ea.deadspace/files/"))
        input.remove_prefix(sizeof("Android/data/com.ea.deadspace/files/")-1);
    if(begins(input,"deadspace/assets/"))
        input.remove_prefix(sizeof("deadspace/assets/")-1);
    else if(begins(input,"deadspace/published/"))
        input.remove_prefix(sizeof("deadspace/")-1);
    else if(begins(input,"assets/"))
        input.remove_prefix(sizeof("assets/")-1);
    if(input.empty()) {
        r.status=GamePathStatus::UnsafePath;return r;
    }
    if(input.find(':')!=std::string_view::npos) {
        r.status=GamePathStatus::UnsupportedPath;return r;
    }
    fs::path relative;
    while(!input.empty()){
        const auto end=input.find('/');
        const auto part=input.substr(0,end);
        if(!safe_part(part)){
            r.status=GamePathStatus::UnsafePath;return r;
        }
        relative/=fs::u8path(std::string(part));
        if(end==std::string_view::npos)break;
        input.remove_prefix(end+1);
        if(input.empty()){
            r.status=GamePathStatus::UnsafePath;return r;
        }
    }
    const auto resolved=(root/"assets"/relative).lexically_normal();
    if(!no_symlink_ancestors(root,resolved)){
        r.status=GamePathStatus::UnsafePath;return r;
    }
    if(check_file){
        std::error_code ec;
        if(!fs::is_regular_file(resolved,ec)){
            r.status=GamePathStatus::MissingFile;return r;
        }
    }
    r.status=GamePathStatus::Resolved;r.path=resolved;
    return r;
}
DeadSpaceAssetLayout inspect_deadspace_assets(const fs::path& root){
    DeadSpaceAssetLayout layout{};
    std::error_code ec;
    if(root.empty()||fs::is_symlink(fs::symlink_status(root,ec))||
       !fs::is_directory(root,ec)){
        layout.diagnostic="Private asset installation not found.";return layout;
    }
    layout.root_found=true;
    const auto assets=root/"assets";
    if(fs::is_symlink(fs::symlink_status(assets,ec))||
       !fs::is_directory(assets,ec)){
        layout.diagnostic="Asset installation has no assets/ directory.";return layout;
    }
    layout.assets_found=true;
    const auto published=assets/"published";
    if(!fs::is_symlink(fs::symlink_status(published,ec))&&
       fs::is_directory(published,ec))layout.published_found=true;
    layout.achievements_found=resolve_game_asset_path(
        root,"published/data/achievements.sb",true).status==GamePathStatus::Resolved;
    layout.diagnostic=layout.achievements_found
        ? "Published-data marker present (Xperia Play layout)."
        : "Published-data marker absent; game editions may differ.";
    return layout;
}
}
