#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace ds_port {

// JSR-184 M3G v1.0 container/object-chunk inspection ONLY.
// Does not interpret model geometry, textures, animations or references.
// Input bytes come from the user's own APK at runtime; no game data shipped.
enum class M3gScanMode {
    StrictJsr184,
    DiagnosticOpaqueVariant // Structural evidence only. NOT gameplay decoding.
};

struct M3gContainerReport {
    bool valid = false;
    std::string error;
    std::uint32_t section_count = 0;
    std::uint32_t object_count = 0;
    std::uint32_t compressed_sections = 0;
    bool has_external_references = false;
    bool diagnostic_mode = false;
    std::uint32_t nonstandard_header_objects = 0;
    std::uint32_t reserved_type_objects = 0;
    // JSR-184 Image2D payload subparser evidence, never raw pixel bytes.
    std::uint32_t image2d_objects = 0;
    std::uint32_t image2d_standard_payloads = 0;
    std::uint32_t image2d_unrecognized_payloads = 0;
    std::uint32_t image2d_max_width = 0;
    std::uint32_t image2d_max_height = 0;
    std::uint64_t image2d_validated_pixel_bytes = 0;
    std::array<std::uint32_t, 5> image2d_format_counts{}; // format 96..100

    std::array<std::uint32_t, 256> object_type_count{};
};

// Bounds-limited in-memory parser. All integers are LE per JSR-184.
// Section Adler32 and zlib compression scheme 1 validated when present.
// Use at most one file at a time; do NOT pass an entire APK into memory.
M3gContainerReport scan_m3g_container(const std::uint8_t* data,
                                     std::size_t size,
                                     std::size_t max_section_bytes = 64u * 1024u * 1024u,
                                     M3gScanMode mode = M3gScanMode::StrictJsr184);
}  // namespace ds_port
