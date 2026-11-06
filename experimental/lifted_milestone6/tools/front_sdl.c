// front_sdl.c - window, OpenGL context and event pump (SDL2). Included by host.c when built with -DDS_SDL.
#include <SDL.h>
static void host_pointer(int kind, float x, float y);   // kind: 0 down, 1 move, 2 up (defined in host.c)
static void host_pointer_id(int kind, int finger, float x, float y);
static void host_key(int down, int keycode, int alt);     // Android key codes (defined in host.c)
static SDL_Window* g_win; static SDL_GLContext g_ctx; static int g_w, g_h; static float g_sx = 1, g_sy = 1;

static int sdl_open(int w, int h) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) { fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError()); return 0; }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24); SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    g_win = SDL_CreateWindow("Dead Space", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
                             SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!g_win) { fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError()); return 0; }
    g_ctx = SDL_GL_CreateContext(g_win);
    if (!g_ctx) { fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError()); return 0; }
    SDL_GL_SetSwapInterval(getenv("DS_NOVSYNC") ? 0 : 1);
    gl_load_all(SDL_GL_GetProcAddress);
    SDL_GL_GetDrawableSize(g_win, &g_w, &g_h);
    { int ww, wh; SDL_GetWindowSize(g_win, &ww, &wh); g_sx = (float)g_w / ww; g_sy = (float)g_h / wh; }
    printf("window %dx%d, GL_RENDERER=%s\n", g_w, g_h, dgl_glGetString ? (const char*)dgl_glGetString(0x1F01) : "?");
    return 1;
}
/* PC movement uses the game's verified left-screen drag path. Separate
 * finger IDs allow movement, mouse look and ordinary menu touches together. */
static int pc_w,pc_a,pc_s,pc_d,pc_shift,pc_capture,pc_move_active,pc_look_active;
static int pc_fire,pc_aim;
static float pc_move_x,pc_move_y,pc_look_x,pc_look_y;
static void pc_release(void) {
    if(pc_move_active) host_pointer_id(2,1,pc_move_x,pc_move_y);
    if(pc_look_active) host_pointer_id(2,2,pc_look_x,pc_look_y);
    if(pc_fire) host_key(0,103,0);
    if(pc_aim) host_key(0,102,0);
    pc_w=pc_a=pc_s=pc_d=pc_shift=pc_move_active=pc_look_active=pc_fire=pc_aim=0;
}
static void pc_set_capture(int enabled) {
    pc_release();
    if(SDL_SetRelativeMouseMode(enabled ? SDL_TRUE : SDL_FALSE)!=0) {
        fprintf(stderr,"[PC_INPUT] mouse capture failed: %s\n",SDL_GetError()); return;
    }
    pc_capture=enabled;
    fprintf(stderr,"[PC_INPUT] mouse look %s (F1 toggles)\n",enabled ? "on" : "off");
}
static void pc_update(float dx,float dy) {
    float x=(float)(pc_d-pc_a),y=(float)(pc_s-pc_w);
    float len=sqrtf(x*x+y*y);
    if(len>0) {
        x/=len; y/=len;
        float bx=g_w*.25f,by=g_h*.70f;
        if(!pc_move_active) host_pointer_id(0,1,bx,by);
        pc_move_x=bx+x*g_w*.16f;
        pc_move_y=by+y*g_h*(pc_shift ? .35f : .20f);
        host_pointer_id(1,1,pc_move_x,pc_move_y); pc_move_active=1;
    } else if(pc_move_active) {
        host_pointer_id(2,1,pc_move_x,pc_move_y); pc_move_active=0;
    }
    if(pc_look_active) { host_pointer_id(2,2,pc_look_x,pc_look_y); pc_look_active=0; }
    if(pc_capture && (dx!=0 || dy!=0)) {
        float bx=g_w*.75f,by=g_h*.50f;
        dx=fmaxf(-g_w*.20f,fminf(g_w*.20f,dx*g_sx));
        dy=fmaxf(-g_h*.40f,fminf(g_h*.40f,dy*g_sy));
        host_pointer_id(0,2,bx,by);
        pc_look_x=bx+dx; pc_look_y=by+dy;
        host_pointer_id(1,2,pc_look_x,pc_look_y); pc_look_active=1;
    }
}
// Default PC bindings, modelled on the Xperia Play layout. Each entry: SDL scancode -> Android key code (+alt flag).
static int map_scancode(int sc, int* alt) {
    *alt = 0;
    switch (sc) {
        case SDL_SCANCODE_UP: return 19;    case SDL_SCANCODE_DOWN: return 20;
        case SDL_SCANCODE_LEFT: return 21;  case SDL_SCANCODE_RIGHT: return 22;
        case SDL_SCANCODE_ESCAPE: case SDL_SCANCODE_BACKSPACE: *alt = 1; return 4; // Android Back; window close/Alt+F4 exits
        case SDL_SCANCODE_RETURN: case SDL_SCANCODE_E: return 23;            // Cross: interact / confirm
        case SDL_SCANCODE_Q: *alt = 1; return 4;                              // Circle: kinesis / back
        case SDL_SCANCODE_R: return 99;                                       // Square: reload
        case SDL_SCANCODE_F: return 100;                                      // Triangle: quick turn / stasis
        case SDL_SCANCODE_Z: return 102;    case SDL_SCANCODE_X: return 103;  // L1 / R1
        case SDL_SCANCODE_TAB: return 109;  case SDL_SCANCODE_P: return 108;  // Select / Start
    }
    return 0;
}
static int map_button(int b, int* alt) {
    *alt = 0;
    switch (b) {
        case SDL_CONTROLLER_BUTTON_A: return 23;  case SDL_CONTROLLER_BUTTON_B: *alt = 1; return 4;
        case SDL_CONTROLLER_BUTTON_X: return 99;  case SDL_CONTROLLER_BUTTON_Y: return 100;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return 102; case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return 103;
        case SDL_CONTROLLER_BUTTON_START: return 108; case SDL_CONTROLLER_BUTTON_BACK: return 109;
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return 19; case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return 20;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return 21; case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return 22;
    }
    return 0;
}
/* Optional real-game automation: enqueue ordinary SDL input events so tests
 * exercise the same mappings and pointer path as a physical keyboard/mouse.
 * DS_KEY_HOLDS=start:end:SDL-key-name,...
 * DS_MOUSE_DRAGS=start:end:x0:y0:x1:y1,... (window coordinates). */
