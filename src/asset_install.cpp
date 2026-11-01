#include "ds_port/asset_install.hpp"
#include "ds_port/apk_probe.hpp"

#include <zlib.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ds_port {
namespace {
namespace fs = std::filesystem;
constexpr std::uint64_t max_file_bytes = 1024ULL * 1024 * 1024;
constexpr std::uint64_t max_total_bytes = 8ULL * 1024 * 1024 * 1024;
constexpr std::uint64_t max_tail = 65535ULL + 22;
constexpr std::size_t chunk_bytes = 65536;

struct Entry {
    std::string name;
    std::uint32_t offset = 0;
    std::uint32_t compressed = 0;
    std::uint32_t size = 0;
    std::uint32_t crc = 0;
    std::uint16_t method = 0;
    std::uint16_t flags = 0;
};

std::uint16_t le16(const unsigned char* p) {
    return static_cast<std::uint16_t>(p[0]) |
           (static_cast<std::uint16_t>(p[1]) << 8);
}
std::uint32_t le32(const unsigned char* p) {
    return static_cast<std::uint32_t>(le16(p)) |
           (static_cast<std::uint32_t>(le16(p + 2)) << 16);
}
void read_exact(std::ifstream& in, unsigned char* dst, std::size_t bytes) {
    if (!in.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(bytes)))
        throw std::runtime_error("Truncated APK data");
}
void seek_abs(std::ifstream& in, std::uint64_t position) {
    in.clear();
    in.seekg(static_cast<std::streamoff>(position), std::ios::beg);
    if (!in) throw std::runtime_error("Invalid APK seek");
}
bool is_asset_file(const std::string& path) {
    return path.size() > 7 && path.compare(0, 7, "assets/") == 0 &&
           path.back() != '/';
}

struct Directory {
    std::vector<Entry> assets;
    std::uint64_t central_begin = 0;
};

// First call probe_apk() to reject traversal, symlinks and ambiguous names.
Directory asset_entries(std::ifstream& in) {
    in.seekg(0, std::ios::end);
    const auto end_pos = in.tellg();
    if (end_pos < 22) throw std::runtime_error("ZIP trailer missing");
    const auto total_size = static_cast<std::uint64_t>(end_pos);
    const auto tail_size = static_cast<std::size_t>(
        std::min(total_size, max_tail));
    seek_abs(in, total_size - tail_size);
    std::vector<unsigned char> tail(tail_size);
    read_exact(in, tail.data(), tail.size());

    std::size_t footer_pos = tail.size();
    for (std::size_t i = tail.size() - 22;; --i) {
        if (le32(tail.data() + i) == 0x06054b50 &&
            i + 22 + le16(tail.data() + i + 20) == tail.size()) {
            footer_pos = i;
            break;
        }
        if (i == 0) break;
    }
    if (footer_pos == tail.size())
        throw std::runtime_error("ZIP footer invalid");

    const auto* footer = tail.data() + footer_pos;
    const auto entry_count = le16(footer + 10);
    const std::uint64_t central_size = le32(footer + 12);
    const std::uint64_t central_start = le32(footer + 16);
    const auto footer_absolute = total_size - tail_size + footer_pos;
    if (central_start + central_size != footer_absolute)
        throw std::runtime_error("ZIP central bounds mismatch");

    Directory result;
    result.central_begin = central_start;
    std::uint64_t cursor = central_start;
    for (std::uint32_t i = 0; i < entry_count; ++i) {
        if (cursor + 46 > footer_absolute)
            throw std::runtime_error("ZIP directory entry truncated");
        seek_abs(in, cursor);
        std::array<unsigned char, 46> header{};
        read_exact(in, header.data(), header.size());
        if (le32(header.data()) != 0x02014b50)
            throw std::runtime_error("ZIP central signature invalid");
        Entry entry;
        entry.flags = le16(header.data() + 8);
        entry.method = le16(header.data() + 10);
        entry.crc = le32(header.data() + 16);
        entry.compressed = le32(header.data() + 20);
        entry.size = le32(header.data() + 24);
        const std::uint16_t name_len = le16(header.data() + 28);
        const std::uint16_t extra_len = le16(header.data() + 30);
        const std::uint16_t comment_len = le16(header.data() + 32);
        entry.offset = le32(header.data() + 42);
        const std::uint64_t record = 46ULL + name_len + extra_len + comment_len;
        if (cursor + record > footer_absolute)
            throw std::runtime_error("ZIP directory record exceeds bounds");
        entry.name.assign(name_len, '\0');
        read_exact(in, reinterpret_cast<unsigned char*>(&entry.name[0]), name_len);
        cursor += record;
        if (is_asset_file(entry.name)) {
            if ((entry.method != 0 && entry.method != 8) || (entry.flags & 1))
                throw std::runtime_error("Unsupported asset compression or encryption");
            if (entry.size > max_file_bytes)
                throw std::runtime_error("Individual asset exceeds 1GiB safety limit");
            result.assets.push_back(entry);
        }
    }
    if (cursor != footer_absolute)
        throw std::runtime_error("Unexpected ZIP directory bytes");
    return result;
}

