#include "guest_sdl_host.hpp"
#include <SDL_opengl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace xperia {
namespace {
constexpr GLenum kPointAttenuation=0x8129u;
constexpr GLenum kArrayBuffer=0x8892u;
constexpr GLenum kElementBuffer=0x8893u;
constexpr GLenum kModelViewMatrix=0x0ba6u;
constexpr GLenum kProjectionMatrix=0x0ba7u;
constexpr GLenum kTextureMatrix=0x0ba8u;
constexpr GLenum kTextureEnvColor=0x2201u;
constexpr GLenum kTextureBorderColor=0x1004u;
constexpr GLenum kFogColor=0x0b66u;
constexpr GLenum kSpotDirection=0x1204u;
constexpr GLenum kLightModelAmbient=0x0b53u;
constexpr GLenum kVertexArrayPointer=0x808eu;
constexpr GLenum kNormalArrayPointer=0x808fu;
constexpr GLenum kColorArrayPointer=0x8090u;
constexpr GLenum kTexCoordArrayPointer=0x8092u;
constexpr GLenum kViewPort=0x0ba2u;
constexpr GLenum kScissorBox=0x0c10u;
constexpr GLenum kCurrentColor=0x0b00u;
constexpr GLenum kColorClear=0x0c22u;

template<typename T> T ext(const char* name){
    const auto address=SDL_GL_GetProcAddress(name);
    if(!address)
        throw std::runtime_error(std::string("Required Windows GL extension missing: ")+name);
    return reinterpret_cast<T>(address);
}
float xfixed(std::uint32_t bits) {
    return float(static_cast<std::int32_t>(bits))/65536.f;
}
std::uint32_t fixed_bits(float v){
    if(!std::isfinite(v))return 0;
    const auto scaled=std::clamp<double>(double(v)*65536.0,
                         double(std::numeric_limits<std::int32_t>::min()),
                         double(std::numeric_limits<std::int32_t>::max()));
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(std::round(scaled)));
}
float as_float(std::uint32_t bits){
    float result=0;
    std::memcpy(&result,&bits,sizeof(result));
    return result;
}
unsigned components(GLenum token) noexcept {
    switch(token){
        case kModelViewMatrix:case kProjectionMatrix:case kTextureMatrix:
            return 16;
        case kViewPort:case kScissorBox:case kCurrentColor:case kColorClear:
        case kFogColor:case kLightModelAmbient:case kTextureEnvColor:
        case kTextureBorderColor:
        case GL_AMBIENT:case GL_DIFFUSE:case GL_SPECULAR:case GL_EMISSION:
        case GL_POSITION:
            return 4;
        case kSpotDirection:case kPointAttenuation:return 3;
        case GL_MAX_VIEWPORT_DIMS:return 2;
        default:return 1;
    }
}
std::uint32_t unpack32(const std::uint8_t* p){
    return std::uint32_t(p[0]) | std::uint32_t(p[1])<<8u |
           std::uint32_t(p[2])<<16u | std::uint32_t(p[3])<<24u;
}
void write32(std::uint8_t* p,std::uint32_t v){
    for(unsigned k=0;k<4;++k)p[k]=std::uint8_t(v>>(8u*k));
}
std::array<GLfloat,16> read_fixed_vector(
        GuestElf32& guest,std::uint32_t address,unsigned count){
    if(count>16u)throw std::runtime_error("ARM GLES fixed vector element count cap");
    const auto* src=guest.read(address,count*4u);
    if(!src)throw std::runtime_error("ARM GLES fixed vector outside original guest memory");
    std::array<GLfloat,16> out{};
    for(unsigned i=0;i<count;++i)out[i]=xfixed(unpack32(src+i*4u));
    return out;
}
std::array<GLfloat,16> read_float_vector(
        GuestElf32& guest,std::uint32_t address,unsigned count){
    if(count>16u)throw std::runtime_error("ARM GLES float vector element count cap");
    const auto* src=guest.read(address,count*4u);
    if(!src)throw std::runtime_error("ARM GLES float vector outside original guest memory");
    std::array<GLfloat,16> out{};
    std::memcpy(out.data(),src,count*4u);
    return out;
}
void write_float_vector(GuestElf32& guest,std::uint32_t address,
                        const GLfloat* values,unsigned n,bool as_fixed){
    if(n>16u)throw std::runtime_error("ARM GLES query vector count cap");
    auto* out=guest.writeable(address,n*4u);
    if(!out||(address&3u))
        throw std::runtime_error("ARM GLES query destination invalid");
    for(unsigned i=0;i<n;++i){
        std::uint32_t word=0;
        if(as_fixed)word=fixed_bits(values[i]);
        else std::memcpy(&word,&values[i],4u);
        write32(out+i*4u,word);
    }
}
void write_int_vector(GuestElf32& guest,std::uint32_t address,
                      const GLint* values,unsigned n){
    if(n>16u)throw std::runtime_error("ARM GLES int query count cap");
    auto* out=guest.writeable(address,n*4u);
    if(!out||(address&3u))
        throw std::runtime_error("ARM GLES int query destination invalid");
    for(unsigned i=0;i<n;++i)write32(out+i*4u,std::uint32_t(values[i]));
}
} // namespace

