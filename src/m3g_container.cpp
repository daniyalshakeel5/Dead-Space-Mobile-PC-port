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


// JSR-184 Object3D + Image2D layout. Only checks bounded lengths and
// format/dimensions; it never copies, transforms or exports image pixels.
struct Image2dEvidence {
    std::uint8_t format = 0;
    std::uint32_t width = 0, height = 0, pixel_bytes = 0;
    std::uint32_t palette_bytes = 0;
    std::size_t pixel_offset = 0;
    bool is_mutable = false;
    std::uint8_t failure_stage = 0;
};

bool parse_image2d_payload(const std::uint8_t* bytes,
                           std::size_t size, Image2dEvidence& evidence) {
    std::size_t at = 0;
    const auto fail = [&](std::uint8_t stage) {
        evidence.failure_stage = stage;
        return false;
    };
    const auto take32 = [&](std::uint32_t& out) -> bool {
        if (at > size || size - at < 4) return false;
        out = le32(bytes + at);
        at += 4;
        return true;
    };
    std::uint32_t user_id = 0, count = 0;
    if (!take32(user_id) || !take32(count)) return fail(1);
    if (count > (size - at) / 4) return fail(1);
    at += std::size_t(count) * 4; // ObjectIndex[] animationTracks
    if (!take32(count) || count > (size - at) / 8) return fail(1);
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t parameter_id = 0, length = 0;
        if (!take32(parameter_id) || !take32(length)) return fail(1);
        if (length > size - at) return fail(1);
        at += length;
    }
    if (size - at < 10) return fail(2);
    const std::uint8_t format = bytes[at++];
    const std::uint8_t mutable_flag = bytes[at++];
    evidence.format = format; // used for aggregate unknown-code research
    if (format < 96 || format > 100) return fail(3);
    if (mutable_flag > 1) return fail(8);
    const std::uint32_t width = le32(bytes + at); at += 4;
    const std::uint32_t height = le32(bytes + at); at += 4;
    if (!width || !height || width > 8192 || height > 8192) return fail(4);
    std::uint32_t pixel_bytes = 0;
    std::uint32_t palette_bytes = 0;
    std::size_t pixel_offset = 0;
    if (mutable_flag == 0) {
        std::uint32_t palette_len = 0, pixel_len = 0;
        if (!take32(palette_len) || palette_len > size - at) return fail(5);
        at += palette_len;
        if (!take32(pixel_len) || pixel_len > size - at) return fail(5);
        pixel_offset = at;
        palette_bytes = palette_len;
        at += pixel_len;
        // Validate counts against JSR-184 encodings, not an arbitrary guessed
        // pixel format. Palette consists of at most 256 component entries.
        const std::uint32_t channels = format <= 97 ? 1u :
            (format == 98 ? 2u : (format == 99 ? 3u : 4u));
        const std::uint64_t pixels = std::uint64_t(width) * height;
        if (palette_len > 0) {
            if (palette_len % channels || palette_len > 256u * channels ||
                pixel_len != pixels) return fail(6);
        } else if (std::uint64_t(pixel_len) != pixels * channels) {
            return fail(6);
        }
        pixel_bytes = pixel_len;
    }
    if (at != size) return fail(7);
    evidence = Image2dEvidence{format, width, height, pixel_bytes, palette_bytes,
                               pixel_offset, mutable_flag != 0};
    return true;
}

}  // namespace

