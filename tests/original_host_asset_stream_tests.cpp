#include "ds_port/original_host_asset_io.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
namespace fs=std::filesystem;
using ds_port::OriginalHostAssetStream;
using ds_port::original_runtime::OriginalEmbeddedProviderStatus;
using ds_port::original_runtime::OriginalOwnerWideLineStatus;
using ds_port::original_runtime::OriginalNativeUtf16String;
int main(){
    const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root=fs::temp_directory_path()/
        ("deadspace_host_stream_"+std::to_string(stamp));
    struct Clean {fs::path root;~Clean(){std::error_code ec;fs::remove_all(root,ec);}} clean{root};
    const auto parent=root/"assets"/"published"/"data";
    fs::create_directories(parent);
    const std::string payload="FIRST\r\nNEXT=2\n\xE2\x82\xAC\n";
    {
        std::ofstream out(parent/"game_test.txt",std::ios::binary);
        assert(out.good());
        out.write(payload.data(),static_cast<std::streamsize>(payload.size()));
        assert(out.good());
    }
    OriginalHostAssetStream stream;
    bool found=false;
    assert(stream.open(root,"published/data/game_test.txt",4096,&found)&&found);
    assert(stream.size_bytes()==payload.size());
    assert(stream.position().status==OriginalEmbeddedProviderStatus::Completed);
    assert(stream.position().original_r0==0u);
    char bytes[16]{};
    auto read=stream.read(bytes,5);
    assert(read.status==OriginalEmbeddedProviderStatus::Completed);
    assert(read.original_r0==5u&&std::string(bytes,5)=="FIRST");
    auto remaining=stream.position(2);
    assert(remaining.status==OriginalEmbeddedProviderStatus::Completed);
    assert(remaining.original_r0==payload.size()-5u);
    assert(stream.seek(0,0).original_r0==1u);
    OriginalNativeUtf16String line{};
    auto result=stream.read_utf16_line(line);
    assert(result.status==OriginalOwnerWideLineStatus::LineDecoded);
    assert(line.contents==u"FIRST");
    result=stream.read_utf16_line(line);
    assert(result.status==OriginalOwnerWideLineStatus::LineDecoded);
    assert(line.contents==u"NEXT=2");
    result=stream.read_utf16_line(line);
    assert(result.status==OriginalOwnerWideLineStatus::LineDecoded);
    assert(line.contents==u"\u20ac");
    result=stream.read_utf16_line(line);
    assert(result.status==OriginalOwnerWideLineStatus::EndOfStream);
    assert(line.contents.empty());
    stream.close();
    assert(!stream.is_open()&&stream.size_bytes()==0u);
    assert(stream.read(bytes,5).status==OriginalEmbeddedProviderStatus::InvalidHostFileBinding);
    assert(!stream.open(root,"../outside",4096,&found)&&!found);
    assert(!stream.open(root,"published/data/game_test.txt",5,&found)&&found);
    assert(!stream.is_open());
    assert(stream.open(root,"published/data/game_test.txt",4096));
    assert(stream.position().original_r0==0u);
    return 0;
}
