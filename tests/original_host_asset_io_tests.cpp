#ifdef NDEBUG
#undef NDEBUG
#endif
#include "ds_port/original_host_asset_io.hpp"
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
namespace fs=std::filesystem;
int main() {
    const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root=fs::temp_directory_path()/
        ("deadspace_native_original_asset_io_"+std::to_string(stamp));
    struct Clean {fs::path root;~Clean(){std::error_code e;fs::remove_all(root,e);}} clean{root};
    const auto parent=root/"assets"/"published"/"models";
    fs::create_directories(parent);
    const auto file=parent/"main_menu.m3g";
    const std::vector<std::uint8_t> fixture={0,1,2,255,0,21,78,65,9};
    {
        std::ofstream out(file,std::ios::binary);
        assert(out.good());
        out.write(reinterpret_cast<const char*>(fixture.data()),
                  static_cast<std::streamsize>(fixture.size()));
        assert(out.good());
    }
    std::vector<std::uint8_t> read;
    bool found=false;
    assert(read_original_asset_file(root,"published/models/main_menu.m3g",
        1024,read,&found));
    assert(found&&read==fixture);
    assert(read_original_asset_file(root,"assets/published/models/main_menu.m3g",
        1024,read,&found)&&read==fixture);
    assert(!read_original_asset_file(root,"published/models/main_menu.m3g",
        fixture.size()-1,read,&found));
    assert(found&&read.empty());
    assert(!read_original_asset_file(root,"../models/main_menu.m3g",
        1024,read,&found));
    assert(!found&&read.empty());
    assert(!read_original_asset_file(root,"published/models/missing.m3g",
        1024,read,&found));
    assert(!found&&read.empty());
    assert(!read_original_asset_file(root,"published/models/main_menu.m3g",
        0,read,&found));
    assert(!found&&read.empty());
    const auto empty=parent/"empty.m3g";
    {std::ofstream out(empty,std::ios::binary);assert(out.good());}
    assert(!read_original_asset_file(root,"published/models/empty.m3g",
        1024,read,&found));
    assert(found&&read.empty());
    return 0;
}
