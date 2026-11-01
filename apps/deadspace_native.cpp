#include "ds_port/original_frame_dispatch.hpp"
#include "ds_port/deadspace_paths.hpp"
#ifdef DS_NATIVE_SPLASH_PRESENTATION
#include "ds_port/original_splash_host.hpp"
#include "ds_port/im_menu_scene.hpp"
#include "ds_port/original_menu_draw_stream.hpp"
#ifdef DS_NATIVE_ORIGINAL_MENU_GL
#include "ds_port/original_menu_gl.hpp"
#endif
#include "ds_port/im_ui_image.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#endif
#ifdef DS_NATIVE_APK_IMPORT
#include "ds_port/asset_install.hpp"
#endif
#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
namespace {
constexpr int kNeedRecoveredEngine=20;
#ifdef DS_NATIVE_SPLASH_PRESENTATION
bool read_original_bounded(const std::string& root,const char* name,
                           std::vector<std::uint8_t>& data) {
    const auto p=ds_port::resolve_game_asset_path(root,name,true);
    if(p.status!=ds_port::GamePathStatus::Resolved)return false;
    std::ifstream in(p.path,std::ios::binary|std::ios::ate);
    if(!in)return false;
    const auto n=in.tellg();
    if(n<=0 || static_cast<std::uint64_t>(n)>64ull*1024*1024)return false;
    data.resize(static_cast<std::size_t>(n));
    in.seekg(0);
    return bool(in.read(reinterpret_cast<char*>(data.data()),n));
}
// Actual native boot stage after the splashes. This does not render or
// simulate a menu: it establishes whether the original scene AND the
// original frontend resources can be instantiated together, bounded and
// without importing any game bytes into the executable.
bool load_original_menu_stage(const std::string& root) {
    std::vector<std::uint8_t> scene_bytes,ui_bytes;
    if(!read_original_bounded(root,"published/models/main_menu.m3g",scene_bytes) ||
       !read_original_bounded(root,"published/texturepacks_ui/ui_frontend.m3g",ui_bytes))
        return false;
    const auto scene=ds_port::decode_im_menu_geometry(
        scene_bytes.data(),scene_bytes.size());
    const auto ui=ds_port::decode_im_ui_rgba_images(
        ui_bytes.data(),ui_bytes.size());
    if(!scene.decoded || !ui.decoded || ui.images.size()!=2 ||
       scene.mesh_count==0 || scene.surfaces.empty() ||
       scene.camera_object_indices.empty() ||
       scene.group_object_indices.empty())return false;
    const auto draw=ds_port::prepare_original_menu_draw_stream(scene);
    if(!draw.valid)return false;
#ifdef DS_NATIVE_ORIGINAL_MENU_GL
    // The GPU backend is linked to this native game; do not draw until
    // original camera, world matrices and material links are recovered.
    ds_port::OriginalMenuGpuFrame original_frame{};
    std::string missing_original;
    if(ds_port::validate_original_menu_gpu_frame(scene,draw,ui.images,
        original_frame,missing_original))return false;
    std::cerr<<"Original GPU menu blocked: "<<missing_original<<"\\n";
#endif
    for(const auto& img:ui.images)
        if(!img.decoded || img.width!=1024 || img.height!=1024 ||
           img.pixels_rgba.size()!=1024u*1024u*4u)return false;
    std::cerr<<"Original menu stage resource load: "
             <<scene.mesh_count<<" meshes, "<<scene.surfaces.size()
             <<" surfaces, "<<scene.indices.size()/3<<" triangles, "
             <<ui.images.size()<<" original frontend atlases.\\n";
    return true;
}
#endif
constexpr int kBadArguments=2;
}
int main(int argc, char** argv) {
    bool readiness=false;
    bool present_splash=false;
    std::string user_apk;
    std::string private_asset_dir;
    std::string inspected_asset_dir;
    for(int i=1;i<argc;++i) {
        const std::string_view arg(argv[i]);
        if(arg=="--check-native" || arg=="--status") readiness=true;
        else if(arg=="--boot-splash") present_splash=true;
        else if(arg=="--apk" && i+1<argc) user_apk=argv[++i];
        else if(arg=="--assets-dir" && i+1<argc) private_asset_dir=argv[++i];
        else if(arg=="--inspect-assets" && i+1<argc) inspected_asset_dir=argv[++i];
        else if(arg=="--help") {
            std::cout<<"Dead Space Mobile native x86-64 desktop port (incomplete)\n"
                     <<"Usage: deadspace_native [--apk PATH --assets-dir NEW_EMPTY_DIR]\n"
                     <<"       deadspace_native --boot-splash --assets-dir INSTALLED_DIR\n"
                     <<"       deadspace_native --inspect-assets EXISTING_DIR\n"
                     <<"       deadspace_native --status\n"
                     <<"Original game files are required; no APK or assets are bundled.\n"
                     <<"The native gameplay engine is NOT ready.\n";
            return 0;
        } else {
            std::cerr<<"Unknown/incomplete argument: "<<arg<<"\n";
            return kBadArguments;
        }
    }
    if(!inspected_asset_dir.empty() && (!user_apk.empty() || !private_asset_dir.empty())) {
        std::cerr<<"--inspect-assets cannot be combined with an APK import.\n";
        return kBadArguments;
    }
    if(present_splash && (private_asset_dir.empty() ||
                          !user_apk.empty() ||
                          !inspected_asset_dir.empty() || readiness)) {
        std::cerr<<"Use --boot-splash --assets-dir INSTALLED_DIR only.\n";
        return kBadArguments;
    }
    if(!present_splash && user_apk.empty()!=private_asset_dir.empty()) {
        std::cerr<<"To import your APK, supply both --apk and --assets-dir.\n";
        return kBadArguments;
    }
    if(present_splash) {
#ifdef DS_NATIVE_SPLASH_PRESENTATION
        const auto status=ds_port::present_original_splashes(private_asset_dir);
        switch(status){
            case ds_port::OriginalSplashResult::Completed:
                if(!load_original_menu_stage(private_asset_dir)) {
                    std::cerr<<"Original menu scene/frontend resource loading failed.\\n";
                    return 8;
                }
                std::cerr<<"Original EA and Dead Space splash images displayed.\n"
                         <<"Native main-menu renderer still incomplete.\n";
                return kNeedRecoveredEngine;
            case ds_port::OriginalSplashResult::UserAborted: return 0;
            case ds_port::OriginalSplashResult::MissingOriginalAssets:
                std::cerr<<"Original installed splash assets not found.\n";
                return 7;
            case ds_port::OriginalSplashResult::ImageDecodeFailed:
                std::cerr<<"Original splash UI image parsing failed.\n";
                return 8;
            case ds_port::OriginalSplashResult::GraphicsUnavailable:
                std::cerr<<"Could not create native SDL2 splash window.\n";
                return 9;
        }
        return 9;
#else
        std::cerr<<"SDL2 splash presentation disabled. "
                    "Rebuild with SDL2 and -DDS_BUILD_BOOT_SPLASH=ON.\n";
        return 5;
#endif
    }
    if(!inspected_asset_dir.empty()) {
        const auto layout=ds_port::inspect_deadspace_assets(inspected_asset_dir);
        std::cout<<"Installed assets: "<<(layout.assets_found?"found":"missing")<<"\n"
                 <<"Published data: "<<(layout.published_found?"found":"missing")<<"\n"
                 <<"Xperia Play marker: "<<(layout.achievements_found?"present":"absent")<<"\n"
                 <<layout.diagnostic<<"\n";
        return layout.assets_found?0:7;
    }
    if(!user_apk.empty()) {
#ifdef DS_NATIVE_APK_IMPORT
        const auto result=ds_port::install_apk_assets(user_apk,private_asset_dir);
        if(!result.success) {
            std::cerr<<"Private asset import failed: "<<result.error<<"\n";
            return 4;
        }
        const auto layout=ds_port::inspect_deadspace_assets(private_asset_dir);
        std::cout<<"Asset layout: "<<layout.diagnostic<<"\n";
        std::cout<<"Imported "<<result.assets_installed<<" private original game files ("
                 <<result.bytes_installed<<" bytes) into "<<private_asset_dir<<"\n"
                 <<"Original files remain local. This does not enable gameplay.\n";
#else
        std::cerr<<"APK import support is disabled in this build. "
                   "Rebuild with -DDS_BUILD_ASSET_INSTALLER=ON.\n";
        return 5;
#endif
    }
    // Original stage-zero 0x27f790 has not been fully reconstructed.
    // Never transition to stage 1 on a missing original game function.
    std::array<std::uint8_t,16> state{};
    const auto probe=ds_port::original_runtime::execute_original_frame_prefix(
        state.data(),state.size(),nullptr);
    if(probe!=ds_port::original_runtime::FramePrefixResult::MissingOriginalInitRoutine) {
        std::cerr<<"Invalid original engine startup contract.\n";
        return 6;
    }
    std::cerr<<"Native executable loaded; gameplay blocked by unrecovered original code.\n"
             <<"Missing original stage-zero registry and allocation services.\n"
             <<"Missing original renderer, world simulation and player update.\n"
             <<"No Android emulator, ARM translation or stand-in game was executed.\n";
    if(readiness)std::cerr<<"Readiness: INCOMPLETE.\n";
    return kNeedRecoveredEngine;
}
