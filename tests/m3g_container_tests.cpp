#include "ds_port/m3g_container.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <zlib.h>

#define REQUIRE(x) do { if (!(x)) { \
    std::cerr << "M3G fixture failure at line " << __LINE__ << ": " << #x << '\n'; \
    return 1; } } while (0)

using Bytes = std::vector<std::uint8_t>;
void put32(Bytes& v, std::uint32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back(static_cast<std::uint8_t>((x >> (i * 8)) & 255));
}
void set32(Bytes& v, std::size_t off, std::uint32_t x) {
    for (int i = 0; i < 4; ++i) v[off+i] = static_cast<std::uint8_t>((x >> (i * 8)) & 255);
}
Bytes make_section(const Bytes& objects, bool compress) {
    Bytes body = objects;
    if (compress) {
        uLongf target_size = compressBound(static_cast<uLong>(objects.size()));
        body.resize(static_cast<std::size_t>(target_size));
        if (compress2(body.data(), &target_size, objects.data(),
                      static_cast<uLong>(objects.size()), Z_BEST_SPEED) != Z_OK)
            throw std::runtime_error("fixture compression");
        body.resize(static_cast<std::size_t>(target_size));
    }
    Bytes section{static_cast<std::uint8_t>(compress ? 1 : 0)};
    put32(section, static_cast<std::uint32_t>(13 + body.size()));
    put32(section, static_cast<std::uint32_t>(objects.size()));
    section.insert(section.end(), body.begin(), body.end());
    const auto crc = adler32(adler32(0L, Z_NULL, 0), section.data(),
                            static_cast<uInt>(section.size()));
    put32(section, static_cast<std::uint32_t>(crc));
    return section;
}
Bytes make_file(bool compressed_scene) {
    const Bytes identifier{0xab,'J','S','R','1','8','4',0xbb,13,10,26,10};
    // Header object: type 0, payload version 1.0, no external refs,
    // total length (patched later), approximate size, empty author.
    Bytes header_obj{0}; put32(header_obj, 12);
    header_obj.insert(header_obj.end(), {1,0,0});
    put32(header_obj, 0); put32(header_obj, 0); header_obj.push_back(0);
    Bytes scene_obj{14}; put32(scene_obj, 0);  // synthetic Mesh object chunk
    const Bytes scene_section = make_section(scene_obj, compressed_scene);
    Bytes header_section = make_section(header_obj, false);
    const auto length = static_cast<std::uint32_t>(
        identifier.size() + header_section.size() + scene_section.size());
    set32(header_obj, 8, length); // 5-byte object header + 3-byte version/flag
    set32(header_obj, 12, length);
    header_section = make_section(header_obj, false);
    Bytes file = identifier;
    file.insert(file.end(), header_section.begin(), header_section.end());
    file.insert(file.end(), scene_section.begin(), scene_section.end());
    return file;
}
std::uint32_t get32(const Bytes& v, std::size_t at) {
    return std::uint32_t(v[at]) | (std::uint32_t(v[at+1]) << 8) |
           (std::uint32_t(v[at+2]) << 16) | (std::uint32_t(v[at+3]) << 24);
}
Bytes replace_scene_type(std::uint8_t scene_type) {
    Bytes file = make_file(false);
    const auto next = std::size_t(12) + get32(file, 13);
    file[next + 9] = scene_type;
    const auto n = get32(file, next + 1);
    const auto sum = adler32(adler32(0L, Z_NULL, 0),
                             file.data() + next, static_cast<uInt>(n - 4));
    set32(file, next + n - 4, static_cast<std::uint32_t>(sum));
    return file;
}
Bytes make_image_file(bool bad_length) {
    const Bytes magic{0xab,'J','S','R','1','8','4',0xbb,13,10,26,10};
    Bytes payload;
    put32(payload, 0); // Object3D userID
    put32(payload, 0); // animation track count
    put32(payload, 0); // user parameter count
    payload.push_back(100); // RGBA
    payload.push_back(0);   // immutable
    put32(payload, 2); put32(payload, 2);
    put32(payload, 0); // empty palette
    put32(payload, bad_length ? 15 : 16);
    payload.insert(payload.end(), bad_length ? 15 : 16, 0x7f);
    Bytes objects{0};
    put32(objects, 0); // nonstandard second header, diagnostic only
    objects.push_back(10);
    put32(objects, static_cast<std::uint32_t>(payload.size()));
    objects.insert(objects.end(), payload.begin(), payload.end());
    Bytes second = make_section(objects, false);
    Bytes header{0}; put32(header, 12);
    header.insert(header.end(), {1, 0, 0});
    put32(header, 0); put32(header, 0); header.push_back(0);
    Bytes first = make_section(header, false);
    const auto total = static_cast<std::uint32_t>(
        magic.size() + first.size() + second.size());
    set32(header, 8, total);
    set32(header, 12, total);
    first = make_section(header, false);
    Bytes result = magic;
    result.insert(result.end(), first.begin(), first.end());
    result.insert(result.end(), second.begin(), second.end());
    return result;
}

