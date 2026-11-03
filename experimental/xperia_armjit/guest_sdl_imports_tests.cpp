#ifdef NDEBUG
#undef NDEBUG
#endif
#include "guest_sdl_host.hpp"
#include "guest_libc_bridge.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <set>
#include <string>
#include <vector>
using namespace xperia;
int main(){
    // No OpenGL window is needed to prove the imported original ARM ELF
    // function pointers map to valid distinct SVC veneers, not x86 pointers.
    GuestSdlHost host;
    GuestLibcBridge bionic;
    const std::array<const char*,105> imported={
        "glActiveTexture","glClientActiveTexture","glColorMask",
        "glPolygonOffset","glBindBuffer","glGenBuffers","glDeleteBuffers",
        "glBufferData","glBufferSubData","glReadPixels",
        "glCopyTexSubImage2D","glGenerateMipmapOES",
        "glTexParameteriv","glTexEnviv","glLoadMatrixx",
        "glMultMatrixx","glTranslatex","glRotatex","glScalex",
        "glCompressedTexImage2D","glCompressedTexSubImage2D",
        "glTexImage2D","glTexSubImage2D","glDrawElements",
        "glGenFramebuffersOES","glBindFramebufferOES",
        "glDeleteFramebuffersOES","glIsFramebufferOES",
        "glCheckFramebufferStatusOES","glFramebufferTexture2DOES",
        "glGetFramebufferAttachmentParameterivOES",
        "glGenRenderbuffersOES","glBindRenderbufferOES",
        "glDeleteRenderbuffersOES","glIsRenderbufferOES",
        "glRenderbufferStorageOES","glFramebufferRenderbufferOES",
        "glGetRenderbufferParameterivOES",
        "glBlendEquationOES","glBlendFuncSeparateOES",
        "glBlendEquationSeparateOES",
        "glPushMatrix",
        "glPopMatrix",
        "glIsEnabled",
        "glIsTexture",
        "glIsBuffer",
        "glStencilMask",
        "glStencilFunc",
        "glStencilOp",
        "glClearStencil",
        "glDepthRangef",
        "glLineWidthx",
        "glNormal3x",
        "glAlphaFuncx",
        "glMaterialf",
        "glMaterialx",
        "glLightf",
        "glLightx",
        "glLightModelf",
        "glFogf",
        "glFogx",
        "glPointSizex",
        "glSampleCoverage",
        "glSampleCoveragex",
        "glGetTexParameteriv",
        "glGetTexParameterfv",
        "glGetTexEnviv",
        "glGetTexEnvfv",
        "glGetBufferParameteriv",
        "glGetFixedv",
        "glGetPointerv",
        "glGetTexParameterxv",
        "glGetTexEnvxv",
        "glGetMaterialfv",
        "glGetLightfv",
        "glLightModelxv",
        "glLightModelfv",
        "glMaterialxv",
        "glFogxv",
        "glTexParameterxv",
        "glTexEnvxv",
        "glTexParameterfv",
        "glMultiTexCoord4f",
        "glMultiTexCoord4x",
        "glCopyTexImage2D",
        "glClipPlanef",
        "glGetClipPlanef",
        "glPointParameterf",
        "glPointParameterx",
        "glPointParameterfv",
        "glPointParameterxv",
        "glGetLightxv",
        "glGetMaterialxv",
        "glPolygonOffsetx",
        "glTexGenfOES",
        "glTexGeniOES",
        "glTexGenfvOES",
        "glTexGenivOES",
        "glTexGenxOES",
        "glTexGenxvOES",
        "glGetTexGenfvOES",
        "glGetTexGenivOES",
        "glGetTexGenxvOES",
        "glGetClipPlanex",
        "glClipPlanex"
    };
    std::set<std::uint32_t> distinct;
    for(const auto* name:imported){
        const auto it=host.symbols().find(name);
        assert(it!=host.symbols().end());
        const auto address=it->second;
        assert((address&3u)==0 && distinct.insert(address).second);
        std::uint32_t instruction=0;
        for(unsigned i=0;i<4;++i){
            std::uint8_t value=0;
            assert(host.code_byte(address+i,value));
            instruction|=std::uint32_t(value)<<(8*i);
        }
        assert((instruction&0xff000000u)==0xef000000u);
        const auto id=instruction&0x00ffffffu;
        assert(host.handles(id));
        assert(host.return_pc(id)==address+4u);
    }
    GuestElf32 elf;
    constexpr std::uint32_t base=0x10001000u;
    elf.segments.push_back({base,std::vector<std::uint8_t>(4096,0)});
    for(std::size_t i=0;i<imported.size();++i)
        elf.pending_imports.push_back({base+std::uint32_t(i*4u),
            21u,0u,imported[i],2u});
    std::string error;
    auto symbols=bionic.plan_guest_imports(elf,error,{},host.symbols());
    assert(error.empty()&&!symbols.empty());
    assert(bionic.unresolved_import_count()==0);
    assert(elf.bind_imports(symbols,error));
    for(std::size_t i=0;i<imported.size();++i)
        assert(elf.read_u32(base+std::uint32_t(i*4u))==
               host.symbols().at(imported[i]));
    return 0;
}
