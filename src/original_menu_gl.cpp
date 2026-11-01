#define SDL_MAIN_HANDLED
#include "ds_port/original_menu_gl.hpp"
#include <SDL.h>
#include <SDL_opengl.h>
#include <algorithm>
#include <cmath>
#include <limits>
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
           all_zero(m.model_matrix)||!transforms.emplace(m.mesh_object_index,&m).second){
            reason="Duplicate or invalid original mesh transform";return false;
        }
    }
    std::unordered_map<std::uint32_t,const OriginalMenuGpuSurface*> materials;
    for(const auto& m:frame.material_bindings){
        if(!m.appearance_object_index||
           (m.use_texture && (m.image_index>=images.size() ||
             !images[m.image_index].decoded)) ||
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
    const OriginalMenuGpuFrame& frame,std::string& diagnostic) {
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
    bool quit_requested=false;
    while(!quit_requested){
        SDL_Event e{};
        while(SDL_PollEvent(&e))
            if(e.type==SDL_QUIT ||
               (e.type==SDL_KEYDOWN&&e.key.keysym.sym==SDLK_ESCAPE))
                quit_requested=true;
        int w=0,h=0;SDL_GL_GetDrawableSize(window.get(),&w,&h);
        if(w<=0||h<=0)continue;
        glViewport(0,0,w,h);
        glClearColor(0,0,0,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);glLoadMatrixf(frame.projection.data());
        for(const auto& batch:stream.batches){
            const auto& object=*transforms.at(batch.mesh_object_index);
            const auto& mat=*materials.at(batch.appearance_object_index);
            glMatrixMode(GL_MODELVIEW);
            glLoadMatrixf(frame.view.data());
            glMultMatrixf(object.model_matrix.data());
            if(mat.use_texture){
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D,textures[mat.image_index]);
            }else glDisable(GL_TEXTURE_2D);
            glDrawElements(GL_TRIANGLES,static_cast<GLsizei>(batch.index_count),
                GL_UNSIGNED_INT,geometry.indices.data()+batch.first_index);
        }
        SDL_GL_SwapWindow(window.get());
    }
    free_textures();
    diagnostic.clear();
    return OriginalMenuGpuStatus::UserAborted;
}
}
