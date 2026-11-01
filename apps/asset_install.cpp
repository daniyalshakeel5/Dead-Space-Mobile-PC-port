#include "ds_port/asset_install.hpp"
#include <iostream>
#include <string>

// Asset setup only. No original code/assets bundled and no gameplay.
int main(int argc, char** argv) {
    std::string apk;
    std::string output;
    if (argc == 3) {
        apk = argv[1];
        output = argv[2];
    } else if (argc == 1) {
        std::cout << "Path to your own Dead Space Mobile APK: ";
        if (!std::getline(std::cin, apk)) return 1;
        std::cout << "New empty destination path (must NOT exist yet): ";
        if (!std::getline(std::cin, output)) return 1;
        const auto strip_quotes = [](std::string& text) {
            if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
                text = text.substr(1, text.size() - 2);
        };
        strip_quotes(apk);
        strip_quotes(output);
    } else {
        std::cerr << "Usage: ds_asset_install <your.apk> <new-output-folder>\n";
        return 1;
    }
    const auto result = ds_port::install_apk_assets(apk, output);
    if (!result.success) {
        std::cerr << "Asset installation rejected: " << result.error << '\n'
                  << "The APK remains unchanged. Do not copy game assets into a public repo.\n";
        return 2;
    }
    std::cout << "Installed " << result.assets_installed
              << " private asset files (" << result.bytes_installed
              << " bytes) into: " << output << '\n'
              << "This is not a game or a game asset decoder.\n";
    return 0;
}
