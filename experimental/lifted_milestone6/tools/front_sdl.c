// front_sdl.c - window, OpenGL context and event pump (SDL2). Included by host.c when built with -DDS_SDL.
#include <SDL.h>
static void host_pointer(int kind, float x, float y);   // kind: 0 down, 1 move, 2 up (defined in host.c)
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
// Default PC bindings, modelled on the Xperia Play layout. Each entry: SDL scancode -> Android key code (+alt flag).
static int map_scancode(int sc, int* alt) {
    *alt = 0;
    switch (sc) {
        case SDL_SCANCODE_UP: case SDL_SCANCODE_W: return 19;    case SDL_SCANCODE_DOWN: case SDL_SCANCODE_S: return 20;
        case SDL_SCANCODE_LEFT: case SDL_SCANCODE_A: return 21;  case SDL_SCANCODE_RIGHT: case SDL_SCANCODE_D: return 22;
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
    SDL_Event e; *resized = 0;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) return 0;
        // Escape is Android Back/Pause (handled by map_scancode), not a forced shutdown.
        if ((e.type == SDL_KEYDOWN && !e.key.repeat) || e.type == SDL_KEYUP) { int alt, k = map_scancode(e.key.keysym.scancode, &alt); if (k) host_key(e.type == SDL_KEYDOWN, k, alt); }
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
