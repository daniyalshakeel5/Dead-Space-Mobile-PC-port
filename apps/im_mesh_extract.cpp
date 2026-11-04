#include "ds_port/im_mesh.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "usage: ds_im_mesh_extract <owned-model.m3g> [new-output.obj]\n";
        return 2;
    }
    std::ifstream input(argv[1],std::ios::binary|std::ios::ate);
    if (!input) {std::cerr<<"Cannot open local input.\n";return 2;}
    const auto size = input.tellg();
    if (size < 0 || static_cast<std::uint64_t>(size) > 64ull*1024*1024) {
        std::cerr<<"M3G exceeds safe input cap.\n";return 2;
    }
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    input.seekg(0);
    if (!data.empty() &&
        !input.read(reinterpret_cast<char*>(data.data()),size)) {
        std::cerr<<"Input read failed.\n";return 2;
    }
    std::ofstream obj;
    const bool export_obj = argc == 3;
    std::filesystem::path dest;
    if (export_obj) {
        dest = argv[2];
        std::error_code ec;
        if (std::filesystem::exists(dest,ec)) {
            std::cerr<<"Refusing to overwrite existing OBJ.\n";return 2;
        }
        obj.open(dest,std::ios::binary);
        if (!obj) {std::cerr<<"Cannot open new output path.\n";return 2;}
    }
    const auto result = ds_port::scan_im_mesh(data.data(),data.size(),
                                                export_obj ? &obj : nullptr);
    if (export_obj) obj.close();
    if (!result.valid || (export_obj && !obj)) {
        if (export_obj) {
            std::error_code ec;
            std::filesystem::remove(dest,ec);
        }
        std::cerr<<"Mesh format or geometry invalid/unsupported.\n";
        return 1;
    }
    std::cout << "{\"valid\":true,\"mesh_objects\":" << result.mesh_objects
              << ",\"submeshes\":" << result.submeshes
              << ",\"vertices\":" << result.vertices
              << ",\"triangles\":" << result.triangles << "}\n";
    return 0;
}
