#include "ds_port/m3g_container.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <vector>
#include <zlib.h>

namespace ds_port {
namespace {

constexpr std::array<std::uint8_t, 12> kSignature = {
    0xab, 'J', 'S', 'R', '1', '8', '4', 0xbb, 0x0d, 0x0a, 0x1a, 0x0a
};

std::uint32_t le32(const std::uint8_t* p) {
    return std::uint32_t(p[0]) |
           (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) |
           (std::uint32_t(p[3]) << 24);
}

bool valid_object_type(std::uint8_t t) {
    return t <= 22 || t == 0xff;  // 23..254 are reserved.
}

}  // namespace

M3gContainerReport scan_m3g_container(const std::uint8_t* data,
                                     std::size_t size,
                                     std::size_t max_section_bytes) {
    M3gContainerReport report;
    const auto fail = [&](const char* message) {
        report.error = message;
        report.valid = false;
        return report;
    };
    if (!data || size < kSignature.size() + 13)
        return fail("truncated M3G identifier or header section");
    if (!std::equal(kSignature.begin(), kSignature.end(), data))
        return fail("not a JSR-184 M3G file");
    if (max_section_bytes < 64 || max_section_bytes > 128u * 1024u * 1024u)
        return fail("unsafe section memory limit");

    std::size_t pos = kSignature.size();
    bool saw_scene_section = false;
    while (pos < size) {
        if (size - pos < 13)
            return fail("truncated section header/checksum");
        const std::uint8_t* section = data + pos;
        const std::uint8_t compression = section[0];
        const std::uint32_t total = le32(section + 1);
        const std::uint32_t uncompressed_size = le32(section + 5);
        if (compression > 1)
            return fail("unsupported M3G section compression scheme");
        if (total < 13 || total > size - pos)
            return fail("M3G section exceeds file bounds");
        const std::size_t stored_size = total - 13u;
        if (stored_size > max_section_bytes || uncompressed_size > max_section_bytes)
            return fail("M3G section exceeds memory limit");
        const auto expected_checksum = le32(section + total - 4u);
        const uLong actual_checksum = adler32(
            adler32(0L, Z_NULL, 0), reinterpret_cast<const Bytef*>(section),
            static_cast<uInt>(total - 4u));
        if (expected_checksum != static_cast<std::uint32_t>(actual_checksum))
            return fail("M3G section Adler32 mismatch");

        std::vector<std::uint8_t> decoded;
        const std::uint8_t* objects = section + 9;
        std::size_t objects_size = stored_size;
        if (compression == 0) {
            if (objects_size != uncompressed_size)
                return fail("uncompressed section length mismatch");
        } else {
            // Compressed M3G sections use zlib on the object bytes alone.
            // Memory is bounded by the declared, validated uncompressed size.
            decoded.resize(std::max<std::size_t>(uncompressed_size, 1));
            uLongf output_size = static_cast<uLongf>(decoded.size());
            const int rc = uncompress(
                decoded.data(), &output_size,
                reinterpret_cast<const Bytef*>(objects),
                static_cast<uLong>(stored_size));
            if (rc != Z_OK || output_size != uncompressed_size)
                return fail("invalid M3G zlib data or output size");
            objects = decoded.data();
            objects_size = output_size;
            ++report.compressed_sections;
        }
        if (report.section_count == 0 && compression != 0)
            return fail("first M3G section must not be compressed");

        std::size_t offset = 0;
        std::uint32_t objects_in_section = 0;
        while (offset < objects_size) {
            if (objects_size - offset < 5)
                return fail("truncated M3G object header");
            const std::uint8_t type = objects[offset];
            const std::uint32_t length = le32(objects + offset + 1);
            offset += 5;
            if (length > objects_size - offset)
                return fail("M3G object payload exceeds section");
            if (!valid_object_type(type))
                return fail("reserved M3G object type");
            if (report.section_count == 0) {
                if (objects_in_section != 0 || type != 0)
                    return fail("header section must contain one Header object");
                if (length < 12)
                    return fail("invalid M3G header payload size");
                const auto* header = objects + offset;
                if (header[0] != 1 || header[1] != 0)
                    return fail("unsupported JSR-184 version");
                if (header[2] > 1)
                    return fail("invalid external references flag");
                report.has_external_references = (header[2] == 1);
                if (le32(header + 3) != size)
                    return fail("M3G declared file size mismatch");
                if (header[length - 1] != 0)
                    return fail("unterminated authoring field");
            } else {
                if (type == 0)
                    return fail("unexpected second M3G header object");
                const bool external_section =
                    report.has_external_references && report.section_count == 1;
                if (external_section && type != 0xff)
                    return fail("external reference section contains scene object");
                if (!external_section && type == 0xff)
                    return fail("unexpected external reference object");
            }
            ++objects_in_section;
            ++report.object_count;
            ++report.object_type_count[type];
            offset += length;
        }
        if (report.section_count == 0 && objects_in_section != 1)
            return fail("M3G header section not exactly one object");
        if (report.section_count == 1 && objects_in_section == 0)
            return fail("first non-header M3G section is empty");
        if (report.section_count > 0 && objects_in_section != 0)
            saw_scene_section = true;
        ++report.section_count;
        pos += total;
    }
    if (report.section_count < 2 || !saw_scene_section)
        return fail("missing non-header M3G section");
    report.valid = true;
    return report;
}
}  // namespace ds_port
