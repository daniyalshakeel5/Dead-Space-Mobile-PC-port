#include "ds_port/asset_install.hpp"
#include <zlib.h>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#define CHECK(x) do { if (!(x)) { \
    std::cerr << "FAIL line " << __LINE__ << ": " << #x << '\n'; return 1; \
} } while (false)

namespace fs = std::filesystem;
struct Fixture {
    std::string name;
    std::string contents;
    bool deflated = false;
    bool bad_crc = false;
};
void put16(std::ofstream& out, std::uint16_t v) {
    out.put(static_cast<char>(v & 255));
    out.put(static_cast<char>((v >> 8) & 255));
}
void put32(std::ofstream& out, std::uint32_t v) {
    put16(out, static_cast<std::uint16_t>(v & 65535));
    put16(out, static_cast<std::uint16_t>(v >> 16));
}
std::string compress_raw(const std::string& input) {
    z_stream stream{};
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                     -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        throw std::runtime_error("deflate init failed");
    std::vector<unsigned char> bytes(compressBound(
        static_cast<uLong>(input.size())) + 64);
    stream.next_in = reinterpret_cast<Bytef*>(
        const_cast<char*>(input.data()));
    stream.avail_in = static_cast<uInt>(input.size());
    stream.next_out = bytes.data();
    stream.avail_out = static_cast<uInt>(bytes.size());
    const int status = deflate(&stream, Z_FINISH);
    const std::size_t count = stream.total_out;
    deflateEnd(&stream);
    if (status != Z_STREAM_END)
        throw std::runtime_error("deflate fixture failed");
    return std::string(reinterpret_cast<const char*>(bytes.data()), count);
}
void write_zip(const fs::path& path, const std::vector<Fixture>& files) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    struct Meta { std::uint32_t offset, crc; std::string data; };
    std::vector<Meta> metadata;
    for (const auto& f : files) {
        const auto offset = static_cast<std::uint32_t>(out.tellp());
        const auto data = f.deflated ? compress_raw(f.contents) : f.contents;
        const auto crc = static_cast<std::uint32_t>(
            crc32(0, reinterpret_cast<const Bytef*>(f.contents.data()),
                  static_cast<uInt>(f.contents.size())));
        metadata.push_back({offset, crc, data});
        put32(out, 0x04034b50);
        put16(out, 20); put16(out, 0); put16(out, f.deflated ? 8 : 0);
        put16(out, 0); put16(out, 0);
        put32(out, crc);
        put32(out, static_cast<std::uint32_t>(data.size()));
        put32(out, static_cast<std::uint32_t>(f.contents.size()));
        put16(out, static_cast<std::uint16_t>(f.name.size()));
        put16(out, 0);
        out.write(f.name.data(), static_cast<std::streamsize>(f.name.size()));
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
    }
    const auto start = static_cast<std::uint32_t>(out.tellp());
    for (std::size_t i = 0; i < files.size(); ++i) {
        const auto& f = files[i];
        const auto& m = metadata[i];
        put32(out, 0x02014b50);
        put16(out, 20); put16(out, 20);
        put16(out, 0); put16(out, f.deflated ? 8 : 0);
        put16(out, 0); put16(out, 0);
        put32(out, m.crc + (f.bad_crc ? 1U : 0U));
        put32(out, static_cast<std::uint32_t>(m.data.size()));
        put32(out, static_cast<std::uint32_t>(f.contents.size()));
        put16(out, static_cast<std::uint16_t>(f.name.size()));
        put16(out, 0); put16(out, 0);
        put16(out, 0); put16(out, 0); put32(out, 0);
        put32(out, m.offset);
        out.write(f.name.data(), static_cast<std::streamsize>(f.name.size()));
    }
    const auto end = static_cast<std::uint32_t>(out.tellp());
    put32(out, 0x06054b50);
    put16(out, 0); put16(out, 0);
    put16(out, static_cast<std::uint16_t>(files.size()));
    put16(out, static_cast<std::uint16_t>(files.size()));
    put32(out, end - start);
    put32(out, start);
    put16(out, 0);
}
std::string read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
}
int main() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() /
                      ("deadspace_install_test_" + std::to_string(stamp));
    fs::create_directory(root);
    const auto apk = root / "owned.apk";
    const auto install = root / "installed";

    const std::string compressed_text(100000, 'A');
    write_zip(apk, {
        {"AndroidManifest.xml", "fake manifest"},
        {"classes.dex", "fake dex"},
        {"assets/hello.dat", "stored fixture"},
        {"assets/sub/deep.dat", compressed_text, true},
        {"lib/armeabi-v7a/libDeadSpace.so", "fake ELF"},
    });
    auto result = ds_port::install_apk_assets(apk.string(), install.string());
    CHECK(result.success);
    CHECK(result.assets_installed == 2);
    CHECK(result.bytes_installed == compressed_text.size() + 14);
    CHECK(read_file(install / "assets/hello.dat") == "stored fixture");
    CHECK(read_file(install / "assets/sub/deep.dat") == compressed_text);
    CHECK(!fs::exists(install / "classes.dex"));
    CHECK(!fs::exists(install / "lib"));
    CHECK(!fs::exists(install / "AndroidManifest.xml"));

    // Never overwrite a prior local installation.
    CHECK(!ds_port::install_apk_assets(apk.string(), install.string()).success);

    fs::remove_all(install);
    write_zip(apk, {{"AndroidManifest.xml", "fixture"},
                    {"assets/one.dat", "first"},
                    {"assets/two.dat", "mismatched checksum", false, true}});
    result = ds_port::install_apk_assets(apk.string(), install.string());
    CHECK(!result.success);
    CHECK(!fs::exists(install)); // partial assets removed on failure

    write_zip(apk, {{"AndroidManifest.xml", "fixture"},
                    {"assets/../escape.txt", "bad"}});
    result = ds_port::install_apk_assets(apk.string(), install.string());
    CHECK(!result.success);
    CHECK(!fs::exists(install));
    CHECK(!fs::exists(root / "escape.txt"));

    write_zip(apk, {{"AndroidManifest.xml", "fixture"}});
    result = ds_port::install_apk_assets(apk.string(), install.string());
    CHECK(!result.success);
    CHECK(!fs::exists(install));

    fs::remove_all(root);
    std::cout << "Local user-owned APK asset installer tests passed.\n";
    return 0;
}
