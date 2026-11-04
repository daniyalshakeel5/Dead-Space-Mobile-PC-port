// Read-only metadata intake. This does not unpack, launch or rebuild the game.
#include "ds_port/apk_probe.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::string input;
    if (argc > 1) {
        input = argv[1];
    } else {
        std::cout << "Path to your own Dead Space Mobile APK: ";
        if (!std::getline(std::cin, input) || input.empty()) {
            std::cerr << "No APK selected.\n";
            return 1;
        }
        if (input.size() > 1 && input.front() == '"' && input.back() == '"')
            input = input.substr(1, input.size() - 2);
    }
    const auto result = ds_port::probe_apk(input);
    if (!result.valid_archive) {
        std::cerr << "Cannot inspect APK: " << result.error << "\n";
        return 2;
    }
    if (!result.looks_like_android_apk) {
        std::cerr << "Not an Android APK: " << result.error << "\n";
        return 3;
    }
    std::cout << "Android APK archive metadata (ZIP32 validation only)\n"
              << "Entries: " << result.entries << "\n"
              << "Assets entries: " << result.assets << "\n"
              << "Native libraries: " << result.native_libraries << "\n"
              << "DEX entries: " << result.dex_files << "\n"
              << "libDeadSpace.so filename hint: "
              << (result.has_deadspace_library_name ? "present" : "not found") << "\n"
              << "Declared uncompressed bytes: "
              << result.declared_uncompressed_bytes << "\n"
              << "No game data extracted; this is NOT a playable PC port.\n";
    return 0;
}
