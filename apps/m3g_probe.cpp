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
    if (argc != 2 && argc != 3) {
        std::cerr << "usage: ds_m3g_probe <local .m3g file> [--diagnostic]\n";
        return 2;
    }
    const bool diagnostic = argc == 3 && std::string(argv[2]) == "--diagnostic";
    if (argc == 3 && !diagnostic) return 2;
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
    auto result = ds_port::scan_m3g_container(
        data.data(), data.size(), 64u * 1024u * 1024u,
        diagnostic ? ds_port::M3gScanMode::DiagnosticOpaqueVariant
                   : ds_port::M3gScanMode::StrictJsr184);
    std::cout << "{\"valid\":" << (result.valid ? "true" : "false")
              << ",\"sections\":" << result.section_count
              << ",\"compressed_sections\":" << result.compressed_sections
              << ",\"objects\":" << result.object_count
              << ",\"diagnostic\":" << (diagnostic ? "true" : "false")
              << ",\"nonstandard_header_objects\":" << result.nonstandard_header_objects
              << ",\"reserved_type_objects\":" << result.reserved_type_objects
              << ",\"image2d_objects\":" << result.image2d_objects
              << ",\"image2d_standard_payloads\":" << result.image2d_standard_payloads
              << ",\"image2d_unrecognized_payloads\":" << result.image2d_unrecognized_payloads
              << ",\"image2d_max_width\":" << result.image2d_max_width
              << ",\"image2d_max_height\":" << result.image2d_max_height
              << ",\"image2d_validated_pixel_bytes\":" << result.image2d_validated_pixel_bytes
              << ",\"reason\":\"" << result.error << "\""
              << ",\"type_counts\":{";
    bool first = true;
    for (std::size_t i = 0; i < result.object_type_count.size(); ++i) {
        if (result.object_type_count[i] == 0) continue;
        if (!first) std::cout << ",";
        first = false;
        std::cout << "\"" << i << "\":" << result.object_type_count[i];
    }
    std::cout << "},\"image2d_formats\":{";
    first = true;
    for (std::size_t i = 0; i < result.image2d_format_counts.size(); ++i) {
        if (!result.image2d_format_counts[i]) continue;
        if (!first) std::cout << ",";
        first = false;
        std::cout << "\"" << (i + 96) << "\":" << result.image2d_format_counts[i];
    }
    std::cout << "}}\n";
    return 0; // Invalid source content is data for the research tally.
}
