#include "ds_port/original_frame_dispatch.hpp"
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
constexpr int kBadArguments=2;
}
int main(int argc, char** argv) {
    bool readiness=false;
    std::string user_apk;
    std::string private_asset_dir;
    for(int i=1;i<argc;++i) {
        const std::string_view arg(argv[i]);
        if(arg=="--check-native" || arg=="--status") readiness=true;
        else if(arg=="--apk" && i+1<argc) user_apk=argv[++i];
        else if(arg=="--assets-dir" && i+1<argc) private_asset_dir=argv[++i];
        else if(arg=="--help") {
            std::cout<<"Dead Space Mobile native x86-64 desktop port (incomplete)\n"
                     <<"Usage: deadspace_native [--apk PATH --assets-dir NEW_EMPTY_DIR] [--status]\n"
                     <<"Original game files are required; no APK or assets are bundled.\n"
                     <<"The native gameplay engine is NOT ready.\n";
            return 0;
        } else {
            std::cerr<<"Unknown/incomplete argument: "<<arg<<"\n";
            return kBadArguments;
        }
    }
    if(user_apk.empty()!=private_asset_dir.empty()) {
        std::cerr<<"To import your APK, supply both --apk and --assets-dir.\n";
        return kBadArguments;
    }
    if(!user_apk.empty()) {
#ifdef DS_NATIVE_APK_IMPORT
        const auto result=ds_port::install_apk_assets(user_apk,private_asset_dir);
        if(!result.success) {
            std::cerr<<"Private asset import failed: "<<result.error<<"\n";
            return 4;
        }
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
