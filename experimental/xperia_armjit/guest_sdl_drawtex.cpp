#include "guest_sdl_host.hpp"
#include <SDL_opengl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace xperia {
namespace {
float decode_float(std::uint32_t bits){
    float value=0;
    std::memcpy(&value,&bits,4);
    return value;
}
float decode_fixed(std::uint32_t bits){
    return float(static_cast<std::int32_t>(bits))/65536.f;
}
std::uint32_t get_word(const std::uint8_t* p){
    return std::uint32_t(p[0]) |
           std::uint32_t(p[1])<<8u |
           std::uint32_t(p[2])<<16u |
           std::uint32_t(p[3])<<24u;
}
std::array<float,5> decode_oes_rect(
    std::uint32_t which,const std::array<std::uint32_t,16>& r,
    GuestElf32& guest) {
    std::array<float,5> args{};
    const unsigned type=unsigned((which-169u)%4u);
    const bool vector=(which-169u)>=4u;
    if(vector) {
        const unsigned step=type==2u?2u:4u;
        const auto* data=guest.read(r[0],step*5u);
        if(!data)
            throw std::runtime_error("Original ARM glDrawTex vector outside guest memory");
        for(unsigned i=0;i<5u;++i) {
            const auto bits=step==2u?
                std::uint32_t(std::uint16_t(data[i*2u])|
                       std::uint16_t(data[i*2u+1])<<8u):
                get_word(data+i*4u);
            if(type==0u)args[i]=decode_float(bits);
            if(type==1u)args[i]=float(static_cast<std::int32_t>(bits));
            if(type==2u)args[i]=float(static_cast<std::int16_t>(bits));
            if(type==3u)args[i]=decode_fixed(bits);
        }
    } else {
        for(unsigned i=0;i<5u;++i){
            // Fifth argument is at the ARM AAPCS stack top, not r4.
            const auto bits=i<4u?r[i]:guest.read_u32(r[13]);
            if(type==0u)args[i]=decode_float(bits);
            if(type==1u)args[i]=float(static_cast<std::int32_t>(bits));
            if(type==2u)args[i]=float(static_cast<std::int16_t>(bits));
            if(type==3u)args[i]=decode_fixed(bits);
        }
    }
    for(const auto value:args)
        if(!std::isfinite(value) || std::abs(value)>32768.f)
            throw std::runtime_error("Original glDrawTex coordinates invalid/out of bounds");
    return args;
}
} // namespace

bool GuestSdlHost::dispatch_draw_tex(
    std::uint32_t which,std::array<std::uint32_t,16>& regs,
    GuestElf32& guest,std::string& error) noexcept {
    try {
        if(which<169u||which>176u||!context_)
            throw std::runtime_error("OES texture draw without valid host context");
        const auto rect=decode_oes_rect(which,regs,guest);
        const float x=rect[0],y=rect[1],
            z=std::clamp(rect[2],0.f,1.f),w=rect[3],h=rect[4];
        if(w<=0.f||h<=0.f)
            throw std::runtime_error("OES glDrawTex requires positive width/height");
        GLint viewport[4]={};
        glGetIntegerv(GL_VIEWPORT,viewport);
        if(viewport[2]<=0||viewport[3]<=0)
            throw std::runtime_error("OES draw requires valid original game viewport");
        const auto original=bound_textures_[active_texture_unit_];
        if(!original)throw std::runtime_error("OES glDrawTex needs bound original texture");
        GLint texWidth=0,texHeight=0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&texWidth);
        glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&texHeight);
        if(texWidth<=0||texHeight<=0)
            throw std::runtime_error("OES glDrawTex source texture not allocated");
        auto crop=crop_rects_.find(original);
        const auto data=crop==crop_rects_.end()?
            std::array<std::int32_t,4>{0,0,texWidth,texHeight}:crop->second;
        const float u0=float(data[0])/float(texWidth),
                    v0=float(data[1])/float(texHeight),
                    u1=float(data[0]+data[2])/float(texWidth),
                    v1=float(data[1]+data[3])/float(texHeight);
        // GLES1 OES_draw_texture specifies window coordinates. The desktop
        // compatibility profile has no equivalent, so draw a 2D quad in a
        // temporary orthographic projection, restoring *all* game GL state.
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(viewport[0],viewport[0]+viewport[2],
                viewport[1],viewport[1]+viewport[3],-1.,1.);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glDisable(GL_CULL_FACE);
        const float zz=1.f-2.f*z; // preserve OES specified depth 0..1
        glBegin(GL_QUADS);
        glTexCoord2f(u0,v0);glVertex3f(x,y,zz);
        glTexCoord2f(u1,v0);glVertex3f(x+w,y,zz);
        glTexCoord2f(u1,v1);glVertex3f(x+w,y+h,zz);
        glTexCoord2f(u0,v1);glVertex3f(x,y+h,zz);
        glEnd();
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glPopAttrib();
        regs[0]=0;
        error.clear();return true;
    }catch(const std::exception& ex){
        error=std::string("Original Xperia OES screen rectangle #")+
              std::to_string(which)+": "+ex.what();
        return false;
    }
}
} // namespace xperia