bool GuestSdlHost::dispatch_legacy(
    std::uint32_t which,std::array<std::uint32_t,16>& regs,
    GuestElf32& guest,std::string& error) noexcept {
    try{
        if(!context_)
            throw std::runtime_error("Original GLES1 call made without Windows GL context");
        const auto a=[&](unsigned i){return get_arg(regs,guest,i);};
        const auto f=[&](unsigned i){return as_float(a(i));};
        const auto x=[&](unsigned i){return xfixed(a(i));};
        const auto query_float=[&](unsigned n,const auto& get,
                                  bool as_fixed=false){
            GLfloat values[16]={};
            get(values);
            write_float_vector(guest,a(n),values,components(a(1)),as_fixed);
        };
        std::uint32_t result=0;
        switch(which){
        case 105:glPushMatrix();break;
        case 106:glPopMatrix();break;
        case 107:result=glIsEnabled(a(0))?1u:0u;break;
        case 108:result=glIsTexture(a(0))?1u:0u;break;
        case 109:result=ext<GLboolean(*)(GLuint)>("glIsBuffer")(a(0))?1u:0u;break;
        case 110:glStencilMask(a(0));break;
        case 111:glStencilFunc(a(0),static_cast<GLint>(a(1)),a(2));break;
        case 112:glStencilOp(a(0),a(1),a(2));break;
        case 113:glClearStencil(static_cast<GLint>(a(0)));break;
        case 114:glDepthRange(f(0),f(1));break;
        case 115:glLineWidth(x(0));break;
        case 116:glNormal3f(x(0),x(1),x(2));break;
        case 117:glAlphaFunc(a(0),x(1));break;
        case 118:glMaterialf(a(0),a(1),f(2));break;
        case 119:glMaterialf(a(0),a(1),x(2));break;
        case 120:glLightf(a(0),a(1),f(2));break;
        case 121:glLightf(a(0),a(1),x(2));break;
        case 122:glLightModelf(a(0),f(1));break;
        case 123:glFogf(a(0),f(1));break;
        case 124:glFogf(a(0),x(1));break;
        case 125:glPointSize(x(0));break;
        case 126:
            ext<void(*)(GLclampf,GLboolean)>("glSampleCoverage")(f(0),a(1)!=0);
            break;
        case 127:
            ext<void(*)(GLclampf,GLboolean)>("glSampleCoverage")(x(0),a(1)!=0);
            break;
        case 128:case 129:case 130:case 131:{
            const unsigned n=components(a(1));
            if(which==128||which==130){
                GLint out[16]={};
                if(which==128)glGetTexParameteriv(a(0),a(1),out);
                else glGetTexEnviv(a(0),a(1),out);
                write_int_vector(guest,a(2),out,n);
            }else{
                GLfloat out[16]={};
                if(which==129)glGetTexParameterfv(a(0),a(1),out);
                else glGetTexEnvfv(a(0),a(1),out);
                write_float_vector(guest,a(2),out,n,false);
            }
            break;
        }
        case 132: { // glGetBufferParameteriv
            if(a(0)!=kArrayBuffer && a(0)!=kElementBuffer)
                throw std::runtime_error("GL buffer query target outside guest GLES1");
            GLint v=0;
            ext<void(*)(GLenum,GLenum,GLint*)>("glGetBufferParameteriv")(
                a(0),a(1),&v);
            write_int_vector(guest,a(2),&v,1u);
            break;
        }
        case 133:{ // glGetFixedv
            GLfloat out[16]={};
            const auto n=components(a(0));
            glGetFloatv(a(0),out);
            write_float_vector(guest,a(1),out,n,true);
            break;
        }
        case 134:{ // glGetPointerv - return original ARM array offset
            const auto target=a(0);
            std::uint32_t guest_ptr=0;
            if(target==kVertexArrayPointer)guest_ptr=vertex_.ptr;
            else if(target==kNormalArrayPointer)guest_ptr=normal_.ptr;
            else if(target==kColorArrayPointer)guest_ptr=color_.ptr;
            else if(target==kTexCoordArrayPointer)
                guest_ptr=texcoord_units_[client_texunit_].ptr;
            else throw std::runtime_error("Unsupported GLES array pointer query");
            auto* dest=guest.writeable(a(1),4u);
            if(!dest||(a(1)&3u))
                throw std::runtime_error("ARM GLES pointer query output invalid");
            write32(dest,guest_ptr);break;
        }
        case 135:case 136:{ // GL fixed texture state queries
            GLfloat out[16]={};
            const auto n=components(a(1));
            if(which==135)glGetTexParameterfv(a(0),a(1),out);
            else glGetTexEnvfv(a(0),a(1),out);
            write_float_vector(guest,a(2),out,n,true);
            break;
        }
        case 137:case 138: { // material/light float vector query
            GLfloat out[16]={};
            const auto n=components(a(1));
            if(which==137)glGetMaterialfv(a(0),a(1),out);
            else glGetLightfv(a(0),a(1),out);
            write_float_vector(guest,a(2),out,n,false);
            break;
        }
        case 139:{
            const auto out=read_fixed_vector(guest,a(1),components(a(0)));
            glLightModelfv(a(0),out.data());break;
        }
        case 140:{
            const auto out=read_float_vector(guest,a(1),components(a(0)));
            glLightModelfv(a(0),out.data());break;
        }
        case 141:{
            const auto out=read_fixed_vector(guest,a(2),components(a(1)));
            glMaterialfv(a(0),a(1),out.data());break;
        }
        case 142:{
            const auto out=read_fixed_vector(guest,a(1),components(a(0)));
            glFogfv(a(0),out.data());break;
        }
        case 143:{
            const auto out=read_fixed_vector(guest,a(2),components(a(1)));
            glTexParameterfv(a(0),a(1),out.data());break;
        }
        case 144:{
            const auto out=read_fixed_vector(guest,a(2),components(a(1)));
            glTexEnvfv(a(0),a(1),out.data());break;
        }
        case 145:{
            const auto out=read_float_vector(guest,a(2),components(a(1)));
            glTexParameterfv(a(0),a(1),out.data());break;
        }
        case 146:
            ext<void(*)(GLenum,GLfloat,GLfloat,GLfloat,GLfloat)>(
                "glMultiTexCoord4f")(a(0),f(1),f(2),f(3),f(4));break;
        case 147:
            ext<void(*)(GLenum,GLfloat,GLfloat,GLfloat,GLfloat)>(
                "glMultiTexCoord4f")(a(0),x(1),x(2),x(3),x(4));break;
        case 148: // glCopyTexImage2D takes eight original ARM arguments
            glCopyTexImage2D(a(0),static_cast<GLint>(a(1)),a(2),
                static_cast<GLint>(a(3)),static_cast<GLint>(a(4)),
                a(5),a(6),static_cast<GLint>(a(7)));
            break;
        case 149:case 168:{ // glClipPlanef / glClipPlanex
            const auto plane=which==149?
                read_float_vector(guest,a(1),4u):
                read_fixed_vector(guest,a(1),4u);
            GLdouble d[4]={};
            for(unsigned i=0;i<4u;++i)d[i]=plane[i];
            glClipPlane(a(0),d);break;
        }
        case 150:case 167:{ // glGetClipPlanef / glGetClipPlanex
            GLdouble d[4]={};
            glGetClipPlane(a(0),d);
            GLfloat floats[4]={};
            for(unsigned i=0;i<4u;++i)floats[i]=static_cast<GLfloat>(d[i]);
            write_float_vector(guest,a(1),floats,4u,which==167);
            break;
        }
        case 151:
            ext<void(*)(GLenum,GLfloat)>("glPointParameterf")(a(0),f(1));
            break;
        case 152:
            ext<void(*)(GLenum,GLfloat)>("glPointParameterf")(a(0),x(1));
            break;
        case 153:case 154:{
            const auto n=components(a(0));
            const auto vec=which==154?
                read_fixed_vector(guest,a(1),n):
                read_float_vector(guest,a(1),n);
            ext<void(*)(GLenum,const GLfloat*)>("glPointParameterfv")(
                a(0),vec.data());break;
        }
        case 155:case 156:{
            GLfloat out[16]={};
            const auto n=components(a(1));
            if(which==155)glGetLightfv(a(0),a(1),out);
            else glGetMaterialfv(a(0),a(1),out);
            write_float_vector(guest,a(2),out,n,true);break;
        }
        case 157:glPolygonOffset(x(0),x(1));break;
        case 158:glTexGenf(a(0),a(1),f(2));break;
        case 159:glTexGeni(a(0),a(1),static_cast<GLint>(a(2)));break;
        case 160:case 161:case 163:{
            const auto n=components(a(1));
            if(which==161){
                GLint value[16]={};
                const auto* ptr=guest.read(a(2),n*4u);
                if(!ptr)throw std::runtime_error("ARM glTexGeniv vector unmapped");
                for(unsigned i=0;i<n;++i)
                    value[i]=static_cast<GLint>(unpack32(ptr+i*4u));
                glTexGeniv(a(0),a(1),value);
            }else{
                const auto vec=which==163?
                    read_fixed_vector(guest,a(2),n):
                    read_float_vector(guest,a(2),n);
                glTexGenfv(a(0),a(1),vec.data());
            }
            break;
        }
        case 162:glTexGenf(a(0),a(1),x(2));break;
        case 164:case 165:case 166:{
            const auto n=components(a(1));
            if(which==165){
                GLint values[16]={};
                glGetTexGeniv(a(0),a(1),values);
                write_int_vector(guest,a(2),values,n);
            }else{
                GLfloat values[16]={};
                glGetTexGenfv(a(0),a(1),values);
                write_float_vector(guest,a(2),values,n,which==166);
            }
            break;
        }
        default:
            throw std::runtime_error("Unimplemented original GLES1 legacy slot "+
                                     std::to_string(which));
        }
        regs[0]=result;
        error.clear();return true;
    }catch(const std::exception& ex){
        error=std::string("Original Xperia fixed-function GLES1 #")+
              std::to_string(which)+": "+ex.what();
        return false;
    }
}
} // namespace xperia
