/*
 * Original-free ARMHF SDL graphics probe. Compiled for ARM Linux, executed
 * through the same packaged QEMU + ARMHF sysroot as Dead Space Mobile.
 *
 * Catches libGL.so.1 and libGLX missing, and checks that the SDL2
 * OpenGL/GLES window works under llvmpipe/Xvfb. Does not use EA game code.
 */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct SDL_Window SDL_Window;
typedef void* SDL_GLContext;
typedef int (*SDL_Init_fn)(uint32_t);
typedef void (*SDL_Quit_fn)(void);
typedef SDL_Window *(*SDL_CreateWindow_fn)(
    const char*,int,int,int,int,uint32_t);
typedef void (*SDL_DestroyWindow_fn)(SDL_Window*);
typedef SDL_GLContext (*SDL_GL_CreateContext_fn)(SDL_Window*);
typedef void (*SDL_GL_DeleteContext_fn)(SDL_GLContext);
typedef int (*SDL_GL_SetAttribute_fn)(int,int);
typedef const char *(*SDL_GetError_fn)(void);
typedef void *(*SDL_GL_GetProcAddress_fn)(const char*);
typedef const unsigned char *(*glGetString_fn)(unsigned int);

#define CHECK_SYM(PTR,LIB,NAME) do { \
    *(void **)(&(PTR))=dlsym((LIB),(NAME)); \
    if (!(PTR)) {fprintf(stderr,"Missing %s: %s\n",(NAME),dlerror());return 3;} \
} while(0)
int main(void){
    void *gl=dlopen("libGL.so.1",RTLD_LAZY|RTLD_LOCAL);
    if(!gl){fprintf(stderr,"ARMHF libGL.so.1 missing: %s\n",dlerror());return 2;}
    void *egl=dlopen("libEGL.so.1",RTLD_LAZY|RTLD_LOCAL);
    if(!egl){fprintf(stderr,"ARMHF libEGL.so.1 missing: %s\n",dlerror());return 2;}
    void *sdl=dlopen("libSDL2-2.0.so.0",RTLD_LAZY|RTLD_LOCAL);
    if(!sdl){fprintf(stderr,"ARMHF SDL2 missing: %s\n",dlerror());return 2;}
    SDL_Init_fn init;SDL_Quit_fn quit;
    SDL_CreateWindow_fn create;SDL_DestroyWindow_fn destroy;
    SDL_GL_CreateContext_fn context;SDL_GL_DeleteContext_fn delete_context;
    SDL_GL_SetAttribute_fn attr;SDL_GetError_fn err;
    SDL_GL_GetProcAddress_fn proc;
    CHECK_SYM(init,sdl,"SDL_Init");CHECK_SYM(quit,sdl,"SDL_Quit");
    CHECK_SYM(create,sdl,"SDL_CreateWindow");
    CHECK_SYM(destroy,sdl,"SDL_DestroyWindow");
    CHECK_SYM(context,sdl,"SDL_GL_CreateContext");
    CHECK_SYM(delete_context,sdl,"SDL_GL_DeleteContext");
    CHECK_SYM(attr,sdl,"SDL_GL_SetAttribute");
    CHECK_SYM(err,sdl,"SDL_GetError");
    CHECK_SYM(proc,sdl,"SDL_GL_GetProcAddress");
    if(init(0x20u)!=0){fprintf(stderr,"SDL_Init: %s\n",err());return 4;}
    /* Match game's ES 1.1 then desktop compatibility fallback. */
    attr(21,4); /* SDL_GL_CONTEXT_PROFILE_MASK / ES */
    attr(17,1);attr(18,1);
    SDL_Window *win=create("Xperia ARMHF QEMU SDL probe",0,0,320,240,2u);
    if(!win){
        fprintf(stderr,"SDL_CreateWindow GLES1: %s\n",err());
        quit();return 5;
    }
    SDL_GLContext ctx=context(win);
    if(!ctx){
        fprintf(stderr,"GLES1 context unsupported: %s; testing GL2.1 compatibility\n",err());
        destroy(win);
        attr(21,2);attr(17,2);attr(18,1);
        win=create("Xperia ARMHF QEMU SDL probe",0,0,320,240,2u);
        if(!win){fprintf(stderr,"SDL_CreateWindow GL2: %s\n",err());quit();return 6;}
        ctx=context(win);
    }
    if(!ctx){fprintf(stderr,"No GLES1/GL2.1 context: %s\n",err());destroy(win);quit();return 7;}
    glGetString_fn get_string;
    *(void **)(&get_string)=proc("glGetString");
    const unsigned char *ver=get_string?get_string(0x1f02u):NULL;
    printf("ARMHF QEMU SDL window + GL context: PASS GL_VERSION=%s\n",ver? (const char*)ver:"unknown");
    delete_context(ctx);destroy(win);quit();
    dlclose(sdl);dlclose(egl);dlclose(gl);
    return 0;
}
