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
int main() {
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
