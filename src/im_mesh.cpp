#include "ds_port/im_mesh.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <ostream>
#include <vector>

namespace ds_port {
namespace {
constexpr std::array<std::uint8_t,12> kMagic = {
    0xab,'I','M','-','M','3','G',0xbb,13,10,26,10
};
constexpr std::size_t kMaxFile = 64u*1024u*1024u;
constexpr std::size_t kMaxObjects = 20000;
struct Chunk { std::uint8_t type; const std::uint8_t* p; std::size_t len; };
std::uint32_t le32(const std::uint8_t* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1])<<8) |
           (std::uint32_t(p[2])<<16) | (std::uint32_t(p[3])<<24);
}
std::uint16_t le16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (std::uint16_t(p[1])<<8));
}
int signed16(const std::uint8_t* p) {
    int v = le16(p);
    return v >= 32768 ? v - 65536 : v;
}
float lefloat(const std::uint8_t* p) {
    const auto v = le32(p);
    float f = 0.f;
    std::memcpy(&f,&v,sizeof(f));
    return f;
}
const Chunk* ref(const std::vector<Chunk>& objects, std::uint32_t id,
                 std::uint8_t expected) {
    if (!id || id > objects.size()) return nullptr;
    const auto* chunk = &objects[id-1];
    return chunk->type == expected ? chunk : nullptr;
}
bool collect(const std::uint8_t* data, std::size_t size,
             std::vector<Chunk>& objects) {
    if (!data || size < 30 || size > kMaxFile ||
        std::memcmp(data,kMagic.data(),kMagic.size()) != 0 ||
        data[12] != 0 || le32(data+13) != size - 12)
        return false;
    const auto end = size - 4; // proprietary checksum not assumed Adler32
    std::size_t p = 21;
    while (p < end && objects.size() < kMaxObjects) {
        if (end-p < 5) return false;
        const auto type = data[p];
        const auto length = le32(data+p+1);
        if (length > end-p-5) return false;
        objects.push_back({type, data+p+5, length});
        p += 5 + length;
    }
    return !objects.empty() && objects[0].type == 0 && p == end;
}
bool strip_ref(const std::vector<Chunk>& objects, const Chunk& ib,
               const Chunk*& strip) {
    if (ib.len < 20) return false;
    std::size_t p = 12;
    const auto flag = le32(ib.p+8);
    if (flag == 1) {
        if (ib.len-p < 8) return false;
        p += 4; // reserved/name field
        const auto count = le32(ib.p+p); p += 4;
        if (count > ib.len-p) return false;
        p += count; // skip original submesh name, never emitted
    } else if (flag != 0) return false;
    if (ib.len-p != 8) return false;
    strip = ref(objects,le32(ib.p+p),11);
    return strip && ref(objects,le32(ib.p+p+4),3);
}
bool parse_strip(const Chunk& strip, std::uint32_t vertex_count,
                 std::uint64_t vertex_offset, std::uint64_t& triangles,
                 std::ostream* stream) {
    if (strip.len < 21) return false;
    const auto width = strip.p[12] == 0x81 ? 1u :
                       strip.p[12] == 0x82 ? 2u : 0u;
    if (!width) return false;
    const auto count = le32(strip.p+13);
    if (count > (strip.len-21)/width) return false;
    std::size_t p = 17 + std::size_t(count)*width;
    const auto strip_count = le32(strip.p+p);
    p += 4;
    if (strip_count > (strip.len-p)/4 ||
        p + std::size_t(strip_count)*4 != strip.len) return false;
    if (std::uint64_t(strip_count)*3 != count) return false;
    for (std::uint32_t i=0; i<strip_count; ++i)
        if (le32(strip.p+p+std::size_t(i)*4) != 3)
            return false; // only the verified three-index-strip subset
    auto index = [&](std::uint32_t i) -> std::uint32_t {
        const auto at = 17 + std::size_t(i)*width;
        return width == 1 ? strip.p[at] : le16(strip.p+at);
    };
    for (std::uint32_t i=0; i<count; i+=3) {
        const auto a=index(i), b=index(i+1), c=index(i+2);
        if (a>=vertex_count || b>=vertex_count || c>=vertex_count)
            return false;
        if (stream) {
            *stream << "f " << (vertex_offset+a+1) << " "
                    << (vertex_offset+b+1) << " "
                    << (vertex_offset+c+1) << "\n";
        }
    }
    triangles += strip_count;
    return !stream || bool(*stream);
}
} // namespace