M3gContainerReport scan_m3g_container(const std::uint8_t* data,
                                     std::size_t size,
                                     std::size_t max_section_bytes, M3gScanMode mode) {
    M3gContainerReport report;
    report.diagnostic_mode = (mode == M3gScanMode::DiagnosticOpaqueVariant);
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
            if (!valid_object_type(type)) {
                if (!report.diagnostic_mode)
                    return fail("reserved M3G object type");
                ++report.reserved_type_objects;
            }
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
                if (type == 0) {
                    if (!report.diagnostic_mode)
                        return fail("unexpected second M3G header object");
                    ++report.nonstandard_header_objects;
                }
                const bool external_section =
                    report.has_external_references && report.section_count == 1;
                if (external_section && type != 0xff && !report.diagnostic_mode)
                    return fail("external reference section contains scene object");
                if (!external_section && type == 0xff && !report.diagnostic_mode)
                    return fail("unexpected external reference object");
            }
            if (type == 10) {
                ++report.image2d_objects;
                Image2dEvidence evidence;
                if (parse_image2d_payload(objects + offset, length, evidence)) {
                    ++report.image2d_standard_payloads;
                    ++report.image2d_format_counts[evidence.format - 96];
                    report.image2d_max_width =
                        std::max(report.image2d_max_width, evidence.width);
                    report.image2d_max_height =
                        std::max(report.image2d_max_height, evidence.height);
                    report.image2d_validated_pixel_bytes += evidence.pixel_bytes;
                } else {
                    ++report.image2d_unrecognized_payloads;
                    ++report.image2d_failure_stage_counts[evidence.failure_stage];
                    if (evidence.failure_stage == 3)
                        ++report.image2d_unknown_format_counts[evidence.format];
                }
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
M3gRgbaImage decode_m3g_rgba_image(const std::uint8_t* data, std::size_t size) {
    M3gRgbaImage image;
    const auto fail = [&](const char* reason) {
        image.error = reason;
        image.decoded = false;
        image.pixels_rgba.clear();
        return image;
    };
    const auto report = scan_m3g_container(
        data, size, 64u * 1024u * 1024u, M3gScanMode::DiagnosticOpaqueVariant);
    if (!report.valid)
        return fail("invalid M3G section framing or checksum");
    if (report.section_count != 2 || report.object_count != 3 ||
        report.nonstandard_header_objects != 1 ||
        report.object_type_count[0] != 2 || report.object_type_count[10] != 1 ||
        report.image2d_standard_payloads != 1)
        return fail("unsupported object grouping or Image2D payload");
    const std::size_t section0_size = le32(data + 13);
    const std::size_t second = kSignature.size() + section0_size;
    // scan_m3g_container already validated section sizes and checksum.
    if (second > size || size - second < 13 || data[second] != 0)
        return fail("unsupported compressed texture section");
    const std::size_t total = le32(data + second + 1);
    if (total < 13 || total > size - second)
        return fail("invalid texture section size");
    const std::uint8_t* content = data + second + 9;
    const std::size_t stored = total - 13;
    if (stored < 10 || content[0] != 0)
        return fail("missing IronMonkey secondary header");
    const std::size_t secondary_length = le32(content + 1);
    if (secondary_length > stored - 10)
        return fail("secondary header length exceeds section");
    const std::size_t type_offset = 5 + secondary_length;
    if (content[type_offset] != 10)
        return fail("not an Image2D resource");
    const std::size_t data_length = le32(content + type_offset + 1);
    if (data_length != stored - (type_offset + 5))
        return fail("unexpected trailing image object data");
    const auto* pixels_object = content + type_offset + 5;
    Image2dEvidence evidence;
    if (!parse_image2d_payload(pixels_object, data_length, evidence))
        return fail("Image2D metadata failed JSR-184 bounds validation");
    if (evidence.format != 100 || evidence.is_mutable ||
        evidence.palette_bytes != 0 ||
        std::uint64_t(evidence.width) * evidence.height * 4 >
            64ull * 1024ull * 1024ull)
        return fail("only non-paletted immutable RGBA is supported");
    if (evidence.pixel_bytes != std::uint64_t(evidence.width) * evidence.height * 4 ||
        evidence.pixel_offset > data_length ||
        evidence.pixel_bytes > data_length - evidence.pixel_offset)
        return fail("RGBA pixel span invalid");
    const auto* start = pixels_object + evidence.pixel_offset;
    image.pixels_rgba.assign(start, start + evidence.pixel_bytes);
    image.width = evidence.width;
    image.height = evidence.height;
    image.decoded = true;
    return image;
}

}  // namespace ds_port
