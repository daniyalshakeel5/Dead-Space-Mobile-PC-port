#include "ds_port/apk_probe.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <limits>
#include <string>
#include <unordered_set>
#include <vector>

namespace ds_port {
namespace {
std::uint16_t u16(const unsigned char* p) {
    return static_cast<std::uint16_t>(p[0]) |
           (static_cast<std::uint16_t>(p[1]) << 8);
}
std::uint32_t u32(const unsigned char* p) {
    return static_cast<std::uint32_t>(u16(p)) |
           (static_cast<std::uint32_t>(u16(p + 2)) << 16);
}
bool has_prefix(const std::string& s, const char* prefix) {
    return s.compare(0, std::char_traits<char>::length(prefix), prefix) == 0;
}
bool has_suffix(const std::string& s, const char* suffix) {
    const auto n = std::char_traits<char>::length(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}
bool path_is_safe(const std::string& path) {
    if (path.empty() || path.front() == '/' || path.find('\\') != std::string::npos ||
        path.find(':') != std::string::npos || path.find('\0') != std::string::npos)
        return false;
    std::size_t begin = 0;
    while (begin < path.size()) {
        const std::size_t end = path.find('/', begin);
        const std::string part = path.substr(begin, end == std::string::npos
                                                       ? std::string::npos : end - begin);
        if (part.empty() || part == "." || part == "..")
            return false;
        if (end == std::string::npos)
            break;
        begin = end + 1;
        // Allow only one trailing slash (directory entry).
        if (begin == path.size())
            break;
    }
    return true;
}
std::string ascii_fold(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}
ApkProbeResult fail(const char* reason) {
    ApkProbeResult result;
    result.error = reason;
    return result;
}
}

ApkProbeResult probe_apk(const std::string& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input)
        return fail("Cannot open APK");
    const auto end_pos = input.tellg();
    if (end_pos < 0)
        return fail("Cannot measure APK");
    const std::uint64_t size = static_cast<std::uint64_t>(end_pos);
    if (size < 22)
        return fail("Not a ZIP file: missing end-of-central-directory");
    constexpr std::uint64_t max_tail = 22 + 65'535;
    const auto tail_size = static_cast<std::size_t>(std::min(size, max_tail));
    std::vector<unsigned char> tail(tail_size);
    input.seekg(static_cast<std::streamoff>(size - tail_size), std::ios::beg);
    input.read(reinterpret_cast<char*>(tail.data()), static_cast<std::streamsize>(tail.size()));
    if (!input)
        return fail("Cannot read ZIP trailer");

    std::size_t eocd = tail.size();
    for (std::size_t i = tail.size() - 22;; --i) {
        if (u32(tail.data() + i) == 0x06054b50 &&
            i + 22 + u16(tail.data() + i + 20) == tail.size()) {
            eocd = i;
            break;
        }
        if (i == 0) break;
    }
    if (eocd == tail.size())
        return fail("ZIP central directory footer missing or malformed");
    const auto* footer = tail.data() + eocd;
    if (u16(footer + 4) != 0 || u16(footer + 6) != 0 ||
        u16(footer + 8) != u16(footer + 10))
        return fail("Multi-disk ZIP is unsupported");

    const std::uint32_t entry_count = u16(footer + 10);
    const std::uint32_t central_bytes = u32(footer + 12);
    const std::uint32_t central_start = u32(footer + 16);
    if (entry_count == 0xffff || central_bytes == 0xffffffffU ||
        central_start == 0xffffffffU)
        return fail("ZIP64 is unsupported by the read-only preflight");
    const std::uint64_t central_end =
        static_cast<std::uint64_t>(central_start) + central_bytes;
    const std::uint64_t eocd_absolute = size - tail_size + eocd;
    if (central_end != eocd_absolute)
        return fail("Central directory bounds do not match trailer");
    if (entry_count > 40'000)
        return fail("Archive contains too many entries for native preflight");

    input.clear();
    input.seekg(static_cast<std::streamoff>(central_start), std::ios::beg);
    if (!input)
        return fail("Cannot seek to ZIP central directory");

    ApkProbeResult result;
    result.entries = entry_count;
    std::unordered_set<std::string> seen;
    seen.reserve(entry_count);
    std::uint64_t cursor = central_start;
    for (std::uint32_t i = 0; i < entry_count; ++i) {
        if (central_end < cursor || central_end - cursor < 46)
            return fail("Truncated ZIP entry header");
        std::array<unsigned char, 46> hdr{};
        input.read(reinterpret_cast<char*>(hdr.data()), hdr.size());
        if (!input || u32(hdr.data()) != 0x02014b50)
            return fail("Invalid ZIP central-directory entry");
        const std::uint16_t flags = u16(hdr.data() + 8);
        const std::uint32_t stored = u32(hdr.data() + 20);
        const std::uint32_t unpacked = u32(hdr.data() + 24);
        const std::uint16_t name_size = u16(hdr.data() + 28);
        const std::uint16_t extra_size = u16(hdr.data() + 30);
        const std::uint16_t comment_size = u16(hdr.data() + 32);
        const std::uint32_t attributes = u32(hdr.data() + 38);
        const std::uint32_t local_offset = u32(hdr.data() + 42);
        const std::uint64_t record_size =
            static_cast<std::uint64_t>(46) + name_size + extra_size + comment_size;
        if (cursor + record_size > central_end)
            return fail("ZIP filename or extra field exceeds directory bounds");
        if (!name_size || flags & 1 || stored == 0xffffffffU ||
            unpacked == 0xffffffffU || local_offset == 0xffffffffU)
            return fail("Unsupported encrypted or ZIP64 entry");
        if (local_offset >= central_start)
            return fail("ZIP entry points outside the local-file region");
        // Unix symlink file type: permissions in external attributes, top word.
        if (((attributes >> 16) & 0170000U) == 0120000U)
            return fail("ZIP symlink entry is not accepted");
        std::string name(name_size, '\0');
        input.read(&name[0], name_size);
        if (!input || !path_is_safe(name))
            return fail("Unsafe ZIP entry filename");
        if (!seen.insert(ascii_fold(name)).second)
            return fail("Duplicate or case-colliding ZIP entry");
        input.seekg(static_cast<std::streamoff>(extra_size) + comment_size,
                    std::ios::cur);
        if (!input)
            return fail("Invalid ZIP extra field");
        cursor += record_size;
        result.declared_uncompressed_bytes += unpacked;
        if (result.declared_uncompressed_bytes > 16ULL * 1024 * 1024 * 1024)
            return fail("Archive exceeds metadata size guard");

        if (name == "AndroidManifest.xml") result.has_android_manifest = true;
        if (name == "classes.dex" || (has_prefix(name, "classes") &&
            has_suffix(name, ".dex"))) ++result.dex_files;
        if (has_prefix(name, "assets/") && !has_suffix(name, "/"))
            ++result.assets;
        if (has_prefix(name, "lib/") && has_suffix(name, ".so")) {
            ++result.native_libraries;
            if (has_suffix(name, "/libDeadSpace.so"))
                result.has_deadspace_library_name = true;
        }
    }
    if (cursor != central_end)
        return fail("Unexpected bytes at end of central directory");
    result.valid_archive = true;
    result.looks_like_android_apk = result.has_android_manifest;
    if (!result.looks_like_android_apk)
        result.error = "Valid ZIP, but AndroidManifest.xml is missing";
    return result;
}
} // namespace ds_port
