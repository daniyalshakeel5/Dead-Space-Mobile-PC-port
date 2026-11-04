// Converts a user's locally installed, strictly checked JSR-184 Image2D RGBA
// resource to PNG. No original assets shipped with this independent tool.
#include "ds_port/m3g_container.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <zlib.h>

namespace {
void big_endian32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int i=3; i>=0; --i) out.push_back(static_cast<std::uint8_t>((v >> (i*8)) & 0xff));
}
bool chunk(std::ofstream& out, const char* type, const std::uint8_t* data, std::size_t length) {
    if (length > 0xffffffffull) return false;
    std::vector<std::uint8_t> record;
    big_endian32(record, static_cast<std::uint32_t>(length));
    for (int i=0;i<4;i++) record.push_back(static_cast<std::uint8_t>(type[i]));
    if (length) record.insert(record.end(), data, data + length);
    const auto crc = crc32(crc32(0L,Z_NULL,0),
              reinterpret_cast<const Bytef*>(record.data()+4),
              static_cast<uInt>(record.size()-4));
    big_endian32(record, static_cast<std::uint32_t>(crc));
    out.write(reinterpret_cast<const char*>(record.data()),
              static_cast<std::streamsize>(record.size()));
    return bool(out);
}
bool write_png_rgba(const std::filesystem::path& dest, const ds_port::M3gRgbaImage& image) {
    if (!image.decoded || !image.width || !image.height ||
        image.width > 4096 || image.height > 4096) return false;
    const std::uint64_t pixel_bytes = std::uint64_t(image.width)*image.height*4;
    if (pixel_bytes != image.pixels_rgba.size() || pixel_bytes > 64ull*1024*1024)
        return false;
    std::vector<std::uint8_t> rows;
    rows.reserve(static_cast<std::size_t>(pixel_bytes)+image.height);
    for (std::uint32_t y=0; y<image.height; ++y) {
        rows.push_back(0); // PNG scanline filter: none
        const auto offset = std::size_t(y)*image.width*4;
        rows.insert(rows.end(),
                    image.pixels_rgba.begin()+static_cast<std::ptrdiff_t>(offset),
                    image.pixels_rgba.begin()+static_cast<std::ptrdiff_t>(offset+image.width*4));
    }
    uLongf compressed_size = compressBound(static_cast<uLong>(rows.size()));
    std::vector<std::uint8_t> compressed(static_cast<std::size_t>(compressed_size));
    if (compress2(compressed.data(), &compressed_size, rows.data(),
                  static_cast<uLong>(rows.size()), Z_BEST_SPEED) != Z_OK)
        return false;
    compressed.resize(compressed_size);
    // Never overwrite existing files; appends only at explicitly requested location.
    if (std::filesystem::exists(dest)) return false;
    std::ofstream out(dest, std::ios::binary | std::ios::out);
    if (!out) return false;
    constexpr unsigned char signature[8]={137,80,78,71,13,10,26,10};
    out.write(reinterpret_cast<const char*>(signature),sizeof(signature));
    std::vector<std::uint8_t> ihdr;
    big_endian32(ihdr,image.width); big_endian32(ihdr,image.height);
    ihdr.insert(ihdr.end(),{8,6,0,0,0}); // 8-bit RGBA, standard PNG
    const bool ok = bool(out) && chunk(out,"IHDR",ihdr.data(),ihdr.size()) &&
                    chunk(out,"IDAT",compressed.data(),compressed.size()) &&
                    chunk(out,"IEND",nullptr,0);
    out.close();
    if (!ok || !out) {
        std::error_code ec;
        std::filesystem::remove(dest,ec);
        return false;
    }
    return true;
}
}
int main(int argc, char** argv) {
    if (argc!=3) {
        std::cerr<<"usage: ds_m3g_texture_extract <owned-resource.m3g> <new-output.png>\n";
        return 2;
    }
    const std::filesystem::path output(argv[2]);
    std::error_code ec;
    if (std::filesystem::exists(output,ec)) {
        std::cerr<<"Refusing to overwrite an existing file.\n";
        return 2;
    }
    std::ifstream in(argv[1],std::ios::binary|std::ios::ate);
    if(!in){std::cerr<<"Cannot open source file.\n";return 2;}
    const auto count=in.tellg();
    if(count<0 || static_cast<std::uint64_t>(count)>128ull*1024*1024){
        std::cerr<<"File too large.\n";return 2;
    }
    std::vector<std::uint8_t> input(static_cast<std::size_t>(count));
    in.seekg(0);
    if(!input.empty() && !in.read(reinterpret_cast<char*>(input.data()),count)){
        std::cerr<<"Input read failed.\n";return 2;
    }
    auto image=ds_port::decode_m3g_rgba_image(input.data(),input.size());
    if(!image.decoded)
        image=ds_port::decode_m3g_etc1_image(input.data(),input.size());
    if(!image.decoded){
        std::cerr<<"Unsupported M3G resource (verified RGBA or ETC1 mip chain only).\n";
        return 1;
    }
    if(!write_png_rgba(output,image)){std::cerr<<"Could not write PNG.\n";return 1;}
    std::cout<<"Decoded local RGBA pixels: "<<image.width<<"x"<<image.height<<"\n";
    return 0;
}