void copy_stored(std::ifstream& in, std::ofstream& out, const Entry& entry) {
    if (entry.size != entry.compressed)
        throw std::runtime_error("Stored asset has mismatched sizes");
    std::array<unsigned char, chunk_bytes> buffer{};
    std::uint64_t remaining = entry.compressed;
    uLong crc = crc32(0L, Z_NULL, 0);
    while (remaining) {
        const auto amount = static_cast<std::size_t>(
            std::min<std::uint64_t>(remaining, buffer.size()));
        read_exact(in, buffer.data(), amount);
        out.write(reinterpret_cast<const char*>(buffer.data()),
                  static_cast<std::streamsize>(amount));
        if (!out) throw std::runtime_error("Output write failed");
        crc = crc32(crc, buffer.data(), static_cast<uInt>(amount));
        remaining -= amount;
    }
    if (static_cast<std::uint32_t>(crc) != entry.crc)
        throw std::runtime_error("Stored asset CRC32 mismatch");
}

void copy_deflated(std::ifstream& in, std::ofstream& out, const Entry& entry) {
    z_stream stream{};
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        throw std::runtime_error("zlib init failed");
    std::array<unsigned char, chunk_bytes> compressed{};
    std::array<unsigned char, chunk_bytes> uncompressed{};
    std::uint64_t left = entry.compressed;
    std::uint64_t written = 0;
    uLong crc = crc32(0L, Z_NULL, 0);
    bool okay = false;
    try {
        for (;;) {
            if (stream.avail_in == 0 && left > 0) {
                const auto amount = static_cast<std::size_t>(
                    std::min<std::uint64_t>(left, compressed.size()));
                read_exact(in, compressed.data(), amount);
                stream.next_in = compressed.data();
                stream.avail_in = static_cast<uInt>(amount);
                left -= amount;
            }
            stream.next_out = uncompressed.data();
            stream.avail_out = static_cast<uInt>(uncompressed.size());
            const auto before_input = stream.avail_in;
            const int code = inflate(&stream, Z_NO_FLUSH);
            const auto produced = uncompressed.size() - stream.avail_out;
            if (written + produced > entry.size)
                throw std::runtime_error("Deflated asset exceeds declared size");
            if (produced) {
                out.write(reinterpret_cast<const char*>(uncompressed.data()),
                          static_cast<std::streamsize>(produced));
                if (!out) throw std::runtime_error("Output write failed");
                crc = crc32(crc, uncompressed.data(), static_cast<uInt>(produced));
                written += produced;
            }
            if (code == Z_STREAM_END) {
                okay = true;
                break;
            }
            if (code != Z_OK || (produced == 0 &&
                                  stream.avail_in == before_input &&
                                  stream.avail_in == 0 && left == 0))
                throw std::runtime_error("Deflate stream invalid or truncated");
        }
        if (!okay || left != 0 || stream.avail_in != 0 ||
            written != entry.size || static_cast<std::uint32_t>(crc) != entry.crc)
            throw std::runtime_error("Deflate length or CRC32 mismatch");
    } catch (...) {
        inflateEnd(&stream);
        throw;
    }
    inflateEnd(&stream);
}