int main() {
    {
        const auto hybrid = replace_scene_type(0);
        const auto strict = ds_port::scan_m3g_container(hybrid.data(), hybrid.size());
        REQUIRE(!strict.valid);
        REQUIRE(strict.error == "unexpected second M3G header object");
        const auto diag = ds_port::scan_m3g_container(
            hybrid.data(), hybrid.size(), 64u * 1024u * 1024u,
            ds_port::M3gScanMode::DiagnosticOpaqueVariant);
        REQUIRE(diag.valid);
        REQUIRE(diag.diagnostic_mode);
        REQUIRE(diag.nonstandard_header_objects == 1);
        REQUIRE(diag.object_type_count[0] == 2);
        REQUIRE(diag.section_count == 2);
    }
    {
        const auto extended = replace_scene_type(23);
        const auto strict = ds_port::scan_m3g_container(extended.data(), extended.size());
        REQUIRE(!strict.valid);
        REQUIRE(strict.error == "reserved M3G object type");
        const auto diag = ds_port::scan_m3g_container(
            extended.data(), extended.size(), 64u * 1024u * 1024u,
            ds_port::M3gScanMode::DiagnosticOpaqueVariant);
        REQUIRE(diag.valid);
        REQUIRE(diag.reserved_type_objects == 1);
        REQUIRE(diag.object_type_count[23] == 1);
    }
    {
        auto valid_image = make_image_file(false);
        auto diagnostic = ds_port::scan_m3g_container(
            valid_image.data(), valid_image.size(),
            64u * 1024u * 1024u, ds_port::M3gScanMode::DiagnosticOpaqueVariant);
        REQUIRE(diagnostic.valid);
        REQUIRE(diagnostic.image2d_objects == 1);
        REQUIRE(diagnostic.image2d_standard_payloads == 1);
        REQUIRE(diagnostic.image2d_unrecognized_payloads == 0);
        REQUIRE(diagnostic.image2d_format_counts[4] == 1);
        REQUIRE(diagnostic.image2d_max_width == 2);
        REQUIRE(diagnostic.image2d_max_height == 2);
        REQUIRE(diagnostic.image2d_validated_pixel_bytes == 16);
        REQUIRE(!ds_port::scan_m3g_container(
            valid_image.data(), valid_image.size()).valid);
        auto malformed = make_image_file(true);
        auto negative = ds_port::scan_m3g_container(
            malformed.data(), malformed.size(),
            64u * 1024u * 1024u, ds_port::M3gScanMode::DiagnosticOpaqueVariant);
        REQUIRE(negative.valid);
        REQUIRE(negative.image2d_objects == 1);
        REQUIRE(negative.image2d_standard_payloads == 0);
        REQUIRE(negative.image2d_unrecognized_payloads == 1);
    }
    for (bool compressed : {false,true}) {
        Bytes file = make_file(compressed);
        auto result = ds_port::scan_m3g_container(file.data(), file.size());
        REQUIRE(result.valid);
        REQUIRE(result.section_count == 2);
        REQUIRE(result.object_count == 2);
        REQUIRE(result.object_type_count[0] == 1);
        REQUIRE(result.object_type_count[14] == 1);
        REQUIRE(result.compressed_sections == static_cast<unsigned>(compressed));
        REQUIRE(!result.has_external_references);

        Bytes bad = file;
        bad[12 + 9 + 5 + 3] ^= 1; // corrupt declared size (Adler invalid as expected)
        REQUIRE(!ds_port::scan_m3g_container(bad.data(), bad.size()).valid);

        bad = file;
        bad[12 + 9 + 5 + 3] ^= 1;
        // Force out-of-bounds section length, no unchecked read.
        bad[12 + 1] = 0xff;
        REQUIRE(!ds_port::scan_m3g_container(bad.data(), bad.size()).valid);

        bad = file;
        bad.resize(bad.size() - 1);
        REQUIRE(!ds_port::scan_m3g_container(bad.data(), bad.size()).valid);
    }
    Bytes file = make_file(false);
    REQUIRE(!ds_port::scan_m3g_container(nullptr, 0).valid);
    REQUIRE(!ds_port::scan_m3g_container(file.data(), file.size(), 1).valid);
    Bytes changed = file;
    changed[0] ^= 0xff;
    REQUIRE(!ds_port::scan_m3g_container(changed.data(), changed.size()).valid);
    std::cout << "JSR-184 M3G section reader synthetic tests passed\n";
}
