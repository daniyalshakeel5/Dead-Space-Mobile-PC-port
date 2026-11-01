#include "ds_port/im_ui_image.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <vector>
using namespace ds_port;
namespace {
void word(std::vector<std::uint8_t>& out,std::uint32_t v){
    for(unsigned i=0;i<4;++i)out.push_back(static_cast<std::uint8_t>(v>>(8*i)));
}
void set32(std::vector<std::uint8_t>& out,std::size_t at,std::uint32_t v){
    for(unsigned i=0;i<4;++i)out[at+i]=std::uint8_t(v>>(i*8));
}
std::vector<std::uint8_t> make(bool two,std::size_t metadata=488){
    std::vector<std::uint8_t> out={
        0xab,'I','M','-','M','3','G',0xbb,13,10,26,10,0,0,0,0,0,0,0,0,0
    };
    auto object=[&](std::uint8_t type,const std::vector<std::uint8_t>& p){
        out.push_back(type);word(out,static_cast<std::uint32_t>(p.size()));
        out.insert(out.end(),p.begin(),p.end());
    };
    object(0,std::vector<std::uint8_t>(12));
    auto image=[&](std::size_t params){
        std::vector<std::uint8_t> body;
        word(body,0);word(body,0);
        word(body,params?1u:0u);
        if(params){word(body,500);word(body,static_cast<std::uint32_t>(params));
            body.insert(body.end(),params,0x77);}
        body.push_back(100);body.push_back(0);
        word(body,2);word(body,2);
        word(body,0);word(body,16);
        for(unsigned k=0;k<16;++k)body.push_back(static_cast<std::uint8_t>(k+1));
        object(10,body);
    };
    image(metadata);
    if(two)image(0);
    word(out,0);set32(out,13,static_cast<std::uint32_t>(out.size()-12));
    return out;
}
}
int main(){
    auto one=make(false);
    auto r=decode_im_ui_rgba_images(one.data(),one.size());
    assert(r.decoded && r.images.size()==1);
    assert(r.images[0].width==2 && r.images[0].height==2);
    assert(r.images[0].pixels_rgba.size()==16);
    assert(r.images[0].pixels_rgba.front()==1);
    assert(r.images[0].pixels_rgba.back()==16);
    // Real original Dead Space splash uses 504 bytes of metadata
    // and the first frontend atlas image uses 29,728 bytes.
    for(auto metadata:{504u,29728u}){
        auto source=make(true,metadata);
        auto output=decode_im_ui_rgba_images(source.data(),source.size());
        assert(output.decoded&&output.images.size()==2);
        assert(output.images[0].pixels_rgba==output.images[1].pixels_rgba);
        source[source.size()-8]=0xff; // corrupt pixel, structural valid
        output=decode_im_ui_rgba_images(source.data(),source.size());
        assert(output.decoded);
        // Stop malformed object framing before pointer access.
        source[22]=0xff;source[23]=0xff;source[24]=0xff;source[25]=0x7f;
        output=decode_im_ui_rgba_images(source.data(),source.size());
        assert(!output.decoded);
    }
    auto broken=one;
    broken[12]=1;assert(!decode_im_ui_rgba_images(broken.data(),broken.size()).decoded);
    broken=one;broken[13]=0xff;
    assert(!decode_im_ui_rgba_images(broken.data(),broken.size()).decoded);
    broken=one;broken.back()=5; // proprietary checksum unverified
    assert(decode_im_ui_rgba_images(broken.data(),broken.size()).decoded);
    broken=one;
    broken[38]=11; // change type 10 to unsupported
    assert(!decode_im_ui_rgba_images(broken.data(),broken.size()).decoded);
    broken=one;
    // Image2D format field at first object's payload:
    // header 21 + 17, metadata user param 488, base 20.
    const auto format=21u+17u+20u+488u;
    assert(broken[format]==100u);
    broken[format]=116;
    assert(!decode_im_ui_rgba_images(broken.data(),broken.size()).decoded);
    broken=one;broken.resize(broken.size()-10);
    assert(!decode_im_ui_rgba_images(broken.data(),broken.size()).decoded);
    return 0;
}
