#include "ds_port/original_frame_dispatch.hpp"
#include "ds_port/original_secondary_pipeline.hpp"
#include "ds_port/original_object_constructor.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <string_view>
namespace {
enum class StartupStatus : int {
    NeedRecoveredStageZero = 20,
    NeedRecoveredVirtualAllocator = 21,
    NeedRecoveredRuntimeService = 22
};
int status_code(StartupStatus s) { return static_cast<int>(s); }
}
int main(int argc, char** argv) {
    bool readiness=false;
    for(int i=1;i<argc;++i) {
        const std::string_view arg(argv[i]);
        if(arg=="--check-native" || arg=="--status") readiness=true;
        else if(arg=="--help") {
            std::cout<<"Dead Space Mobile native desktop port (work in progress)\n"
                     <<"Usage: deadspace_native [--check-native|--status]\n"
                     <<"No original APK or game assets are bundled.\n";
            return 0;
        } else {
            std::cerr<<"Unknown argument: "<<arg<<"\n";
            return 2;
        }
    }
    // IMPORTANT: no fabricated state transition, fake gameplay loop or
    // artificial allocator callback. Original ARM stage zero requires
    // a live instance of 0x27F790 and downstream recovered engine calls.
    std::array<std::uint8_t,16> state{};
    const auto probe=ds_port::original_runtime::execute_original_frame_prefix(
        state.data(),state.size(),nullptr);
    if(probe!=ds_port::original_runtime::FramePrefixResult::MissingOriginalInitRoutine){
        std::cerr<<"Unexpected native startup contract result.\n";
        return 3;
    }
    std::cerr<<"Native x86-64 executable loaded. Original startup is BLOCKED.\n"
             <<"Required: complete original 0x27F790 stage-zero service initialization.\n"
             <<"Required: actual original registry virtual-service methods and "
               "0x281D54 allocator implementation.\n"
             <<"Required: original renderer, world, player control and game update.\n"
             <<"No Android emulation, ARM translation or dummy engine is running.\n";
    if(readiness)std::cerr<<"Readiness: INCOMPLETE (not a playable game).\n";
    return status_code(StartupStatus::NeedRecoveredStageZero);
}
