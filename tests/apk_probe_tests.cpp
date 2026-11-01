#include "ds_port/apk_probe.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#define CHECK(cond) do { if (!(cond)) { \
    std::cerr << "FAIL line " << __LINE__ << ": " << #cond << '\n'; return 1; \
} } while (false)

struct FileEntry {
    std::string name, contents;
};
void put16(std::ostream& out, std::uint16_t v) {
    out.put(static_cast<char>(v & 255));
    out.put(static_cast<char>((v >> 8) & 255));
}
void put32(std::ostream& out, std::uint32_t v) {
    put16(out, static_cast<std::uint16_t>(v & 65535));
    put16(out, static_cast<std::uint16_t>(v >> 16));
}
void make_zip(const std::filesystem::path& path,
              const std::vector<FileEntry>& entries) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    std::vector<std::uint32_t> offsets;
    for (const auto& item : entries) {
        offsets.push_back(static_cast<std::uint32_t>(out.tellp()));
        put32(out, 0x04034b50);
        put16(out, 20);  // version
        put16(out, 0);   // flags
        put16(out, 0);   // stored
        put16(out, 0); put16(out, 0); put32(out, 0); // times, CRC
        put32(out, static_cast<std::uint32_t>(item.contents.size()));
        put32(out, static_cast<std::uint32_t>(item.contents.size()));
        put16(out, static_cast<std::uint16_t>(item.name.size()));
        put16(out, 0); // extra
        out.write(item.name.data(), static_cast<std::streamsize>(item.name.size()));
        out.write(item.contents.data(), static_cast<std::streamsize>(item.contents.size()));
    }
    const auto cd_start = static_cast<std::uint32_t>(out.tellp());
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto& item = entries[i];
        put32(out, 0x02014b50);
        put16(out, 20); put16(out, 20); // made by and needed
        put16(out, 0); put16(out, 0); // flags, method
        put16(out, 0); put16(out, 0); put32(out, 0); // time, CRC
        put32(out, static_cast<std::uint32_t>(item.contents.size()));
        put32(out, static_cast<std::uint32_t>(item.contents.size()));
        put16(out, static_cast<std::uint16_t>(item.name.size()));
        put16(out, 0); put16(out, 0); // extra and comment
        put16(out, 0); put16(out, 0); put32(out, 0); // disk, attrs
        put32(out, offsets[i]);
        out.write(item.name.data(), static_cast<std::streamsize>(item.name.size()));
    }
    const auto cd_end = static_cast<std::uint32_t>(out.tellp());
    put32(out, 0x06054b50);
    put16(out, 0); put16(out, 0);
    put16(out, static_cast<std::uint16_t>(entries.size()));
    put16(out, static_cast<std::uint16_t>(entries.size()));
    put32(out, cd_end - cd_start);
    put32(out, cd_start);
    put16(out, 0); // no comment
}

int main() {
    namespace fs = std::filesystem;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto file = fs::temp_directory_path() /
                       ("deadspace_apk_fixture_" + std::to_string(stamp) + ".zip");

    make_zip(file, {{"AndroidManifest.xml", "synthetic"}, 
                    {"classes.dex", "not real dex"},
                    {"assets/engine.bin", "synthetic asset"},
                    {"lib/armeabi-v7a/libDeadSpace.so", "not real ELF"}});
    auto result = ds_port::probe_apk(file.string());
    CHECK(result.valid_archive);
    CHECK(result.looks_like_android_apk);
    CHECK(result.entries == 4);
    CHECK(result.assets == 1);
    CHECK(result.native_libraries == 1);
    CHECK(result.dex_files == 1);
    CHECK(result.has_deadspace_library_name);

    make_zip(file, {{"readme.txt", "just a zip"}});
    result = ds_port::probe_apk(file.string());
    CHECK(result.valid_archive);
    CHECK(!result.looks_like_android_apk);

    make_zip(file, {{"AndroidManifest.xml", ""},
                    {"../escape.cpp", "not permitted"}});
    CHECK(!ds_port::probe_apk(file.string()).valid_archive);

    make_zip(file, {{"AndroidManifest.xml", ""},
                    {"assets/Thing.bin", "a"}, {"assets/thing.bin", "b"}});
    CHECK(!ds_port::probe_apk(file.string()).valid_archive);

    make_zip(file, {{"AndroidManifest.xml", ""}});
    {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << "corrupt";
    }
    CHECK(!ds_port::probe_apk(file.string()).valid_archive);
    fs::remove(file);

    CHECK(!ds_port::probe_apk(file.string()).valid_archive);
    std::cout << "ZIP32 APK metadata preflight tests passed.\n";
    return 0;
}
