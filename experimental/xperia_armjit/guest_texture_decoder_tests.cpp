#ifdef NDEBUG
#undef NDEBUG
#endif
#include "guest_texture_decoder.hpp"
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
using namespace xperia::texture;
int main(){
    std::size_t n=0;std::string err;
    assert(expected_size(ATC_RGB,4,4,n,err)&&n==8);
    assert(expected_size(ATC_RGB,5,5,n,err)&&n==32);
    assert(expected_size(ATC_RGBA_EXPLICIT,4,4,n,err)&&n==16);
    assert(expected_size(ATC_RGBA_INTERPOLATED,4,4,n,err)&&n==16);
    assert(expected_size(PVRTC_RGB_4BPP,4,4,n,err)&&n==32);
    assert(expected_size(PVRTC_RGBA_2BPP,4,4,n,err)&&n==32);
    assert(expected_size(PVRTC_RGB_2BPP,16,8,n,err)&&n==32);
    assert(!expected_size(ATC_RGB,0,4,n,err));
    assert(!expected_size(PVRTC_RGBA_4BPP,32768,32768,n,err));
    assert(!expected_size(0xffffffff,4,4,n,err));
    std::vector<std::uint8_t> rgba;
    // ATC color block: endpoint0 solid RGB555 red, all indices select 0.
    const std::uint8_t red[8]={0x00,0x7c,0,0,0,0,0,0};
    assert(decode(ATC_RGB,4,4,red,sizeof(red),rgba,err));
    assert(rgba.size()==64);
    for(std::size_t p=0;p<64;p+=4){
        assert(rgba[p]==255 && rgba[p+1]==0 &&
               rgba[p+2]==0 && rgba[p+3]==255);
    }
    assert(!decode(ATC_RGB,4,4,red,7,rgba,err));
    assert(rgba.empty() && !err.empty());
    // Every alpha nibble 0xf in an ATC explicit-alpha block is opaque.
    std::uint8_t explicit_alpha[16]={};
    for(unsigned i=0;i<8;++i)explicit_alpha[i]=0xff;
    explicit_alpha[8]=0x00;explicit_alpha[9]=0x7c; // red endpoint
    assert(decode(ATC_RGBA_EXPLICIT,4,4,explicit_alpha,16,rgba,err));
    assert(rgba.size()==64 && rgba[3]==255 && rgba[63]==255);
    explicit_alpha[0]=0x0f;
    assert(decode(ATC_RGBA_EXPLICIT,4,4,explicit_alpha,16,rgba,err));
    assert(rgba[3]==255 && rgba[7]==0); // low/high alpha nibbles
    // Interpolated-alpha endpoints: selector 0 returns alpha0.
    std::uint8_t interp[16]={};
    interp[0]=73;interp[1]=5;
    interp[8]=0;interp[9]=0x7c;
    assert(decode(ATC_RGBA_INTERPOLATED,4,4,interp,16,rgba,err));
    assert(rgba[3]==73 && rgba[63]==73);
    // PVRTC four and two bpp dispatch both use MIT-licensed SDK decoder.
    std::vector<std::uint8_t> zeros(32,0);
    assert(decode(PVRTC_RGB_4BPP,8,8,zeros.data(),zeros.size(),rgba,err));
    assert(rgba.size()==8*8*4);
    for(std::size_t p=3;p<rgba.size();p+=4)assert(rgba[p]==255);
    assert(decode(PVRTC_RGBA_2BPP,16,8,zeros.data(),zeros.size(),rgba,err));
    assert(rgba.size()==16*8*4);
    assert(!decode(PVRTC_RGBA_2BPP,16,8,zeros.data(),16,rgba,err));
    return 0;
}
