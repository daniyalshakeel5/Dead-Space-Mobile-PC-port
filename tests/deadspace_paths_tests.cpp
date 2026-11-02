#include "ds_port/deadspace_paths.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
namespace fs=std::filesystem;
#define CHECK(cond) do { if(!(cond)){std::cerr<<"FAILED line "<<__LINE__<<": "<<#cond<<"\n";return 1;}}while(false)
int main(){
    const auto root=fs::temp_directory_path()/
        ("deadspace_native_paths_"+std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root/"assets"/"published"/"data");
    fs::create_directories(root/"assets"/"textures");
    {std::ofstream(root/"assets"/"published"/"data"/"achievements.sb")<<"test";}
    {std::ofstream(root/"assets"/"textures"/"mesh.m3g")<<"fixture";}
    const auto valid=[&](std::string_view key,fs::path expected){
        auto x=ds_port::resolve_game_asset_path(root,key,true);
        return x.status==ds_port::GamePathStatus::Resolved &&
               x.path==root/"assets"/expected;
    };
    CHECK(valid("published/data/achievements.sb","published/data/achievements.sb"));
    CHECK(valid("assets/published/data/achievements.sb","published/data/achievements.sb"));
    CHECK(valid("deadspace/published/data/achievements.sb","published/data/achievements.sb"));
    CHECK(valid("appbundle:/published/data/achievements.sb","published/data/achievements.sb"));
    CHECK(valid("Android/data/com.ea.deadspace/files/deadspace/published/data/achievements.sb","published/data/achievements.sb"));
    CHECK(valid("appbundle:/textures/mesh.m3g","textures/mesh.m3g"));
    CHECK(valid("assets/textures/mesh.m3g","textures/mesh.m3g"));
    for(const auto* bad:{"appbundle:/../bad","assets/../bad","assets//bad",
                         "C:\\Windows\\win.ini","/etc/passwd","assets/a/./b",
                         "assets/name:"}) {
        auto status=ds_port::resolve_game_asset_path(root,bad).status;
        CHECK(status!=ds_port::GamePathStatus::Resolved);
    }
    CHECK(ds_port::resolve_game_asset_path(root,"assets/missing.m3g",true)
          .status==ds_port::GamePathStatus::MissingFile);
    auto state=ds_port::inspect_deadspace_assets(root);
    CHECK(state.root_found&&state.assets_found&&state.published_found&&state.achievements_found);
    fs::remove(root/"assets"/"published"/"data"/"achievements.sb");
    CHECK(!ds_port::inspect_deadspace_assets(root).achievements_found);
    std::error_code ec;
    fs::create_symlink(root/"assets"/"textures"/"mesh.m3g",
        root/"assets"/"published"/"data"/"achievements.sb",ec);
    if(!ec)CHECK(ds_port::resolve_game_asset_path(
        root,"published/data/achievements.sb",true).status==ds_port::GamePathStatus::UnsafePath);
    // Boot reuse requires the exact splash, menu, layout and UI
    // assets, rather than a marker from one particular APK edition.
    fs::remove(root/"assets"/"published"/"data"/"achievements.sb",ec);
    fs::create_directories(root/"assets"/"published"/"texturepacks_ui");
    fs::create_directories(root/"assets"/"published"/"layoutdata");
    fs::create_directories(root/"assets"/"published"/"models");
    CHECK(!ds_port::has_original_native_menu_boot_assets(root));
    for(const auto* name:{
        "texturepacks_ui/splash_ea.m3g",
        "texturepacks_ui/splash_deadspace.m3g",
        "texturepacks_ui/ui_frontend.m3g",
        "layoutdata/layouts.xml.sb",
        "models/main_menu.m3g"}){
        std::ofstream(root/"assets"/"published"/fs::u8path(name),
                      std::ios::binary)<<"test";
    }
    CHECK(ds_port::has_original_native_menu_boot_assets(root));
    fs::remove(root/"assets"/"published"/"models"/"main_menu.m3g");
    CHECK(!ds_port::has_original_native_menu_boot_assets(root));
    fs::remove_all(root);
    return 0;
}