void extract_one(std::ifstream& in, const fs::path& root,
                 const Entry& entry, std::uint64_t central_begin) {
    if (static_cast<std::uint64_t>(entry.offset) + 30 > central_begin)
        throw std::runtime_error("Asset local header outside ZIP data");
    seek_abs(in, entry.offset);
    std::array<unsigned char, 30> local{};
    read_exact(in, local.data(), local.size());
    if (le32(local.data()) != 0x04034b50)
        throw std::runtime_error("Asset local header invalid");
    if (le16(local.data() + 8) != entry.method ||
        le16(local.data() + 6) != entry.flags)
        throw std::runtime_error("Local and central asset methods/flags disagree");
    const std::uint16_t local_name_len = le16(local.data() + 26);
    const std::uint16_t local_extra_len = le16(local.data() + 28);
    const std::uint64_t start = static_cast<std::uint64_t>(entry.offset) + 30 +
                                local_name_len + local_extra_len;
    if (start > central_begin || entry.compressed > central_begin - start)
        throw std::runtime_error("Asset compressed bytes outside ZIP region");
    std::string local_name(local_name_len, '\0');
    if (local_name_len)
        read_exact(in, reinterpret_cast<unsigned char*>(&local_name[0]),
                   local_name_len);
    if (local_name != entry.name)
        throw std::runtime_error("Local and central asset names disagree");
    seek_abs(in, start);

    // All names were screened by probe_apk(). Fresh destination tree contains
    // only directories and files created by this installer (no archive symlinks).
    const fs::path output = root / fs::u8path(entry.name);
    fs::create_directories(output.parent_path());
    std::ofstream out(output, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot create local asset file");
    if (entry.method == 0) copy_stored(in, out, entry);
    else copy_deflated(in, out, entry);
    out.flush();
    if (!out) throw std::runtime_error("Cannot flush local asset file");
}

} // namespace

AssetInstallResult install_apk_assets(const std::string& apk_path,
                                      const std::string& destination) {
    AssetInstallResult result;
    try {
        const auto preflight = probe_apk(apk_path);
        if (!preflight.valid_archive || !preflight.looks_like_android_apk)
            throw std::runtime_error("APK preflight failed: " + preflight.error);
        std::ifstream in(apk_path, std::ios::binary);
        if (!in) throw std::runtime_error("Cannot open input APK");
        const Directory directory = asset_entries(in);
        if (directory.assets.empty())
            throw std::runtime_error("APK has no assets/ entries to install");
        std::uint64_t total = 0;
        for (const Entry& entry : directory.assets) {
            total += entry.size;
            if (total > max_total_bytes)
                throw std::runtime_error("Total assets exceed 8GiB safety limit");
        }
        const fs::path dest = fs::u8path(destination);
        if (destination.empty() || dest.empty() || dest == dest.root_path() ||
            fs::exists(dest) || fs::is_symlink(fs::symlink_status(dest)))
            throw std::runtime_error("Choose a new, nonexistent destination directory");
        if (!fs::create_directory(dest))
            throw std::runtime_error("Cannot create destination directory");

        try {
            for (const Entry& entry : directory.assets) {
                extract_one(in, dest, entry, directory.central_begin);
                ++result.assets_installed;
                result.bytes_installed += entry.size;
            }
        } catch (...) {
            std::error_code ignored;
            fs::remove_all(dest, ignored); // Cleanup partial installation.
            throw;
        }
        result.success = true;
    } catch (const std::exception& e) {
        result.error = e.what();
    }
    return result;
}
} // namespace ds_port
