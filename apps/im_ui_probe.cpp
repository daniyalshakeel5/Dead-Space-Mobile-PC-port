#include "ds_port/im_ui_image.hpp"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>
int main(int argc,char** argv){
    if(argc!=2){
        std::cerr<<"usage: ds_im_ui_probe <original-owned-ui-pack.m3g>\n";
        return 2;
    }
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);
    if(!file)return 2;
    const auto n=file.tellg();
    if(n<0 || static_cast<std::uint64_t>(n)>64ull*1024*1024)
        return 2;
    std::vector<std::uint8_t> blob(static_cast<std::size_t>(n));
    file.seekg(0);
    if(!blob.empty() &&
       !file.read(reinterpret_cast<char*>(blob.data()),n))
        return 2;
    const auto images=ds_port::decode_im_ui_rgba_images(blob.data(),blob.size());
    if(!images.decoded){
        std::cerr<<"Invalid or unsupported original IM-M3G UI pack\n";
        return 1;
    }
    // No image bytes, proprietary metadata or pixel samples printed.
    std::cout<<"{\"decoded\":true,\"images\":[";
    for(std::size_t i=0;i<images.images.size();++i){
        if(i)std::cout<<",";
        std::cout<<"{\"width\":"<<images.images[i].width
                 <<",\"height\":"<<images.images[i].height
                 <<",\"rgba_bytes\":"<<images.images[i].pixels_rgba.size()
                 <<"}";
    }
    std::cout<<"]}\n";
    return 0;
}
