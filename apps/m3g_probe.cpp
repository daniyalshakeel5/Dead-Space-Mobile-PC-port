#include "ds_port/m3g_container.hpp"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

// Diagnostic ONLY: reads one authorized, locally installed .m3g file,
// prints metadata counts; never writes original content or decodes models.
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: ds_m3g_probe <local .m3g file>\n";
        return 2;
    }
    std::ifstream file(argv[1], std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Cannot open input (no file name printed)\n";
        return 2;
    }
    const auto size = file.tellg();
    if (size < 0 || static_cast<std::uint64_t>(size) > 128ull * 1024ull * 1024ull) {
        std::cerr << "Input exceeds safe per-file limit\n";
        return 2;
    }
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    file.seekg(0);
    if (!data.empty() &&
        !file.read(reinterpret_cast<char*>(data.data()), size)) {
        std::cerr << "Could not read input file\n";
        return 2;
    }
    auto result = ds_port::scan_m3g_container(data.data(), data.size());
    std::cout << "{\"valid\":" << (result.valid ? "true" : "false")
              << ",\"sections\":" << result.section_count
              << ",\"compressed_sections\":" << result.compressed_sections
              << ",\"objects\":" << result.object_count
              << ",\"reason\":\"" << result.error << "\"}\n";
    return 0; // Invalid source content is data for the research tally.
}
