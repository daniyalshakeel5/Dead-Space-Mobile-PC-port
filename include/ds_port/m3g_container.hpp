#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace ds_port {

// JSR-184 M3G v1.0 container/object-chunk inspection ONLY.
// Does not interpret model geometry, textures, animations or references.
// Input bytes come from the user's own APK at runtime; no game data shipped.
struct M3gContainerReport {
    bool valid = false;
    std::string error;
    std::uint32_t section_count = 0;
    std::uint32_t object_count = 0;
    std::uint32_t compressed_sections = 0;
    bool has_external_references = false;
    std::array<std::uint32_t, 256> object_type_count{};
};

// Bounds-limited in-memory parser. All integers are LE per JSR-184.
// Section Adler32 and zlib compression scheme 1 validated when present.
// Use at most one file at a time; do NOT pass an entire APK into memory.
M3gContainerReport scan_m3g_container(const std::uint8_t* data,
                                     std::size_t size,
                                     std::size_t max_section_bytes = 64u * 1024u * 1024u);
}  // namespace ds_port