static void sdl_script_input(int frame) {
    const char* p=getenv("DS_KEY_HOLDS");
    while(p && *p) {
        int start,end,n=0; char name[32]={0};
        if(sscanf(p,"%d:%d:%31[^,]%n",&start,&end,name,&n)!=3 || !n) break;
        SDL_Scancode sc=SDL_GetScancodeFromName(name);
        if(start>0 && end>start && sc!=SDL_SCANCODE_UNKNOWN && (frame==start || frame==end)) {
            SDL_Event e; memset(&e,0,sizeof e);
            e.type=frame==start ? SDL_KEYDOWN : SDL_KEYUP;
            e.key.state=frame==start ? SDL_PRESSED : SDL_RELEASED;
            e.key.keysym.scancode=sc;
            e.key.keysym.sym=SDL_GetKeyFromScancode(sc);
            if(SDL_PushEvent(&e)<0) fprintf(stderr,"[INPUT_SCRIPT_ERROR] %s\n",SDL_GetError());
            fprintf(stderr,"[INPUT_SCRIPT] frame=%d key=%s down=%d\n",frame,name,frame==start);
        }
        p+=n; if(*p==',') ++p; else break;
    }
    p=getenv("DS_MOUSE_LOOK");
    while(p && *p) {
        int start,end,dx,dy,n=0;
        if(sscanf(p,"%d:%d:%d:%d%n",&start,&end,&dx,&dy,&n)!=4 || !n) break;
        if(frame>=start && frame<end) {
            SDL_Event e; memset(&e,0,sizeof e); e.type=SDL_MOUSEMOTION;
            e.motion.xrel=dx; e.motion.yrel=dy; SDL_PushEvent(&e);
            if(frame==start) fprintf(stderr,"[INPUT_SCRIPT] mouse look frame=%d dx=%d dy=%d\n",frame,dx,dy);
        }
        p+=n; if(*p==',') ++p; else break;
    }
    p=getenv("DS_MOUSE_DRAGS");
    while(p && *p) {
        int start,end,x0,y0,x1,y1,n=0;
        if(sscanf(p,"%d:%d:%d:%d:%d:%d%n",&start,&end,&x0,&y0,&x1,&y1,&n)!=6 || !n) break;
        if(start>0 && end>start && frame>=start && frame<=end) {
            int x=x0+(int)(((s64)x1-x0)*(frame-start)/(end-start));
            int y=y0+(int)(((s64)y1-y0)*(frame-start)/(end-start));
            SDL_Event e; memset(&e,0,sizeof e);
            if(frame==start || frame==end) {
                e.type=frame==start ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
                e.button.button=SDL_BUTTON_LEFT;
                e.button.state=frame==start ? SDL_PRESSED : SDL_RELEASED;
                e.button.x=x; e.button.y=y;
                fprintf(stderr,"[INPUT_SCRIPT] frame=%d mouse_down=%d x=%d y=%d\n",frame,frame==start,x,y);
            } else {
                e.type=SDL_MOUSEMOTION; e.motion.state=SDL_BUTTON_LMASK;
                e.motion.x=x; e.motion.y=y;
            }
            if(SDL_PushEvent(&e)<0) fprintf(stderr,"[INPUT_SCRIPT_ERROR] %s\n",SDL_GetError());
        }
        p+=n; if(*p==',') ++p; else break;
    }
}
// returns 0 when the user closed the window; *resized is set when the drawable size changed
static int sdl_pump(int* resized) {
    static int frame; sdl_script_input(++frame);
    SDL_Event e; float look_dx=0,look_dy=0; *resized = 0;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { pc_release(); return 0; }
        // Escape is Android Back/Pause (handled by map_scancode), not a forced shutdown.
        if ((e.type == SDL_KEYDOWN && !e.key.repeat) || e.type == SDL_KEYUP) {
            int down=e.type==SDL_KEYDOWN; SDL_Scancode sc=e.key.keysym.scancode;
            if(sc==SDL_SCANCODE_F1) { if(down) pc_set_capture(!pc_capture); continue; }
            if(sc==SDL_SCANCODE_W) { pc_w=down; continue; }
            if(sc==SDL_SCANCODE_A) { pc_a=down; continue; }
            if(sc==SDL_SCANCODE_S) { pc_s=down; continue; }
            if(sc==SDL_SCANCODE_D) { pc_d=down; continue; }
            if(sc==SDL_SCANCODE_LSHIFT) { pc_shift=down; continue; }
            if(sc==SDL_SCANCODE_ESCAPE && down && pc_capture) pc_set_capture(0);
            int alt,k=map_scancode(sc,&alt); if(k) host_key(down,k,alt);
        }
        if(e.type==SDL_WINDOWEVENT && e.window.event==SDL_WINDOWEVENT_FOCUS_LOST) {
            pc_set_capture(0); host_pointer(2,0,0);
        }
        if(pc_capture && (e.type==SDL_MOUSEBUTTONDOWN || e.type==SDL_MOUSEBUTTONUP)) {
            int down=e.type==SDL_MOUSEBUTTONDOWN;
            if(e.button.button==SDL_BUTTON_LEFT) { pc_fire=down; host_key(down,103,0); }
            if(e.button.button==SDL_BUTTON_RIGHT) { pc_aim=down; host_key(down,102,0); }
            continue;
        }
        if(pc_capture && e.type==SDL_MOUSEMOTION) { look_dx+=e.motion.xrel; look_dy+=e.motion.yrel; continue; }
        if (e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP) { int alt, k = map_button(e.cbutton.button, &alt); if (k) host_key(e.type == SDL_CONTROLLERBUTTONDOWN, k, alt); }
        if (e.type == SDL_CONTROLLERDEVICEADDED) SDL_GameControllerOpen(e.cdevice.which);
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) host_pointer(0, (float)e.button.x * g_sx, (float)e.button.y * g_sy);
        if (e.type == SDL_MOUSEMOTION && (e.motion.state & SDL_BUTTON_LMASK)) host_pointer(1, (float)e.motion.x * g_sx, (float)e.motion.y * g_sy);
        if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) host_pointer(2, (float)e.button.x * g_sx, (float)e.button.y * g_sy);
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            int w, h; SDL_GL_GetDrawableSize(g_win, &w, &h);
            if (w != g_w || h != g_h) { g_w = w; g_h = h; *resized = 1; int ww, wh; SDL_GetWindowSize(g_win, &ww, &wh); g_sx = (float)w / ww; g_sy = (float)h / wh; }
        }
    }
    pc_update(look_dx,look_dy);
    return 1;
}
static void sdl_present(void) { SDL_GL_SwapWindow(g_win); }
static void sdl_shot(const char* path) {                      // writes the back buffer as a binary PPM
    u8* px = malloc((size_t)g_w * g_h * 3); u8* row = malloc((size_t)g_w * 3);
    dgl_glPixelStorei(0x0D05, 1);                              // GL_PACK_ALIGNMENT
    dgl_glReadPixels(0, 0, g_w, g_h, 0x1907, 0x1401, px);
    for (int y = 0; y < g_h / 2; y++) { u8* a = px + (size_t)y * g_w * 3; u8* b = px + (size_t)(g_h - 1 - y) * g_w * 3; memcpy(row, a, g_w * 3); memcpy(a, b, g_w * 3); memcpy(b, row, g_w * 3); }
    FILE* f = fopen(path, "wb"); if (f) { fprintf(f, "P6\n%d %d\n255\n", g_w, g_h); fwrite(px, 3, (size_t)g_w * g_h, f); fclose(f); }
    free(px); free(row);
}
