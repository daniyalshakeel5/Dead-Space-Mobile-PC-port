#include "guest_sdl_host.hpp"
#include "guest_texture_decoder.hpp"
#include <SDL_opengl.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace xperia {
namespace {
template<typename Fn> Fn resolve_gl(const char* name) {
    const auto p=SDL_GL_GetProcAddress(name);
    if(!p)throw std::runtime_error(std::string("Required desktop GL extension missing: ")+name);
    return reinterpret_cast<Fn>(p);
}
template<typename Fn> Fn resolve_fbo(const char* core,const char* extension) {
    auto* p=SDL_GL_GetProcAddress(core);
    if(!p)p=SDL_GL_GetProcAddress(extension);
    if(!p)throw std::runtime_error(
        std::string("Framebuffer GL function missing: ")+core+"/"+extension);
    return reinterpret_cast<Fn>(p);
}
constexpr GLenum kArrayBuffer=0x8892u,kElementBuffer=0x8893u;
constexpr GLenum kTextureEnvColor=0x2201u;
constexpr std::uint64_t kMaxBuffer=64u*1024u*1024u;
float fixed(std::uint32_t value) {
    return static_cast<float>(static_cast<std::int32_t>(value))/65536.f;
}
float number(std::uint32_t bits){float x;std::memcpy(&x,&bits,4);return x;}
constexpr int kDown=0x6000c,kMove=0x4000c,kUp=0x8000c;
const char* const names[]={
"glClear","glClearColor","glViewport","glEnable","glDisable",
"glBlendFunc","glBindTexture","glGenTextures","glDeleteTextures",
"glTexParameteri","glTexImage2D","glTexSubImage2D","glMatrixMode",
"glLoadIdentity","glTranslatef","glRotatef","glScalef","glOrthof",
"glEnableClientState","glDisableClientState","glVertexPointer",
"glTexCoordPointer","glColorPointer","glDrawArrays","glDrawElements",
"glGetError","glColor4f","glDepthFunc","glDepthMask","glFinish",
"glFlush","glClearDepthf","glFrustumf","glTexEnvf","glTexEnvi",
"glAlphaFunc","glShadeModel","glHint","glScissor","glPixelStorei",
"glTexParameterf","glLineWidth","glCullFace","glFrontFace",
"glGetString","glGetIntegerv","glGetFloatv","glGetBooleanv",
"glNormalPointer","glLightfv","glMaterialfv","glFogfv",
"glLoadMatrixf","glMultMatrixf","glNormal3f","glColor4ub",
"glPointSize","glTexEnvfv",
"glCompressedTexImage2D","glCompressedTexSubImage2D",
"glActiveTexture","glClientActiveTexture","glColorMask","glPolygonOffset",
"glBindBuffer","glGenBuffers","glDeleteBuffers","glBufferData",
"glBufferSubData","glReadPixels","glCopyTexSubImage2D",
"glGenerateMipmapOES","glTexParameteriv","glTexEnviv",
"glLoadMatrixx","glMultMatrixx","glTranslatex","glRotatex","glScalex",
"glClearColorx","glColor4x","glOrthox","glFrustumx","glClearDepthx",
"glDepthRangex","glTexEnvx","glTexParameterx","glTexCoord4f",
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
constexpr std::size_t count=sizeof(names)/sizeof(names[0]);
unsigned element_bytes(unsigned type){
    if(type==GL_FLOAT)return 4;
    if(type==GL_SHORT||type==GL_UNSIGNED_SHORT)return 2;
    if(type==GL_UNSIGNED_BYTE||type==GL_BYTE)return 1;
    return 0;
}
} // namespace
GuestSdlHost::GuestSdlHost(){
    for(unsigned i=0;i<count;++i){
        symbols_.emplace(names[i],veneer_base+static_cast<std::uint32_t>(veneers_.size()));
        for(auto word:{std::uint32_t(0xef000000u|(svc_base+i)),std::uint32_t(0xe12fff1eu)})
            for(unsigned b=0;b<4;++b)veneers_.push_back(std::uint8_t(word>>(8*b)));
    }
}
GuestSdlHost::~GuestSdlHost(){stop();}
bool GuestSdlHost::start(std::string& error,int width,int height){
    if(window_||width<320||height<240||width>4096||height>4096){
        error="Invalid or duplicate Xperia SDL display";return false;
    }
    if(SDL_InitSubSystem(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMECONTROLLER)){
        error=std::string("SDL initialisation: ")+SDL_GetError();return false;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    window_=SDL_CreateWindow("Dead Space Mobile - Xperia ARM",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,width,height,
        SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_SHOWN);
    if(!window_){error=SDL_GetError();stop();return false;}
    context_=SDL_GL_CreateContext(window_);
    if(!context_){error=std::string("OpenGL compatibility context: ")+SDL_GetError();stop();return false;}
    SDL_GL_MakeCurrent(window_,context_);
    SDL_GL_SetSwapInterval(1);
    SDL_AudioSpec want{},got{};
    want.freq=44100;want.format=AUDIO_S16SYS;want.channels=2;want.samples=2048;
    audio_device_=SDL_OpenAudioDevice(nullptr,0,&want,&got,0);
    if(audio_device_){
        if(got.freq==44100&&got.channels==2&&got.format==AUDIO_S16SYS)
            SDL_PauseAudioDevice(audio_device_,0);
        else {SDL_CloseAudioDevice(audio_device_);audio_device_=0;}
    }
    if(!audio_device_)std::clog<<"Xperia 44100Hz stereo PCM unavailable: "<<SDL_GetError()<<"\n";
    for(int i=0;i<SDL_NumJoysticks();++i)controller_open(i);
    last_tick_ms_=SDL_GetTicks64();
    error.clear();return true;
}
void GuestSdlHost::stop() noexcept{
    if(controller_){SDL_GameControllerClose(controller_);controller_=nullptr;}
    controller_id_=-1;
    if(audio_device_){SDL_ClearQueuedAudio(audio_device_);SDL_CloseAudioDevice(audio_device_);audio_device_=0;}
    if(context_){SDL_GL_DeleteContext(context_);context_=nullptr;}
    if(window_){SDL_DestroyWindow(window_);window_=nullptr;}
    SDL_QuitSubSystem(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMECONTROLLER);
}
void GuestSdlHost::controller_open(int index) noexcept{
    if(controller_||!SDL_IsGameController(index))return;
    controller_=SDL_GameControllerOpen(index);
    if(controller_){
        controller_id_=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller_));
        std::clog<<"Controller: "<<SDL_GameControllerName(controller_)<<"\n";
    }
}
void GuestSdlHost::button(GuestXperiaInput& input,int code,bool pressed) noexcept{
    using B=ds_port::Ds4Button;
    B b=B::Unknown;
    switch(code){
    case SDL_CONTROLLER_BUTTON_A:b=B::Cross;break;
    case SDL_CONTROLLER_BUTTON_B:b=B::Circle;break;
    case SDL_CONTROLLER_BUTTON_X:b=B::Square;break;
    case SDL_CONTROLLER_BUTTON_Y:b=B::Triangle;break;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:b=B::L1;break;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:b=B::R1;break;
    case SDL_CONTROLLER_BUTTON_START:b=B::Options;break;
    case SDL_CONTROLLER_BUTTON_BACK:b=B::Share;break;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:b=B::DpadUp;break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:b=B::DpadDown;break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:b=B::DpadLeft;break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:b=B::DpadRight;break;
    default:break;
    }
    if(b!=B::Unknown)input.button(b,pressed);
    // Xperia menus are touch operated; Cross taps the d-pad cursor.
    if(b==B::Cross){
        input.pointer(pressed?kDown:kUp,1000,3,cursor_x_,cursor_y_);
        cursor_down_=pressed;
    }
    if(code==SDL_CONTROLLER_BUTTON_DPAD_LEFT)cursor_dx_+=pressed?-1:1;
    if(code==SDL_CONTROLLER_BUTTON_DPAD_RIGHT)cursor_dx_+=pressed?1:-1;
    if(code==SDL_CONTROLLER_BUTTON_DPAD_UP)cursor_dy_+=pressed?-1:1;
    if(code==SDL_CONTROLLER_BUTTON_DPAD_DOWN)cursor_dy_+=pressed?1:-1;
    cursor_dx_=std::clamp(cursor_dx_,-1,1);
    cursor_dy_=std::clamp(cursor_dy_,-1,1);
}
void GuestSdlHost::pump(GuestXperiaInput& input,bool& quit) noexcept{
    SDL_Event e{};
    while(SDL_PollEvent(&e)){
        switch(e.type){
        case SDL_QUIT:quit=true;break;
        case SDL_CONTROLLERDEVICEADDED:controller_open(e.cdevice.which);break;
        case SDL_CONTROLLERDEVICEREMOVED:
            if(e.cdevice.which==controller_id_){
                input.disconnect();
                SDL_GameControllerClose(controller_);
                controller_=nullptr;controller_id_=-1;axes_.fill(0);
                cursor_dx_=cursor_dy_=0;
                for(int i=0;i<SDL_NumJoysticks();++i)controller_open(i);
            }
            break;
        case SDL_CONTROLLERBUTTONDOWN:case SDL_CONTROLLERBUTTONUP:
            if(e.cbutton.which==controller_id_)
                button(input,e.cbutton.button,e.type==SDL_CONTROLLERBUTTONDOWN);
            break;
        case SDL_CONTROLLERAXISMOTION:
            if(e.caxis.which==controller_id_&&e.caxis.axis<4u)
                axes_[e.caxis.axis]=e.caxis.value;
            break;
        case SDL_CONTROLLERTOUCHPADDOWN:
        case SDL_CONTROLLERTOUCHPADMOTION:
        case SDL_CONTROLLERTOUCHPADUP:
            if(e.ctouchpad.which==controller_id_)
                input.pointer(e.type==SDL_CONTROLLERTOUCHPADDOWN?kDown:
                    e.type==SDL_CONTROLLERTOUCHPADUP?kUp:kMove,
                    1000,4+e.ctouchpad.finger,
                    std::clamp(e.ctouchpad.x,0.f,1.f)*960.f,
                    std::clamp(e.ctouchpad.y,0.f,1.f)*544.f);
            break;
        case SDL_MOUSEBUTTONDOWN:case SDL_MOUSEBUTTONUP:
            if(e.button.button==SDL_BUTTON_LEFT){
                int w=960,h=544;SDL_GetWindowSize(window_,&w,&h);
                cursor_x_=e.button.x*960.f/std::max(1,w);
                cursor_y_=e.button.y*544.f/std::max(1,h);
                mouse_down_=e.type==SDL_MOUSEBUTTONDOWN;
                input.pointer(mouse_down_?kDown:kUp,1000,5,cursor_x_,cursor_y_);
            }
            break;
        case SDL_MOUSEMOTION:
            if(mouse_down_){
                int w=960,h=544;SDL_GetWindowSize(window_,&w,&h);
                input.pointer(kMove,1000,5,e.motion.x*960.f/std::max(1,w),
                              e.motion.y*544.f/std::max(1,h));
            }
            break;
        case SDL_WINDOWEVENT:
            if(e.window.event==SDL_WINDOWEVENT_FOCUS_LOST){
                input.disconnect();
                if(cursor_down_)input.pointer(kUp,1000,3,cursor_x_,cursor_y_);
                if(mouse_down_)input.pointer(kUp,1000,5,cursor_x_,cursor_y_);
                mouse_down_=cursor_down_=false;cursor_dx_=cursor_dy_=0;
            }
            break;
        default:break;
        }
    }
    const auto now=SDL_GetTicks64();
    const auto dt=std::min<double>(0.05,double(now-last_tick_ms_)/1000.);
    last_tick_ms_=now;
    cursor_x_=std::clamp(cursor_x_+float(420.*dt*cursor_dx_),0.f,959.f);
    cursor_y_=std::clamp(cursor_y_+float(420.*dt*cursor_dy_),0.f,543.f);
    if(cursor_down_&&(cursor_dx_||cursor_dy_))
        input.pointer(kMove,1000,3,cursor_x_,cursor_y_);
    input.sticks(axes_[0],axes_[1],axes_[2],axes_[3]);
}
void GuestSdlHost::swap() noexcept{if(window_&&context_)SDL_GL_SwapWindow(window_);}
bool GuestSdlHost::queue_pcm16(const std::uint8_t* data,std::size_t bytes,
                               std::string& error) noexcept{
    if(!audio_device_||(!data&&bytes)||bytes%4u||bytes>2u*1024u*1024u||
       SDL_GetQueuedAudioSize(audio_device_)>2u*1024u*1024u-bytes){
        error="PCM audio device missing or queue full/invalid";return false;
    }
    if(bytes&&SDL_QueueAudio(audio_device_,data,static_cast<Uint32>(bytes))!=0){
        error=std::string("SDL queued PCM: ")+SDL_GetError();return false;
    }
    error.clear();return true;
}
bool GuestSdlHost::audio_control(unsigned action,std::string& error) noexcept{
    if(!audio_device_){error="SDL audio device not initialized";return false;}
    if(action==0)SDL_PauseAudioDevice(audio_device_,0); // play
    else if(action==1)SDL_PauseAudioDevice(audio_device_,1); // stop
    else if(action==2)SDL_ClearQueuedAudio(audio_device_); // release
    else {error="Unknown AudioTrack lifecycle command";return false;}
    error.clear();return true;
}
bool GuestSdlHost::code_byte(std::uint32_t addr,std::uint8_t& value) const noexcept{
    if(addr<veneer_base||std::uint64_t(addr)-veneer_base>=veneers_.size())return false;
    value=veneers_[addr-veneer_base];return true;
}
bool GuestSdlHost::handles(std::uint32_t id) const noexcept{
    return id>=svc_base&&id<svc_base+count;
}
std::uint32_t GuestSdlHost::return_pc(std::uint32_t id) const noexcept{
    return handles(id)?veneer_base+(id-svc_base)*8u+4u:0u;
}
std::uint32_t GuestSdlHost::get_arg(const std::array<std::uint32_t,16>& r,
                                   GuestElf32& g,unsigned i){
    if(i<4)return r[i];
    if(i>15)throw std::runtime_error("GLES1 ARM argument count cap");
    return g.read_u32(r[13]+(i-4)*4u);
}
bool GuestSdlHost::apply_arrays(GuestElf32& g,std::uint32_t last,
                                std::string& error) noexcept {
    const auto bind=[&](const ArrayDesc& d,int which) {
        if(!d.enabled)return true;
        const auto bytes=element_bytes(d.type);
        if(!bytes||d.size<1||d.size>4||d.stride<0)return false;
        const std::uint64_t stride=d.stride?d.stride:d.size*bytes;
        const auto n=std::uint64_t(last)*stride+d.size*bytes;
        if(n>64u*1024u*1024u)return false;
        const void* ptr=nullptr;
        if(d.buffer){
            const auto it=buffer_sizes_.find(d.buffer);
            if(it==buffer_sizes_.end() || std::uint64_t(d.ptr)+n>it->second)return false;
            auto bind_buffer=resolve_gl<void(*)(GLenum,GLuint)>("glBindBuffer");
            bind_buffer(kArrayBuffer,d.buffer);
            ptr=reinterpret_cast<const void*>(std::uintptr_t(d.ptr));
        }else{
            if(array_buffer_){
                auto bind_buffer=resolve_gl<void(*)(GLenum,GLuint)>("glBindBuffer");
                bind_buffer(kArrayBuffer,0u);
            }
            ptr=g.read(d.ptr,static_cast<std::size_t>(n));
            if(!ptr)return false;
        }
        if(which==0)glVertexPointer(d.size,d.type,d.stride,ptr);
        if(which==1)glColorPointer(d.size,d.type,d.stride,ptr);
        if(which>=2 && which<2+int(kMaxTextureUnits)) {
            const auto unit=unsigned(which-2);
            resolve_gl<void(*)(GLenum)>("glClientActiveTexture")(0x84c0u+unit);
            glTexCoordPointer(d.size,d.type,d.stride,ptr);
        }
        if(which==11)glNormalPointer(d.type,d.stride,ptr);
        return true;
    };
    try{
        if(!bind(vertex_,0)||!bind(color_,1)||!bind(normal_,11)){
            error="GLES1 client array refers to unmapped ARM data or VBO range";
            return false;
        }
        for(unsigned unit=0;unit<kMaxTextureUnits;++unit){
            if(!bind(texcoord_units_[unit],int(unit)+2)){
                error="GLES1 multitexture coordinate unit "+
                    std::to_string(unit)+" outside ARM/VBO memory";
                return false;
            }
        }
        resolve_gl<void(*)(GLenum)>("glClientActiveTexture")(
            0x84c0u+client_texunit_);
        if(array_buffer_)
            resolve_gl<void(*)(GLenum,GLuint)>("glBindBuffer")(kArrayBuffer,array_buffer_);
        return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
bool GuestSdlHost::dispatch(std::uint32_t id,std::array<std::uint32_t,16>& r,
                             GuestElf32& guest,std::string& error) noexcept {
    try {
        if(!handles(id)||!context_)
            throw std::runtime_error("OpenGL guest call without real SDL context");
        const auto a=[&](unsigned i){return get_arg(r,guest,i);};
        const auto f=[&](unsigned i){return number(a(i));};
        const auto which=id-svc_base;
        std::uint32_t result=0;
        switch(which) {
        case 0:glClear(a(0));break;
        case 1:glClearColor(f(0),f(1),f(2),f(3));break;
        case 2:glViewport(static_cast<GLint>(a(0)),static_cast<GLint>(a(1)),
                          a(2),a(3));break;
        case 3:glEnable(a(0));break;
        case 4:glDisable(a(0));break;
        case 5:glBlendFunc(a(0),a(1));break;
        case 6:glBindTexture(a(0),a(1));break;
        case 7:case 8: {
            const auto n=a(0);
            if(n>4096u)throw std::runtime_error("Texture allocation count cap");
            std::vector<GLuint> textures(n);
            if(which==7){
                auto* p=guest.writeable(a(1),n*4u);
                if(n&&!p)throw std::runtime_error("Texture output guest address unmapped");
                glGenTextures(n,textures.data());
                for(unsigned i=0;i<n;++i)for(unsigned j=0;j<4;++j)
                    p[i*4u+j]=std::uint8_t(textures[i]>>(j*8u));
            }else{
                const auto* p=guest.read(a(1),n*4u);
                if(n&&!p)throw std::runtime_error("Texture names guest address unmapped");
                for(unsigned i=0;i<n;++i)for(unsigned j=0;j<4;++j)
                    textures[i]|=GLuint(p[i*4u+j])<<(j*8u);
                glDeleteTextures(n,textures.data());
            }break;
        }
        case 9:glTexParameteri(a(0),a(1),a(2));break;
        case 10:case 11: {
            const auto width=a(which==10?3:4);
            const auto height=a(which==10?4:5);
            const auto fmt=a(6),type=a(7),ptr=a(8);
            unsigned components=0;
            if(fmt==GL_RGBA)components=4;
            else if(fmt==GL_RGB)components=3;
            else if(fmt==GL_ALPHA||fmt==GL_LUMINANCE)components=1;
            else if(fmt==GL_LUMINANCE_ALPHA)components=2;
            const bool packed=type==GL_UNSIGNED_SHORT_5_6_5||
                type==GL_UNSIGNED_SHORT_4_4_4_4||
                type==GL_UNSIGNED_SHORT_5_5_5_1;
            if((packed && ((type==GL_UNSIGNED_SHORT_5_6_5&&fmt!=GL_RGB)||
               (type!=GL_UNSIGNED_SHORT_5_6_5&&fmt!=GL_RGBA))) ||
               (!packed && type!=GL_UNSIGNED_BYTE) ||
               !components || width>8192||height>8192)
                throw std::runtime_error("Unsupported GLES1 texture component/type combination");
            const unsigned pixel_bytes=packed?2u:components;
            GLint align=4;glGetIntegerv(GL_UNPACK_ALIGNMENT,&align);
            if(align!=1&&align!=2&&align!=4&&align!=8)
                throw std::runtime_error("Invalid GL pixel unpack alignment");
            const auto row=std::uint64_t(width)*pixel_bytes;
            const auto pitch=(row+unsigned(align)-1u)&~(unsigned(align)-1u);
            const auto bytes=height?pitch*(height-1u)+row:0u;
            if(bytes>64u*1024u*1024u)
                throw std::runtime_error("Texture upload exceeds 64MiB");
            const auto* pixels=ptr?guest.read(ptr,static_cast<std::size_t>(bytes)):nullptr;
            if(ptr&&!pixels)throw std::runtime_error("Texture pixels not mapped in guest");
            if(which==10)
                glTexImage2D(a(0),a(1),a(2),width,height,a(5),fmt,type,pixels);
            else glTexSubImage2D(a(0),a(1),a(2),a(3),width,height,fmt,type,pixels);
            break;
        }
        case 12:glMatrixMode(a(0));break;
        case 13:glLoadIdentity();break;
        case 14:glTranslatef(f(0),f(1),f(2));break;
        case 15:glRotatef(f(0),f(1),f(2),f(3));break;
        case 16:glScalef(f(0),f(1),f(2));break;
        case 17:glOrtho(f(0),f(1),f(2),f(3),f(4),f(5));break;
        case 18:case 19:{
            const bool enabled=which==18;const auto cap=a(0);
            if(enabled)glEnableClientState(cap);else glDisableClientState(cap);
            if(cap==GL_VERTEX_ARRAY)vertex_.enabled=enabled;
            if(cap==GL_COLOR_ARRAY)color_.enabled=enabled;
            if(cap==GL_TEXTURE_COORD_ARRAY)
                texcoord_units_[client_texunit_].enabled=enabled;
            if(cap==GL_NORMAL_ARRAY)normal_.enabled=enabled;
            break;
        }
        case 20:vertex_={static_cast<int>(a(0)),static_cast<int>(a(1)),
                          static_cast<int>(a(2)),a(3),vertex_.enabled,array_buffer_};break;
        case 21:{
            auto& tex=texcoord_units_[client_texunit_];
            tex={static_cast<int>(a(0)),static_cast<int>(a(1)),
                 static_cast<int>(a(2)),a(3),tex.enabled,array_buffer_};
            break;
        }
        case 22:color_={static_cast<int>(a(0)),static_cast<int>(a(1)),
                         static_cast<int>(a(2)),a(3),color_.enabled,array_buffer_};break;
        case 23:{
            const auto first=a(1),n=a(2);
            if(first>1000000u||n>1000000u||std::uint64_t(first)+n>1000000u)
                throw std::runtime_error("ARM guest vertex count cap");
            if(n&&!apply_arrays(guest,first+n-1u,error))return false;
            glDrawArrays(a(0),first,n);break;
        }
        case 24:{
            const auto n=a(1),type=a(2),ptr=a(3);
            if(n>1000000u||(type!=GL_UNSIGNED_BYTE&&type!=GL_UNSIGNED_SHORT))
                throw std::runtime_error("Unsupported indexed draw size/type");
            if(!n)break;
            const auto length=std::size_t(n)*(type==GL_UNSIGNED_SHORT?2u:1u);
            std::vector<std::uint8_t> readback;
            const std::uint8_t* indices=nullptr;
            if(element_buffer_){
                const auto it=buffer_sizes_.find(element_buffer_);
                if(it==buffer_sizes_.end() || std::uint64_t(ptr)+length>it->second)
                    throw std::runtime_error("Original GLES EBO draw exceeds allocated index buffer");
                readback.resize(length);
                resolve_gl<void(*)(GLenum,std::ptrdiff_t,std::ptrdiff_t,void*)>(
                    "glGetBufferSubData")(kElementBuffer,
                    static_cast<std::ptrdiff_t>(ptr),
                    static_cast<std::ptrdiff_t>(length),readback.data());
                indices=readback.data();
            }else{
                indices=guest.read(ptr,length);
                if(!indices)throw std::runtime_error("Guest draw indices unmapped");
            }
            unsigned highest=0;
            for(unsigned i=0;i<n;++i){
                unsigned index=type==GL_UNSIGNED_BYTE?indices[i]:
                    unsigned(indices[2*i])|(unsigned(indices[2*i+1])<<8u);
                highest=std::max(highest,index);
            }
            if(!apply_arrays(guest,highest,error))return false;
            const void* draw_pointer=element_buffer_
                ?reinterpret_cast<const void*>(std::uintptr_t(ptr))
                :static_cast<const void*>(indices);
            glDrawElements(a(0),n,type,draw_pointer);break;
        }
        case 25:result=glGetError();break;
        case 26:glColor4f(f(0),f(1),f(2),f(3));break;
        case 27:glDepthFunc(a(0));break;
        case 28:glDepthMask(static_cast<GLboolean>(a(0)));break;
        case 29:glFinish();break;
        case 30:glFlush();break;
        case 31:glClearDepth(static_cast<double>(f(0)));break;
        case 32:glFrustum(f(0),f(1),f(2),f(3),f(4),f(5));break;
        case 33:glTexEnvf(a(0),a(1),f(2));break;
        case 34:glTexEnvi(a(0),a(1),a(2));break;
        case 35:glAlphaFunc(a(0),f(1));break;
        case 36:glShadeModel(a(0));break;
        case 37:glHint(a(0),a(1));break;
        case 38:glScissor(a(0),a(1),a(2),a(3));break;
        case 39:glPixelStorei(a(0),a(1));break;
        case 40:glTexParameterf(a(0),a(1),f(2));break;
        case 41:glLineWidth(f(0));break;
        case 42:glCullFace(a(0));break;
        case 43:glFrontFace(a(0));break;
        case 44: { // glGetString: return ARM32 guest storage, never an x86 ptr
            const auto enum_value=a(0);
            if(enum_value<GL_VENDOR||enum_value>GL_EXTENSIONS)
                throw std::runtime_error("Unsupported GLES string selector");
            constexpr std::uint32_t base=0x74500000u;
            if(!guest.read(base,1)){
                if(guest.read(base+65535u,1))
                    throw std::runtime_error("Guest OpenGL strings memory collision");
                guest.segments.push_back({base,std::vector<std::uint8_t>(65536,0)});
            }
            const char* reported=nullptr;
            if(enum_value==GL_VERSION)
                reported="OpenGL ES-CM 1.1 Xperia ARM32 Windows wrapper";
            else if(enum_value==GL_EXTENSIONS)
                reported="GL_AMD_compressed_ATC_texture "
                         "GL_IMG_texture_compression_pvrtc";
            else{
                const auto* src=glGetString(enum_value);
                if(!src)throw std::runtime_error("Host OpenGL query string unavailable");
                reported=reinterpret_cast<const char*>(src);
            }
            const auto size=std::strlen(reported);
            if(size>=16384u)
                throw std::runtime_error("Host GL string exceeds bounded guest buffer");
            const auto guest_addr=base+(enum_value-GL_VENDOR)*16384u;
            auto* dst=guest.writeable(guest_addr,size+1u);
            if(!dst)throw std::runtime_error("Guest GL string buffer unmapped");
            std::memcpy(dst,reported,size+1u);
            result=guest_addr;break;
        }
        case 45:case 46:case 47: { // glGet{Integer,Float,Boolean}v
            const auto p=a(0);
            const unsigned components=p==GL_VIEWPORT||p==GL_SCISSOR_BOX||
                p==GL_CURRENT_COLOR||p==GL_COLOR_CLEAR_VALUE?4u:
                p==GL_MODELVIEW_MATRIX||p==GL_PROJECTION_MATRIX||
                p==GL_TEXTURE_MATRIX?16u:p==GL_MAX_VIEWPORT_DIMS?2u:1u;
            const unsigned per=which==47?1u:4u;
            auto* dst=guest.writeable(a(1),components*per);
            if(!dst)throw std::runtime_error("GL query output not guest mapped");
            if(which==45){
                GLint values[16]={};glGetIntegerv(p,values);
                for(unsigned i=0;i<components;++i)
                    for(unsigned j=0;j<4;++j)
                        dst[i*4+j]=std::uint8_t(std::uint32_t(values[i])>>(8*j));
            }else if(which==46){
                GLfloat values[16]={};glGetFloatv(p,values);
                std::memcpy(dst,values,components*4u);
            }else{
                GLboolean values[16]={};glGetBooleanv(p,values);
                std::memcpy(dst,values,components);
            }
            break;
        }
        case 48:normal_={3,static_cast<int>(a(0)),static_cast<int>(a(1)),
                         a(2),normal_.enabled,array_buffer_};break;
        case 49:case 50:case 51:case 57:{ // light, material, fog, tex-env floats
            const auto ptr=a(which==51?1:2);
            const auto n=(which==51)?4u:4u;
            const auto* src=guest.read(ptr,n*4u);
            if(!src)throw std::runtime_error("GLES1 float vector not guest mapped");
            GLfloat value[4]={};std::memcpy(value,src,sizeof(value));
            if(which==49)glLightfv(a(0),a(1),value);
            if(which==50)glMaterialfv(a(0),a(1),value);
            if(which==51)glFogfv(a(0),value);
            if(which==57)glTexEnvfv(a(0),a(1),value);
            break;
        }
        case 52:case 53:{
            const auto* src=guest.read(a(0),16u*4u);
            if(!src)throw std::runtime_error("GLES1 matrix outside guest memory");
            GLfloat matrix[16];std::memcpy(matrix,src,sizeof(matrix));
            if(which==52)glLoadMatrixf(matrix);
            else glMultMatrixf(matrix);
            break;
        }
        case 54:glNormal3f(f(0),f(1),f(2));break;
        case 55:glColor4ub(static_cast<GLubyte>(a(0)),
                           static_cast<GLubyte>(a(1)),
                           static_cast<GLubyte>(a(2)),
                           static_cast<GLubyte>(a(3)));break;
        case 56:glPointSize(f(0));break;
        case 58:case 59: { // glCompressedTexImage2D / glCompressedTexSubImage2D
            // Both APIs take 8 or 9 ARM words; last arguments are on
            // the guest AAPCS stack, NOT native host pointers.
            const auto fmt=a(which==58?2u:6u);
            const auto width=static_cast<int>(a(which==58?3u:4u));
            const auto height=static_cast<int>(a(which==58?4u:5u));
            const auto n=static_cast<std::int32_t>(a(which==58?6u:7u));
            const auto ptr=a(which==58?7u:8u);
            std::size_t expected=0;
            std::string reason;
            if(!texture::expected_size(fmt,width,height,expected,reason))
                throw std::runtime_error(reason);
            if(n<0 || static_cast<std::uint64_t>(n)!=expected)
                throw std::runtime_error("Original compressed texture payload size mismatch");
            const auto* src=guest.read(ptr,expected);
            if(!src)
                throw std::runtime_error("Compressed texture ARM guest pointer unmapped");
            std::vector<std::uint8_t> rgba;
            if(!texture::decode(fmt,width,height,src,expected,rgba,reason))
                throw std::runtime_error(reason);
            // Software fallback is deterministic and works on GPUs without
            // AMD ATC or PowerVR PVRTC support. Preserve mip level and
            // texture state; never claim a successful upload if decode fails.
            if(which==58) {
                glTexImage2D(a(0),static_cast<GLint>(a(1)),GL_RGBA,width,height,
                    static_cast<GLint>(a(5)),GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
            }else{
                glTexSubImage2D(a(0),static_cast<GLint>(a(1)),
                    static_cast<GLint>(a(2)),static_cast<GLint>(a(3)),
                    width,height,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
            }
            const auto status=glGetError();
            if(status!=GL_NO_ERROR)
                throw std::runtime_error("Host OpenGL rejected decompressed RGBA texture, error "+std::to_string(status));
            break;
        }
        // These are directly evidenced by the Xperia v1.1.33 ELF import
        // inventory, not just a generic GLES compatibility wish list.
        case 60:
            resolve_gl<void(*)(GLenum)>("glActiveTexture")(a(0));break;
        case 61: {
            const auto texunit=a(0);
            if(texunit<0x84c0u || texunit>=0x84c0u+kMaxTextureUnits)
                throw std::runtime_error("GLES1 client texture unit exceeds supported slots");
            client_texunit_=unsigned(texunit-0x84c0u);
            resolve_gl<void(*)(GLenum)>("glClientActiveTexture")(texunit);
            break;
        }
        case 62:glColorMask(a(0)!=0,a(1)!=0,a(2)!=0,a(3)!=0);break;
        case 63:glPolygonOffset(f(0),f(1));break;
        case 64: { // glBindBuffer: VBO object identity belongs to host GL
            const auto target=a(0),name=a(1);
            if(target!=kArrayBuffer && target!=kElementBuffer)
                throw std::runtime_error("Unknown ARM GLES buffer binding target");
            resolve_gl<void(*)(GLenum,GLuint)>("glBindBuffer")(target,name);
            if(name)buffer_sizes_.try_emplace(name,0u);
            if(target==kArrayBuffer)array_buffer_=name;
            else element_buffer_=name;
            break;
        }
        case 65:case 66: { // glGenBuffers / glDeleteBuffers
            const auto n=a(0);
            if(n>4096)throw std::runtime_error("GLES buffer count cap");
            std::vector<GLuint> ids(n,0);
            if(which==65){
                auto* out=guest.writeable(a(1),n*4u);
                if(n&&!out)throw std::runtime_error("Unmapped GLES buffer output");
                resolve_gl<void(*)(GLsizei,GLuint*)>("glGenBuffers")(n,ids.data());
                for(unsigned i=0;i<n;++i){
                    buffer_sizes_[ids[i]]=0u;
                    for(unsigned k=0;k<4;++k)
                        out[i*4u+k]=std::uint8_t(ids[i]>>(k*8u));
                }
            }else{
                const auto* src=guest.read(a(1),n*4u);
                if(n&&!src)throw std::runtime_error("Unmapped GLES buffer deletion IDs");
                for(unsigned i=0;i<n;++i){
                    for(unsigned k=0;k<4;++k)
                        ids[i]|=GLuint(src[i*4u+k])<<(k*8u);
                    buffer_sizes_.erase(ids[i]);
                    if(ids[i]==array_buffer_)array_buffer_=0;
                    if(ids[i]==element_buffer_)element_buffer_=0;
                }
                resolve_gl<void(*)(GLsizei,const GLuint*)>("glDeleteBuffers")(n,ids.data());
            }
            break;
        }
        case 67:case 68: { // glBufferData / glBufferSubData
            const auto target=a(0);
            if(target!=kArrayBuffer && target!=kElementBuffer)
                throw std::runtime_error("GLES upload target not VBO/EBO");
            const auto bound=target==kArrayBuffer?array_buffer_:element_buffer_;
            if(!bound)throw std::runtime_error("GLES buffer transfer without bound object");
            std::uint64_t offset=0,bytes=0;
            std::uint32_t address=0;
            if(which==67){
                bytes=a(1);address=a(2);
            }else{
                offset=a(1);bytes=a(2);address=a(3);
            }
            if(bytes>kMaxBuffer || offset+bytes>kMaxBuffer)
                throw std::runtime_error("GLES buffer transfer exceeds 64MiB");
            if(which==68){
                const auto it=buffer_sizes_.find(bound);
                if(it==buffer_sizes_.end()||offset+bytes>it->second)
                    throw std::runtime_error("GLES sub-buffer write exceeds allocated buffer");
            }
            const void* source=nullptr;
            if(address && bytes) {
                source=guest.read(address,static_cast<std::size_t>(bytes));
                if(!source)throw std::runtime_error("GLES source ARM buffer unmapped");
            }else if(bytes && which==68)
                throw std::runtime_error("Null GLES sub-buffer source");
            if(which==67){
                resolve_gl<void(*)(GLenum,std::ptrdiff_t,const void*,GLenum)>("glBufferData")(
                    target,static_cast<std::ptrdiff_t>(bytes),source,a(3));
                buffer_sizes_[bound]=static_cast<std::size_t>(bytes);
            }else{
                resolve_gl<void(*)(GLenum,std::ptrdiff_t,std::ptrdiff_t,const void*)>("glBufferSubData")(
                    target,static_cast<std::ptrdiff_t>(offset),
                    static_cast<std::ptrdiff_t>(bytes),source);
            }
            break;
        }
        case 69: { // glReadPixels(x,y,width,height,format,type,pixels)
            const auto width=a(2),height=a(3),format=a(4),type=a(5);
            if(width>8192||height>8192||type!=GL_UNSIGNED_BYTE ||
               (format!=GL_RGBA && format!=GL_RGB))
                throw std::runtime_error("Unsupported GLES glReadPixels pixel layout");
            const unsigned components=format==GL_RGBA?4u:3u;
            GLint alignment=4;glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);
            if(alignment!=1&&alignment!=2&&alignment!=4&&alignment!=8)
                throw std::runtime_error("GL pixel pack alignment invalid");
            const std::uint64_t row=std::uint64_t(width)*components;
            const std::uint64_t pitch=(row+unsigned(alignment)-1u)&~(unsigned(alignment)-1u);
            const std::uint64_t bytes=height?pitch*(height-1u)+row:0u;
            if(bytes>kMaxBuffer)throw std::runtime_error("Guest framebuffer read >64MiB");
            auto* dest=guest.writeable(a(6),static_cast<std::size_t>(bytes));
            if(!dest && bytes)throw std::runtime_error("GLES framebuffer destination unmapped");
            glReadPixels(static_cast<GLint>(a(0)),static_cast<GLint>(a(1)),
                         width,height,format,type,dest);
            break;
        }
        case 70:
            glCopyTexSubImage2D(a(0),static_cast<GLint>(a(1)),
                               static_cast<GLint>(a(2)),static_cast<GLint>(a(3)),
                               static_cast<GLint>(a(4)),static_cast<GLint>(a(5)),
                               a(6),a(7));
            break;
        case 71:
            resolve_gl<void(*)(GLenum)>("glGenerateMipmap")(a(0));break;
        case 72:case 73:{ // glTexParameteriv / glTexEnviv
            const auto token=a(1);
            const auto n=(which==73 && token==kTextureEnvColor)?4u:1u;
            const auto* bytes=guest.read(a(2),n*4u);
            if(!bytes)throw std::runtime_error("GLES integer parameter array unmapped");
            GLint values[4]={};
            for(unsigned i=0;i<n;++i)
                for(unsigned k=0;k<4;++k)
                    values[i]|=GLint(std::uint32_t(bytes[i*4u+k])<<(k*8u));
            if(which==72)glTexParameteriv(a(0),token,values);
            else glTexEnviv(a(0),token,values);
            break;
        }
        case 74:case 75: { // glLoadMatrixx / glMultMatrixx
            const auto* src=guest.read(a(0),16u*4u);
            if(!src)throw std::runtime_error("ARM fixed-point matrix unmapped");
            GLfloat matrix[16]{};
            for(unsigned i=0;i<16;++i){
                const auto* p=src+i*4u;
                const auto fixedword=std::uint32_t(p[0]) |
                    std::uint32_t(p[1])<<8u |
                    std::uint32_t(p[2])<<16u |
                    std::uint32_t(p[3])<<24u;
                matrix[i]=fixed(fixedword);
            }
            if(which==74)glLoadMatrixf(matrix);
            else glMultMatrixf(matrix);
            break;
        }
        case 76:glTranslatef(fixed(a(0)),fixed(a(1)),fixed(a(2)));break;
        case 77:glRotatef(fixed(a(0)),fixed(a(1)),fixed(a(2)),fixed(a(3)));break;
        case 78:glScalef(fixed(a(0)),fixed(a(1)),fixed(a(2)));break;
        case 79:glClearColor(fixed(a(0)),fixed(a(1)),fixed(a(2)),fixed(a(3)));break;
        case 80:glColor4f(fixed(a(0)),fixed(a(1)),fixed(a(2)),fixed(a(3)));break;
        case 81:glOrtho(fixed(a(0)),fixed(a(1)),fixed(a(2)),fixed(a(3)),
                        fixed(a(4)),fixed(a(5)));break;
        case 82:glFrustum(fixed(a(0)),fixed(a(1)),fixed(a(2)),fixed(a(3)),
                          fixed(a(4)),fixed(a(5)));break;
        case 83:glClearDepth(static_cast<GLdouble>(fixed(a(0))));break;
        case 84:glDepthRange(fixed(a(0)),fixed(a(1)));break;
        case 85:glTexEnvi(a(0),a(1),static_cast<GLint>(a(2)));break;
        case 86:glTexParameteri(a(0),a(1),static_cast<GLint>(a(2)));break;
        case 87:glTexCoord4f(f(0),f(1),f(2),f(3));break;
        // Android GLES1 OES framebuffer extensions; desktop Win64
        // OpenGL implements the same operations under core/EXT names.
        case 88:case 90:case 95:case 97:{ // Gen/Delete FBO/RBO objects
            const auto n=a(0);
            if(n>4096u)throw std::runtime_error("GLES FBO object count cap");
            std::vector<GLuint> values(n,0u);
            if(which==88 || which==95){
                auto* guest_output=guest.writeable(a(1),n*4u);
                if(n&&!guest_output)
                    throw std::runtime_error("GLES FBO IDs guest output unmapped");
                if(which==88)
                    resolve_fbo<void(*)(GLsizei,GLuint*)>(
                        "glGenFramebuffers","glGenFramebuffersEXT")(n,values.data());
                else
                    resolve_fbo<void(*)(GLsizei,GLuint*)>(
                        "glGenRenderbuffers","glGenRenderbuffersEXT")(n,values.data());
                for(unsigned i=0;i<n;++i)
                    for(unsigned k=0;k<4;++k)
                        guest_output[i*4u+k]=std::uint8_t(values[i]>>(k*8u));
            }else{
                const auto* guest_ids=guest.read(a(1),n*4u);
                if(n&&!guest_ids)
                    throw std::runtime_error("GLES FBO ID deletion source unmapped");
                for(unsigned i=0;i<n;++i)
                    for(unsigned k=0;k<4;++k)
                        values[i]|=GLuint(guest_ids[i*4u+k])<<(k*8u);
                if(which==90)
                    resolve_fbo<void(*)(GLsizei,const GLuint*)>(
                        "glDeleteFramebuffers","glDeleteFramebuffersEXT")(n,values.data());
                else
                    resolve_fbo<void(*)(GLsizei,const GLuint*)>(
                        "glDeleteRenderbuffers","glDeleteRenderbuffersEXT")(n,values.data());
            }
            break;
        }
        case 89: // glBindFramebufferOES
            resolve_fbo<void(*)(GLenum,GLuint)>(
                "glBindFramebuffer","glBindFramebufferEXT")(a(0),a(1));
            break;
        case 91: // glIsFramebufferOES
            result=resolve_fbo<GLboolean(*)(GLuint)>(
                "glIsFramebuffer","glIsFramebufferEXT")(a(0))?1u:0u;
            break;
        case 92: // glCheckFramebufferStatusOES
            result=resolve_fbo<GLenum(*)(GLenum)>(
                "glCheckFramebufferStatus","glCheckFramebufferStatusEXT")(a(0));
            break;
        case 93: // glFramebufferTexture2DOES
            resolve_fbo<void(*)(GLenum,GLenum,GLenum,GLuint,GLint)>(
                "glFramebufferTexture2D","glFramebufferTexture2DEXT")(
                   a(0),a(1),a(2),a(3),static_cast<GLint>(a(4)));
            break;
        case 94: { // glGetFramebufferAttachmentParameterivOES
            auto* dst=guest.writeable(a(3),4u);
            if(!dst||(a(3)&3u))
                throw std::runtime_error("ARM FBO attachment query outptr invalid");
            GLint value=0;
            resolve_fbo<void(*)(GLenum,GLenum,GLenum,GLint*)>(
                "glGetFramebufferAttachmentParameteriv",
                "glGetFramebufferAttachmentParameterivEXT")(a(0),a(1),a(2),&value);
            for(unsigned i=0;i<4u;++i)
                dst[i]=std::uint8_t(std::uint32_t(value)>>(i*8u));
            break;
        }
        case 96: // glBindRenderbufferOES
            resolve_fbo<void(*)(GLenum,GLuint)>(
                "glBindRenderbuffer","glBindRenderbufferEXT")(a(0),a(1));
            break;
        case 98: // glIsRenderbufferOES
            result=resolve_fbo<GLboolean(*)(GLuint)>(
                "glIsRenderbuffer","glIsRenderbufferEXT")(a(0))?1u:0u;
            break;
        case 99: { // glRenderbufferStorageOES
            if(a(2)>8192u || a(3)>8192u)
                throw std::runtime_error("ARM renderbuffer size cap");
            resolve_fbo<void(*)(GLenum,GLenum,GLsizei,GLsizei)>(
                "glRenderbufferStorage","glRenderbufferStorageEXT")(
                  a(0),a(1),a(2),a(3));
            break;
        }
        case 100: // glFramebufferRenderbufferOES
            resolve_fbo<void(*)(GLenum,GLenum,GLenum,GLuint)>(
                "glFramebufferRenderbuffer","glFramebufferRenderbufferEXT")(
                  a(0),a(1),a(2),a(3));
            break;
        case 101: { // glGetRenderbufferParameterivOES
            auto* dst=guest.writeable(a(2),4u);
            if(!dst||(a(2)&3u))
                throw std::runtime_error("ARM renderbuffer query outptr invalid");
            GLint value=0;
            resolve_fbo<void(*)(GLenum,GLenum,GLint*)>(
                "glGetRenderbufferParameteriv",
                "glGetRenderbufferParameterivEXT")(a(0),a(1),&value);
            for(unsigned i=0;i<4u;++i)
                dst[i]=std::uint8_t(std::uint32_t(value)>>(i*8u));
            break;
        }
        case 102: // glBlendEquationOES
            resolve_fbo<void(*)(GLenum)>(
                "glBlendEquation","glBlendEquationEXT")(a(0));
            break;
        case 103: // glBlendFuncSeparateOES
            resolve_fbo<void(*)(GLenum,GLenum,GLenum,GLenum)>(
                "glBlendFuncSeparate","glBlendFuncSeparateEXT")(
                  a(0),a(1),a(2),a(3));
            break;
        case 104: // glBlendEquationSeparateOES
            resolve_fbo<void(*)(GLenum,GLenum)>(
                "glBlendEquationSeparate","glBlendEquationSeparateEXT")(a(0),a(1));
            break;
        default:
            if(which>=105u)return dispatch_legacy(which,r,guest,error);
            throw std::runtime_error("Unknown GLES1 dispatch index");
        }
        r[0]=result;
        error.clear();return true;
    }catch(const std::exception& e){
        error=std::string("GLES1 host call ")+
            (handles(id)?names[id-svc_base]:"unknown")+": "+e.what();
        return false;
    }
}
} // namespace xperia
