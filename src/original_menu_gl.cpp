#define SDL_MAIN_HANDLED
#include "ds_port/original_menu_gl.hpp"
#include "ds_port/original_menu_animation.hpp"
#include "ds_port/original_ui_interaction.hpp"
#include "ds_port/native_menu_input.hpp"
#include "ds_port/original_input_jni_bridge.hpp"
#include <SDL.h>
#include <SDL_opengl.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <unordered_set>
namespace ds_port {
namespace {
bool finite_matrix(const std::array<float,16>& m) {
    for(float value:m)if(!std::isfinite(value))return false;
    return true;
}
struct SDLQuit { ~SDLQuit(){SDL_QuitSubSystem(SDL_INIT_VIDEO);} };
struct WindowDeleter { void operator()(SDL_Window* w)const {if(w)SDL_DestroyWindow(w);} };
struct GLContextDeleter { void operator()(void* c)const {if(c)SDL_GL_DeleteContext(c);} };
}
bool validate_original_menu_gpu_frame(
    const ImMenuGeometry& geometry,const OriginalMenuDrawStream& stream,
    const std::vector<M3gRgbaImage>& images,
    const OriginalMenuGpuFrame& frame,std::string& reason) {
    if(!geometry.decoded||!stream.valid||stream.batches.empty()||
       geometry.indices.empty()||geometry.vertices.empty()){
        reason="Original menu draw stream not initialized";return false;
    }
    if(!finite_matrix(frame.projection)||!finite_matrix(frame.view)||
       frame.mesh_transforms.size()!=geometry.mesh_count){
        reason="Original camera or mesh transforms missing";return false;
    }
    const auto all_zero=[](const std::array<float,16>& a) {
        return std::all_of(a.begin(),a.end(),[](float f){return f==0.f;});
    };
    if(all_zero(frame.projection)||all_zero(frame.view)){
        reason="Original camera projection/view not recovered";return false;
    }
    std::unordered_map<std::uint32_t,const OriginalMenuGpuMesh*> transforms;
    for(const auto& m:frame.mesh_transforms){
        if(!m.mesh_object_index||!finite_matrix(m.model_matrix)||
           all_zero(m.model_matrix)||!std::isfinite(m.alpha)||
           m.alpha<0.f||m.alpha>1.f||
           !transforms.emplace(m.mesh_object_index,&m).second){
            reason="Duplicate or invalid original mesh transform";return false;
        }
    }
    std::unordered_map<std::uint32_t,const OriginalMenuGpuSurface*> materials;
    for(const auto& m:frame.material_bindings){
        if(!m.appearance_object_index||
           (m.use_texture && (m.image_index>=images.size() ||
             !images[m.image_index].decoded)) ||
           m.blending<64||m.blending>68||
           m.culling<160||m.culling>162||
           (m.winding!=168&&m.winding!=169)||
           !std::isfinite(m.depth_offset_factor)||
           !std::isfinite(m.depth_offset_units)||
           !materials.emplace(m.appearance_object_index,&m).second){
            reason="Duplicate or invalid original material binding";return false;
        }
    }
    for(const auto& bounds:geometry.mesh_bounds)
        if(!transforms.count(bounds.mesh_object_index)){
            reason="Original mesh world transform not supplied";return false;
        }
    for(const auto& batch:stream.batches){
        if(!transforms.count(batch.mesh_object_index)||
           !materials.count(batch.appearance_object_index)){
            reason="Original draw batch transform/material unresolved";return false;
        }
        if(batch.first_index>geometry.indices.size() ||
           batch.index_count>geometry.indices.size()-batch.first_index){
            reason="Original draw batch index span invalid";return false;
        }
    }
    for(const auto& image:images){
        if(image.decoded && (image.width==0||image.height==0||
           image.width>4096||image.height>4096||
           image.pixels_rgba.size()!=
              std::size_t(image.width)*image.height*4u)){
            reason="Invalid original image dimensions";return false;
        }
    }
    reason.clear();return true;
}
OriginalMenuGpuStatus run_original_menu_gl(
    const ImMenuGeometry& geometry,const OriginalMenuDrawStream& stream,
    const std::vector<M3gRgbaImage>& images,
    const OriginalMenuGpuFrame& frame,std::string& diagnostic,
    const OriginalMenuAnimationRuntime* motion_runtime,
    const OriginalUiRuntime* ui_runtime) {
    if(!validate_original_menu_gpu_frame(geometry,stream,images,frame,diagnostic))
        return OriginalMenuGpuStatus::IncompleteOriginalScene;
    if(SDL_InitSubSystem(SDL_INIT_VIDEO)!=0){
        diagnostic=SDL_GetError();return OriginalMenuGpuStatus::GraphicsUnavailable;
    }
    SDLQuit quit{};
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    std::unique_ptr<SDL_Window,WindowDeleter> window(SDL_CreateWindow(
        "Dead Space Mobile - native original 3D menu",SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,1280,720,
        SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE));
    if(!window){diagnostic=SDL_GetError();return OriginalMenuGpuStatus::GraphicsUnavailable;}
    std::unique_ptr<void,GLContextDeleter> gl(SDL_GL_CreateContext(window.get()));
    if(!gl){diagnostic=SDL_GetError();return OriginalMenuGpuStatus::GraphicsUnavailable;}
    SDL_GL_SetSwapInterval(1);
    // OpenGL compatibility profile: fixed function is supported on GL 2.1.
    // On modern platforms, replace with shaders without changing the scene contract.
    std::vector<GLuint> textures(images.size(),0);
    glGenTextures(static_cast<GLsizei>(textures.size()),textures.data());
    auto free_textures=[&](){glDeleteTextures(static_cast<GLsizei>(textures.size()),textures.data());};
    for(std::size_t i=0;i<images.size();++i){
        const auto& img=images[i];if(!img.decoded)continue;
        glBindTexture(GL_TEXTURE_2D,textures[i]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,
            static_cast<GLsizei>(img.width),static_cast<GLsizei>(img.height),0,
            GL_RGBA,GL_UNSIGNED_BYTE,img.pixels_rgba.data());
    }
    bool has_ui_layer=false;
    std::vector<GLuint> ui_textures;
    OriginalUiInteractionState ui_input{};
    if(ui_runtime && ui_runtime->database && ui_runtime->atlases &&
       ui_runtime->draw_commands) {
        std::string reason;
        if(!validate_original_ui_draw_list(*ui_runtime->database,
                *ui_runtime->atlases,*ui_runtime->draw_commands,reason)){
            diagnostic="Original frontend draw commands rejected: "+reason;
            free_textures();return OriginalMenuGpuStatus::InvalidScene;
        }
        ui_textures.resize(ui_runtime->atlases->size(),0);
        glGenTextures(static_cast<GLsizei>(ui_textures.size()),ui_textures.data());
        for(std::size_t i=0;i<ui_textures.size();++i){
            const auto& a=ui_runtime->atlases->at(i);
            if(!a.decoded)continue;
            glBindTexture(GL_TEXTURE_2D,ui_textures[i]);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,
                static_cast<GLsizei>(a.width),static_cast<GLsizei>(a.height),
                0,GL_RGBA,GL_UNSIGNED_BYTE,a.pixels_rgba.data());
        }
        has_ui_layer=true;
    }
    auto release_all_textures=[&](){
        if(!ui_textures.empty())
            glDeleteTextures(static_cast<GLsizei>(ui_textures.size()),
                             ui_textures.data());
        free_textures();
    };
    const auto ui_pointer=[&](OriginalUiPointerAction action,int x,int y){
        if(!has_ui_layer)return;
        int window_width=0,window_height=0;
        SDL_GetWindowSize(window.get(),&window_width,&window_height);
        const auto previous=ui_input.hovered;
        const auto result=dispatch_original_ui_window_pointer(ui_input,
            *ui_runtime->draw_commands,action,window_width,window_height,x,y);
        if(result.hovered!=previous && ui_runtime->on_hover)
            ui_runtime->on_hover(ui_runtime->context,result.hovered);
        if(result.activated && ui_runtime->on_activate)
            ui_runtime->on_activate(ui_runtime->context,result.activated);
    };
    struct PackedVertex{float xyz[3];float uv[2];};
    std::vector<PackedVertex> packed;
    packed.reserve(geometry.vertices.size());
    for(const auto& v:geometry.vertices)packed.push_back({{v.x,v.y,v.z},{v.u,v.v}});
    std::unordered_map<std::uint32_t,const OriginalMenuGpuMesh*> transforms;
    for(const auto& m:frame.mesh_transforms)transforms.emplace(m.mesh_object_index,&m);
    std::unordered_map<std::uint32_t,const OriginalMenuGpuSurface*> materials;
    for(const auto& m:frame.material_bindings)materials.emplace(m.appearance_object_index,&m);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(3,GL_FLOAT,sizeof(PackedVertex),packed[0].xyz);
    glTexCoordPointer(2,GL_FLOAT,sizeof(PackedVertex),packed[0].uv);
    NativeMenuInputState input{};
    NativeMenuInputCallbacks callbacks{};
    callbacks.pointer=[](void*,int event,int module,int id,float x,float y) {
        Java_com_ea_blast_TouchSurfaceAndroid_NativeOnPointerEvent(
            nullptr,nullptr,event,module,id,x,y);
    };
    callbacks.key_down=[](void*,int module,int key,int alt) {
        Java_com_ea_blast_KeyboardAndroid_NativeOnKeyDown(
            nullptr,nullptr,module,key,alt);
    };
    callbacks.key_up=[](void*,int module,int key,int alt) {
        Java_com_ea_blast_KeyboardAndroid_NativeOnKeyUp(
            nullptr,nullptr,module,key,alt);
    };
    bool quit_requested=false;
    const std::uint32_t animation_start=SDL_GetTicks();
    bool reported_unbound_input=false;
    while(!quit_requested){
        SDL_Event e{};
        while(SDL_PollEvent(&e)) {
            if(e.type==SDL_QUIT)quit_requested=true;
            else if(e.type==SDL_WINDOWEVENT &&
                    e.window.event==SDL_WINDOWEVENT_FOCUS_LOST) {
                native_menu_focus_lost(input,callbacks);
                ui_pointer(OriginalUiPointerAction::Cancel,0,0);
            }else if(e.type==SDL_KEYDOWN && !e.key.repeat) {
                const auto key=e.key.keysym.sym;
                if(key==SDLK_ESCAPE) {
                    native_menu_key(input,callbacks,4,true);
                    // When the original JNI queue is unavailable, invoke
                    // only an existing recovered BTN_BACK layout action.
                    if(ui_runtime && ui_runtime->on_back &&
                       original_runtime::original_last_input_status()!=
                           original_runtime::OriginalInputStatus::Completed) {
                        ui_runtime->on_back(ui_runtime->context);
                        ui_input.hovered=0;ui_input.pressed=0;
                    }
                }else if(key==SDLK_RETURN || key==SDLK_KP_ENTER) {
                    native_menu_key(input,callbacks,66,true);
                    // Desktop keyboard support uses the same named UI
                    // actions as mouse clicks, never fabricated screens.
                    if(has_ui_layer && ui_runtime->on_activate &&
                       ui_input.hovered &&
                       original_runtime::original_last_input_status()!=
                           original_runtime::OriginalInputStatus::Completed) {
                        const auto selected=ui_input.hovered;
                        ui_input.hovered=0;ui_input.pressed=0;
                        ui_runtime->on_activate(ui_runtime->context,selected);
                    }
                }else if(has_ui_layer && ui_runtime->on_hover &&
                         (key==SDLK_TAB || key==SDLK_UP ||
                          key==SDLK_DOWN || key==SDLK_LEFT ||
                          key==SDLK_RIGHT)) {
                    const bool backwards=(key==SDLK_UP||key==SDLK_LEFT||
                        (key==SDLK_TAB && (e.key.keysym.mod&KMOD_SHIFT)));
                    const auto focused=cycle_original_ui_focus(
                        *ui_runtime->draw_commands,ui_input.hovered,backwards);
                    ui_input.hovered=focused;ui_input.pressed=0;
                    ui_runtime->on_hover(ui_runtime->context,focused);
                }
            }else if(e.type==SDL_KEYUP &&
                     (e.key.keysym.sym==SDLK_RETURN ||
                      e.key.keysym.sym==SDLK_KP_ENTER ||
                      e.key.keysym.sym==SDLK_ESCAPE))
                native_menu_key(input,callbacks,
                                e.key.keysym.sym==SDLK_ESCAPE?4:66,false);
            else if(e.type==SDL_MOUSEBUTTONDOWN ||
                    e.type==SDL_MOUSEBUTTONUP ||
                    e.type==SDL_MOUSEMOTION) {
                int width=0,height=0;
                SDL_GetWindowSize(window.get(),&width,&height);
                // SDL mouse coordinates are window pixels. The original
                // 480x320 viewport is applied once inside ui_pointer;
                // pre-scaling here broke hit tests on widescreen windows.
                if(e.type==SDL_MOUSEMOTION)
                    ui_pointer(OriginalUiPointerAction::Move,
                               e.motion.x,e.motion.y);
                else if(e.type==SDL_MOUSEBUTTONDOWN &&
                        e.button.button==SDL_BUTTON_LEFT)
                    ui_pointer(OriginalUiPointerAction::Down,
                               e.button.x,e.button.y);
                else if(e.type==SDL_MOUSEBUTTONUP &&
                        e.button.button==SDL_BUTTON_LEFT)
                    ui_pointer(OriginalUiPointerAction::Up,
                               e.button.x,e.button.y);
                if(e.type==SDL_MOUSEMOTION && input.mouse_held)
                    native_menu_pointer(input,callbacks,2,e.motion.x,e.motion.y,width,height);
                else if(e.type==SDL_MOUSEBUTTONDOWN &&
                        e.button.button==SDL_BUTTON_LEFT)
                    native_menu_pointer(input,callbacks,0,e.button.x,e.button.y,width,height);
                else if(e.type==SDL_MOUSEBUTTONUP &&
                        e.button.button==SDL_BUTTON_LEFT)
                    native_menu_pointer(input,callbacks,1,e.button.x,e.button.y,width,height);
            }
        }
        // JNI wrappers fail closed until original mapped queue owners and
        // event allocator are connected. Do not silently claim UI input works.
        if(!reported_unbound_input &&
           original_runtime::original_last_input_status()==
               original_runtime::OriginalInputStatus::MissingOriginalService &&
           (input.mouse_held || input.enter_held || input.escape_held)) {
            std::cerr<<"Original menu input unavailable: original event allocator/queue "
                        "not bound to the recovered runtime.\\n";
            reported_unbound_input=true;
        }
        int w=0,h=0;SDL_GL_GetDrawableSize(window.get(),&w,&h);
        if(w<=0||h<=0)continue;
        glViewport(0,0,w,h);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        glDepthMask(GL_TRUE);
        glClearColor(0,0,0,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        const char* requested_smoke=std::getenv("DS_NATIVE_MENU_SMOKE_FRAMES");
        const bool inspect_smoke=requested_smoke&&std::strcmp(requested_smoke,"1")==0;
        std::uint32_t last_center=0xffffffffu;
        if(inspect_smoke){
            std::uint8_t clear_pixel[4]{};
            glReadPixels(w/2,h/2,1,1,GL_RGBA,GL_UNSIGNED_BYTE,clear_pixel);
            std::cerr<<"Original GPU cleared center RGB ("<<int(clear_pixel[0])<<","
                     <<int(clear_pixel[1])<<","<<int(clear_pixel[2])<<")\\n";
            last_center=std::uint32_t(clear_pixel[0])<<16u|
                        std::uint32_t(clear_pixel[1])<<8u|clear_pixel[2];
        }
        // Use the original keyframe sequences and recompute all world
        // matrices from the real M3G hierarchy, rather than moving a
        // synthetic menu object. Controller preview activation is
        // explicitly separated from the original runtime's activation.
        OriginalMenuGpuFrame animated_frame{};
        const OriginalMenuGpuFrame* draw_frame=&frame;
        if(motion_runtime && motion_runtime->motion &&
           motion_runtime->nodes && motion_runtime->camera &&
           motion_runtime->materials) {
            std::vector<OriginalSceneNode> animated_nodes;
            const std::int64_t elapsed=std::int64_t(SDL_GetTicks()-animation_start);
            if(!sample_original_menu_motion(*motion_runtime->motion,
                    *motion_runtime->nodes,elapsed,animated_nodes,
                    motion_runtime->preview_activate_controllers)){
                diagnostic="Original menu keyframe sampling failed";
                release_all_textures();return OriginalMenuGpuStatus::InvalidScene;
            }
            auto resolved=resolve_original_menu_scene(geometry,stream,images,
                animated_nodes,*motion_runtime->camera,*motion_runtime->materials);
            if(!resolved.valid){
                diagnostic="Animated original hierarchy rejected: "+resolved.error;
                release_all_textures();return OriginalMenuGpuStatus::InvalidScene;
            }
            animated_frame=std::move(resolved.frame);
            draw_frame=&animated_frame;
            transforms.clear();
            for(const auto& m:draw_frame->mesh_transforms)
                transforms.emplace(m.mesh_object_index,&m);
        }
        glMatrixMode(GL_PROJECTION);glLoadMatrixf(draw_frame->projection.data());
        for(const auto& batch:stream.batches){
            const auto& object=*transforms.at(batch.mesh_object_index);
            const auto& mat=*materials.at(batch.appearance_object_index);
            if(!object.visible||object.alpha<=0.f)continue;
            glColor4f(1.f,1.f,1.f,object.alpha);
            if(mat.depth_test)glEnable(GL_DEPTH_TEST);
            else glDisable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
            glDepthMask(mat.depth_write?GL_TRUE:GL_FALSE);
            glColorMask(mat.color_write?GL_TRUE:GL_FALSE,
                        mat.color_write?GL_TRUE:GL_FALSE,
                        mat.color_write?GL_TRUE:GL_FALSE,
                        mat.alpha_write?GL_TRUE:GL_FALSE);
            if(mat.alpha_threshold){
                glEnable(GL_ALPHA_TEST);
                glAlphaFunc(GL_GEQUAL,float(mat.alpha_threshold)/255.f);
            }else glDisable(GL_ALPHA_TEST);
            if(mat.blending==68)glDisable(GL_BLEND);
            else{
                glEnable(GL_BLEND);
                switch(mat.blending){
                  case 64:glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);break;
                  case 65:glBlendFunc(GL_SRC_ALPHA,GL_ONE);break;
                  case 66:glBlendFunc(GL_DST_COLOR,GL_ZERO);break;
                  case 67:glBlendFunc(GL_DST_COLOR,GL_SRC_COLOR);break;
                }
            }
            if(mat.culling==162)glDisable(GL_CULL_FACE);
            else{
                glEnable(GL_CULL_FACE);
                glCullFace(mat.culling==161?GL_FRONT:GL_BACK);
            }
            glFrontFace(mat.winding==169?GL_CW:GL_CCW);
            if(mat.depth_offset_factor!=0.f||mat.depth_offset_units!=0.f){
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(mat.depth_offset_factor,mat.depth_offset_units);
            }else glDisable(GL_POLYGON_OFFSET_FILL);
            glMatrixMode(GL_MODELVIEW);
            glLoadMatrixf(draw_frame->view.data());
            glMultMatrixf(object.model_matrix.data());
            if(mat.use_texture){
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D,textures[mat.image_index]);
            }else glDisable(GL_TEXTURE_2D);
            glDrawElements(GL_TRIANGLES,static_cast<GLsizei>(batch.index_count),
                GL_UNSIGNED_INT,geometry.indices.data()+batch.first_index);
            if(inspect_smoke){
                std::uint8_t center_pixel[4]{};
                glReadPixels(w/2,h/2,1,1,GL_RGBA,GL_UNSIGNED_BYTE,center_pixel);
                const std::uint32_t rgb=std::uint32_t(center_pixel[0])<<16u|
                    std::uint32_t(center_pixel[1])<<8u|center_pixel[2];
                if(rgb!=last_center){
                    std::cerr<<"Original GPU center changed after mesh "
                             <<batch.mesh_object_index<<" appearance "
                             <<batch.appearance_object_index
                             <<" textured="<<mat.use_texture
                             <<" RGB=("<<int(center_pixel[0])<<","
                             <<int(center_pixel[1])<<","<<int(center_pixel[2])
                             <<")\\n";
                    last_center=rgb;
                }
            }
        }
        // A real two-pass GPU smoke detects a UI that decoded and
        // submitted draw calls but never changed the framebuffer.
        std::vector<std::uint8_t> pre_ui_pixels;
        if(inspect_smoke && has_ui_layer &&
           !ui_runtime->draw_commands->empty() && w<=4096 && h<=4096){
            pre_ui_pixels.resize(std::size_t(w)*h*4u);
            glPixelStorei(GL_PACK_ALIGNMENT,1);
            glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,
                         pre_ui_pixels.data());
        }
        if(has_ui_layer && !ui_runtime->draw_commands->empty()) {
            const auto viewport=original_ui_viewport(w,h);
            if(viewport.width<=0||viewport.height<=0){
                diagnostic="Invalid native original 480x320 UI viewport";
                release_all_textures();return OriginalMenuGpuStatus::InvalidScene;
            }
            // The original UI geometry is 480x320. Preserve the original
            // aspect ratio instead of stretching it across 16:9 monitors.
            glViewport(viewport.x,h-viewport.y-viewport.height,
                       viewport.width,viewport.height);
            glPushAttrib(GL_ENABLE_BIT|GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|
                         GL_POLYGON_BIT|GL_TEXTURE_BIT);
            glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);
            glDisable(GL_CULL_FACE);glDisable(GL_ALPHA_TEST);
            glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_TEXTURE_2D);glColor4f(1,1,1,1);
            glDisableClientState(GL_VERTEX_ARRAY);
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
            glOrtho(0,480,320,0,-1,1);
            glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
            for(const auto& cmd:*ui_runtime->draw_commands){
                if(cmd.hit_only)continue;
                auto sprite=std::find_if(ui_runtime->database->sprites.begin(),
                    ui_runtime->database->sprites.end(),
                    [&](const OriginalUiSprite& x){
                        return x.object_id==cmd.sprite_object_id;
                    });
                if(sprite==ui_runtime->database->sprites.end())continue;
                const auto& atlas=ui_runtime->atlases->at(sprite->atlas);
                const auto& src=sprite->source_rect;
                const auto& packed=sprite->atlas_rect;
                const float x0=float(cmd.x)+float(cmd.width)*src.x/sprite->source_width;
                const float y0=float(cmd.y)+float(cmd.height)*src.y/sprite->source_height;
                const float x1=x0+float(cmd.width)*src.width/sprite->source_width;
                const float y1=y0+float(cmd.height)*src.height/sprite->source_height;
                const float u0=float(packed.x)/atlas.width;
                const float v0=float(packed.y)/atlas.height;
                const float u1=float(packed.x+packed.width)/atlas.width;
                const float v1=float(packed.y+packed.height)/atlas.height;
                glBindTexture(GL_TEXTURE_2D,ui_textures.at(sprite->atlas));
                glBegin(GL_QUADS);
                glTexCoord2f(u0,v0);glVertex2f(x0,y0);
                glTexCoord2f(u1,v0);glVertex2f(x1,y0);
                glTexCoord2f(u1,v1);glVertex2f(x1,y1);
                glTexCoord2f(u0,v1);glVertex2f(x0,y1);
                glEnd();
            }
            glMatrixMode(GL_MODELVIEW);glPopMatrix();
            glMatrixMode(GL_PROJECTION);glPopMatrix();
            glEnableClientState(GL_VERTEX_ARRAY);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glPopAttrib();
            glViewport(0,0,w,h);
        }
        // Optional one-frame production renderer smoke for GitHub's
        // software OpenGL runner. No framebuffer image is persisted.
        const char* smoke=std::getenv("DS_NATIVE_MENU_SMOKE_FRAMES");
        if(smoke && std::strcmp(smoke,"1")==0) {
            if(w>4096||h>4096) {
                diagnostic="OpenGL smoke window dimensions exceeded";release_all_textures();
                return OriginalMenuGpuStatus::InvalidScene;
            }
            std::vector<std::uint8_t> rgba(std::size_t(w)*h*4u);
            glPixelStorei(GL_PACK_ALIGNMENT,1);
            glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
            const auto error=glGetError();
            std::size_t ui_changed_pixels=0;
            if(!pre_ui_pixels.empty()){
                for(std::size_t i=0;i<rgba.size();i+=4u)
                    if(std::memcmp(pre_ui_pixels.data()+i,rgba.data()+i,4)!=0)
                        ++ui_changed_pixels;
                std::cerr<<"Original 2D frontend GPU overlay: "
                         <<ui_runtime->draw_commands->size()
                         <<" original UI draw/hit commands, changed pixels "
                         <<ui_changed_pixels<<"\\n";
            }
            std::size_t nonblack=0;
            std::array<bool,4096> color_buckets{};
            std::size_t diverse_colors=0;
            for(std::size_t index=0;index<rgba.size();index+=4u) {
                if(rgba[index]>12||rgba[index+1]>12||rgba[index+2]>12)
                    ++nonblack;
                if(index%(4u*16u)!=0)continue;
                const unsigned bucket=((rgba[index]>>4u)<<8u) |
                    ((rgba[index+1]>>4u)<<4u) | (rgba[index+2]>>4u);
                if(!color_buckets[bucket]) {
                    color_buckets[bucket]=true;
                    ++diverse_colors;
                }
            }
            const auto center=(std::size_t(h/2)*w+std::size_t(w/2))*4u;
            std::cerr<<"Original 3D menu GPU frame: "<<w<<"x"<<h
                     <<", nonblack pixels "<<nonblack
                     <<", distinct color buckets "<<diverse_colors
                     <<", first RGB ("<<int(rgba[0])<<","
                     <<int(rgba[1])<<","<<int(rgba[2])<<")"
                     <<", center RGB ("<<int(rgba[center])<<","
                     <<int(rgba[center+1])<<","<<int(rgba[center+2])<<")"
                     <<", GL error "<<unsigned(error)<<"\\n";
            release_all_textures();
            if(error!=GL_NO_ERROR || nonblack==0 || diverse_colors<8 ||
               (!pre_ui_pixels.empty() && ui_changed_pixels==0)) {
                diagnostic="Original GPU scene/frontend generated no visible pixels or GL error";
                return OriginalMenuGpuStatus::InvalidScene;
            }
            diagnostic.clear();
            return OriginalMenuGpuStatus::Rendered;
        }
        SDL_GL_SwapWindow(window.get());
    }
    native_menu_focus_lost(input,callbacks);
    release_all_textures();
    diagnostic.clear();
    return OriginalMenuGpuStatus::UserAborted;
}
}