ImMeshReport scan_im_mesh(const std::uint8_t* data, std::size_t size,
                          std::ostream* output) {
    ImMeshReport report;
    auto fail = [&](const char* msg) {
        report.error = msg; report.valid = false; return report;
    };
    std::vector<Chunk> objects;
    if (!collect(data,size,objects))
        return fail("unsupported or malformed IM-M3G section/object framing");
    std::uint32_t mesh_no = 0;
    std::uint64_t global_vertex_offset = 0;
    if (output) *output << "# User-provided IM-M3G mesh geometry (no materials)\n";
    for (const auto& mesh : objects) {
        if (mesh.type != 14) continue;
        if (mesh.len < 30) return fail("truncated Mesh object");
        const auto* vb=ref(objects,le32(mesh.p+22),21);
        if (!vb || vb->len < 48) return fail("Mesh references invalid VertexBuffer");
        const auto* positions=ref(objects,le32(vb->p+16),20);
        if (!positions || positions->len < 17 || positions->p[12]!=2 ||
            positions->p[13]!=3 || positions->p[14]!=0)
            return fail("unsupported Mesh position VertexArray format");
        const auto vertex_count = le16(positions->p+15);
        if (!vertex_count || positions->len != 17+std::size_t(vertex_count)*6)
            return fail("position VertexArray byte count mismatch");
        const float bias[3] = {lefloat(vb->p+20),lefloat(vb->p+24),lefloat(vb->p+28)};
        const float scale = lefloat(vb->p+32);
        if (!std::isfinite(scale) || !std::isfinite(bias[0]) ||
            !std::isfinite(bias[1]) || !std::isfinite(bias[2]))
            return fail("non-finite Mesh transform");
        const auto sub_count = le32(mesh.p+26);
        if (sub_count > (mesh.len-30)/4 ||
            mesh.len != 30+std::size_t(sub_count)*4)
            return fail("Mesh submesh references do not fit object");
        if (output) *output << "o mesh_" << mesh_no << "\n";
        for (std::uint32_t v=0; v<vertex_count; ++v) {
            const auto* xyz = positions->p + 17 + std::size_t(v)*6;
            const double x = double(bias[0]) + double(scale)*signed16(xyz);
            const double y = double(bias[1]) + double(scale)*signed16(xyz+2);
            const double z = double(bias[2]) + double(scale)*signed16(xyz+4);
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
                return fail("non-finite decoded vertex position");
            if (output) *output << "v " << x << " " << y << " " << z << "\n";
        }
        for (std::uint32_t i=0; i<sub_count; ++i) {
            const auto* ib=ref(objects,le32(mesh.p+30+std::size_t(i)*4),100);
            if (!ib) return fail("Mesh submesh reference missing IM IndexBuffer");
            const Chunk* strip = nullptr;
            if (!strip_ref(objects,*ib,strip))
                return fail("invalid IM IndexBuffer->strip/appearance references");
            if (output) *output << "g mesh_" << mesh_no << "_part_" << i << "\n";
            if (!parse_strip(*strip,vertex_count,global_vertex_offset,
                             report.triangles,output))
                return fail("index data or triangle strip bounds invalid");
            ++report.submeshes;
        }
        report.vertices += vertex_count;
        global_vertex_offset += vertex_count;
        ++mesh_no;
        ++report.mesh_objects;
    }
    if (!report.mesh_objects || !report.submeshes || !report.triangles)
        return fail("no supported Mesh geometry found");
    if (output && !(*output)) return fail("OBJ output write failed");
    report.valid = true;
    return report;
}
}  // namespace ds_port
