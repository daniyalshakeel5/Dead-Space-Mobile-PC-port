// front_sdl.c - window, OpenGL context and event pump (SDL2). Included by host.c when built with -DDS_SDL.
#include <SDL.h>
static void host_pointer(int kind, float x, float y);   // kind: 0 down, 1 move, 2 up (defined in host.c)
static void host_pointer_id(int kind, int finger, float x, float y);
static void host_touchpad_id(int kind, int finger, float x, float y);
static void host_key(int down, int keycode, int alt);     // Android key codes (defined in host.c)
static SDL_Window* g_win; static SDL_GLContext g_ctx; static int g_w, g_h; static float g_sx = 1, g_sy = 1;

/* Linux touchscreen mode consumes SDL_FINGER* directly, without inventing
 * joystick/key events. Off on Windows/desktop mouse builds. */
static int touch_enabled(void) {
    static int enabled=-1;
    if(enabled<0) { const char *p=getenv("DS_TOUCHSCREEN"); enabled=p && strcmp(p,"0")!=0; }
    return enabled;
}
#define DS_TOUCH_SLOTS 10
static struct {
    SDL_TouchID device;
    SDL_FingerID finger;
    float x, y;
    int active;
} ds_touch[DS_TOUCH_SLOTS];
static unsigned ds_touch_downs, ds_touch_moves, ds_touch_ups, ds_touch_dups;
static int ds_touch_slot(SDL_TouchID device, SDL_FingerID finger) {
    for(int i=0;i<DS_TOUCH_SLOTS;i++)
        if(ds_touch[i].active && ds_touch[i].device==device && ds_touch[i].finger==finger) return i;
    return -1;
}
static void ds_touch_release_all(void) {
    for(int i=0;i<DS_TOUCH_SLOTS;i++) if(ds_touch[i].active) {
        host_pointer_id(2,i,ds_touch[i].x,ds_touch[i].y);
        ds_touch[i].active=0;
    }
}
static void ds_touch_input(const SDL_TouchFingerEvent* f) {
    int kind=f->type==SDL_FINGERDOWN?0:f->type==SDL_FINGERMOTION?1:2;
    int slot=ds_touch_slot(f->touchId,f->fingerId);
    if(kind==0) {
        if(slot>=0) { ds_touch_dups++; return; }
        for(int i=0;i<DS_TOUCH_SLOTS;i++) if(!ds_touch[i].active) { slot=i; break; }
        if(slot<0) { fprintf(stderr,"[LINUX_TOUCH] warning: exhausted touch slots\n"); return; }
        ds_touch[slot].active=1;
        ds_touch[slot].device=f->touchId;
        ds_touch[slot].finger=f->fingerId;
    } else if(slot<0) {
        return; /* Never send an unmatched MOVE/UP to the original game. */
    }
    float x=fmaxf(0.0f,fminf((float)g_w,f->x*g_w));
    float y=fmaxf(0.0f,fminf((float)g_h,f->y*g_h));
    ds_touch[slot].x=x; ds_touch[slot].y=y;
    host_pointer_id(kind,slot,x,y);
    if(kind==0) ds_touch_downs++;
    if(kind==1) ds_touch_moves++;
    if(kind==2) { ds_touch_ups++; ds_touch[slot].active=0; }
    if(getenv("DS_INPUT_DIAG") && (kind!=1 || ds_touch_moves%120==1))
        fprintf(stderr,"[LINUX_TOUCH] kind=%d slot=%d x=%.1f y=%.1f down=%u move=%u up=%u active=%d\n",
                kind,slot,x,y,ds_touch_downs,ds_touch_moves,ds_touch_ups,
                ds_touch[slot].active);
}
static void pad_init(void);
static int sdl_open(int w, int h) {
    if(touch_enabled()) {
        /* SDL would otherwise synthesize an additional mouse click from
         * each physical touch, which the game can interpret as firing. */
        SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
        SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) { fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError()); return 0; }
    pad_init();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24); SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    Uint32 winflags=SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    const char* fullscreen=getenv("DS_TOUCH_FULLSCREEN");
    if(touch_enabled() && fullscreen && strcmp(fullscreen,"0")!=0)
        winflags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    g_win = SDL_CreateWindow("Dead Space Mobile — Linux Touch",
                             SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,w,h,winflags);
    if (!g_win) { fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError()); return 0; }
    g_ctx = SDL_GL_CreateContext(g_win);
    if (!g_ctx) { fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError()); return 0; }
    SDL_GL_SetSwapInterval(getenv("DS_NOVSYNC") ? 0 : 1);
    gl_load_all(SDL_GL_GetProcAddress);
    SDL_GL_GetDrawableSize(g_win, &g_w, &g_h);
    ds_touch_output_h = g_h > 0 ? (u32)g_h : 480u;
    { int ww, wh; SDL_GetWindowSize(g_win, &ww, &wh); g_sx = (float)g_w / ww; g_sy = (float)g_h / wh; }
    printf("window %dx%d, GL_RENDERER=%s\n", g_w, g_h, dgl_glGetString ? (const char*)dgl_glGetString(0x1F01) : "?");
    if(touch_enabled()) {
        fprintf(stderr,
            "[LINUX_TOUCH] direct SDL multi-touch enabled; drawable=%dx%d; synthetic mouse disabled\n",g_w,g_h);
        fprintf(stderr,
            "[REANIMATED_TOUCH] enabled=%d base720p=%g oldtap=10 expanded=%u olddouble=30 expanded=%u drawable-height=%u\n",
            ds_reanimated_touch_scale()>1.0f,ds_reanimated_touch_scale(),
            ds_reanimated_tap_px(10,10),ds_reanimated_tap_px(30,30),ds_touch_output_h);
    }
    return 1;
}
/* PC movement uses the game's verified left-screen drag path. Separate
 * finger IDs allow movement, mouse look and ordinary menu touches together. */
static int pc_w,pc_a,pc_s,pc_d,pc_shift,pc_capture,pc_move_active,pc_look_active;
static int pc_fire,pc_aim;
static SDL_GameController* pad;
static SDL_JoystickID pad_id=-1;
static SDL_Joystick* test_pad;
static int pad_buttons[SDL_CONTROLLER_BUTTON_MAX];
static int pad_focused=1;
static void pad_release(void);
static void pad_read(float* lx,float* ly,float* rx,float* ry);
static int pc_move_finger(void) {
    const char* p=getenv("DS_PC_MOVE_FINGER");
    /* The game uses the primary pointer for the left virtual joystick. A
     * secondary finger is still available for regression comparisons. */
    return p && !strcmp(p,"1") ? 1 : 0;
}
static float pc_move_x,pc_move_y,pc_look_x,pc_look_y;
/* The Fixes APK does not rotate the view through TouchPad module 1100
 * (physical Windows test). Test native TouchScreen camera drag without
 * repeated pointer-down/up events. All alternatives are diagnostics. */
enum { DS_CAM_DRAG=0, DS_CAM_MOVE_ONLY=1, DS_CAM_PULSE=2, DS_CAM_TOUCHPAD=3 };
static int pc_camera_mode(void) {
    const char* p=getenv("DS_CAMERA_MODE");
    if(p && !strcmp(p,"move-only")) return DS_CAM_MOVE_ONLY;
    if(p && !strcmp(p,"pulse")) return DS_CAM_PULSE;
    if(p && !strcmp(p,"touchpad")) return DS_CAM_TOUCHPAD;
    return DS_CAM_DRAG;
}
static unsigned pc_look_downs,pc_look_moves,pc_look_ups;
static const char* pc_camera_mode_name(void) {
    static const char* names[]={"drag","move-only","pulse","touchpad"};
    return names[pc_camera_mode()];
}
static void pc_look_event(int kind,float x,float y) {
    if(kind==0) ++pc_look_downs;
    if(kind==1) ++pc_look_moves;
    if(kind==2) ++pc_look_ups;
    if(pc_camera_mode()==DS_CAM_TOUCHPAD)
        host_touchpad_id(kind,2,x,y);
    else host_pointer_id(kind,2,x,y);
    if(getenv("DS_INPUT_DIAG") && (kind==0 || kind==2 ||
       (kind==1 && pc_look_moves%120==0)))
        fprintf(stderr,"[CAMERA_EVENT] mode=%s action=%d x=%.2f y=%.2f downs=%u moves=%u ups=%u\n",
                pc_camera_mode_name(),kind,x,y,
                pc_look_downs,pc_look_moves,pc_look_ups);
}
static unsigned pc_input_trace_frames;
/* Context-sensitive bindings use the genuine game's JNI keyboard/touch.
 * State is independent so triggers never accidentally release each other. */
static int pad_aim, pad_fire, pad_cross;
static int pad_r1_held, pad_stomp_held;
static float pad_cross_x, pad_cross_y;
/* Only explicitly recognized melee/stomp gestures enter the touchscreen.
 * Right-stick look goes to the separate game's TouchPad module. */
static int pad_swipe_phase, pad_swipe_direction, pad_swipe_next;
static float pad_swipe_x, pad_swipe_y, pad_swipe_start_y, pad_swipe_end_y;
static int pad_physical_touch_down, pad_physical_touch_finger;
static float pad_physical_x0, pad_physical_y0, pad_physical_x1, pad_physical_y1;
static void pad_begin_swipe(int direction, const char* source) {
    if (pad_swipe_phase) { pad_swipe_next=direction; return; }
    pad_swipe_direction=direction;
    pad_swipe_phase=1;
    if(getenv("DS_INPUT_DIAG")) fprintf(stderr,
        "[PAD_SWIPE] %s from %s; top-left touchscreen; no right-stick action\n",
        direction<0 ? "plasma-saw-up" : "stomp-down",source);
}
static void pad_swipe_tick(void) {
    if (!pad_swipe_phase) return;
    /* Top-left quadrant. Keep the action clear of the lower-left
     * movement pad and the right-side firing/aiming controls. */
    pad_swipe_x=g_w*.19f;
    pad_swipe_start_y=g_h*(pad_swipe_direction<0 ? .39f : .13f);
    pad_swipe_end_y=g_h*(pad_swipe_direction<0 ? .13f : .39f);
    if (pad_swipe_phase==1) {
        pad_swipe_y=pad_swipe_start_y;
        host_pointer_id(0,4,pad_swipe_x,pad_swipe_y);
    } else if (pad_swipe_phase==2 || pad_swipe_phase==3) {
        float step=(pad_swipe_phase==2 ? .5f : 1.0f);
        pad_swipe_y=pad_swipe_start_y+(pad_swipe_end_y-pad_swipe_start_y)*step;
        host_pointer_id(1,4,pad_swipe_x,pad_swipe_y);
    } else {
        host_pointer_id(2,4,pad_swipe_x,pad_swipe_y);
        pad_swipe_phase=0;
        if (pad_swipe_next) {
            int next=pad_swipe_next; pad_swipe_next=0;
            pad_begin_swipe(next,"queued");
        }
        return;
    }
    ++pad_swipe_phase;
}
static void pad_physical_touch(const SDL_Event* e) {
    if (!pad || e->ctouchpad.which!=pad_id || e->ctouchpad.touchpad!=0)
        return;
    if(e->type==SDL_CONTROLLERTOUCHPADDOWN) {
        if(!pad_physical_touch_down) {
            pad_physical_touch_down=1;
            pad_physical_touch_finger=e->ctouchpad.finger;
            pad_physical_x0=pad_physical_x1=e->ctouchpad.x;
            pad_physical_y0=pad_physical_y1=e->ctouchpad.y;
        }
    } else if(pad_physical_touch_down && e->ctouchpad.finger==pad_physical_touch_finger) {
        pad_physical_x1=e->ctouchpad.x; pad_physical_y1=e->ctouchpad.y;
        if(e->type==SDL_CONTROLLERTOUCHPADUP) {
            float dx=pad_physical_x1-pad_physical_x0;
            float dy=pad_physical_y1-pad_physical_y0;
            /* Touch alone is never a fire event; vertical swipes trigger
             * the game's *upper-left* native screen gestures on release. */
            if(fabsf(dy)>.16f && fabsf(dy)>fabsf(dx)*1.25f)
                pad_begin_swipe(dy<0 ? -1 : 1,"physical-PS4-touchpad");
            if(getenv("DS_INPUT_DIAG")) fprintf(stderr,
                "[PAD_TOUCH] dx=%.3f dy=%.3f recognised=%d\n",
                dx,dy,fabsf(dy)>.16f && fabsf(dy)>fabsf(dx)*1.25f);
            pad_physical_touch_down=0;
        }
    }
}
static void pad_input_log(const char* action, int down) {
    if(getenv("DS_INPUT_DIAG")) fprintf(stderr,"[PAD_ACTION] %s=%d\n",action,down);
}
static void pad_special_release(void) {
    if(pad_aim) host_key(0,102,0);
    if(pad_fire) host_key(0,103,0);
     if(pad_cross) {
        host_key(0,23,0);
        host_pointer_id(2,3,pad_cross_x,pad_cross_y);
    }
    if(pad_swipe_phase) host_pointer_id(2,4,pad_swipe_x,pad_swipe_y);
    pad_aim=pad_fire=pad_cross=0;
    pad_r1_held=pad_stomp_held=pad_swipe_phase=pad_swipe_direction=pad_swipe_next=0;
    pad_physical_touch_down=0;
}
static void pc_release(void) {
    if(pc_move_active) host_pointer_id(2,pc_move_finger(),pc_move_x,pc_move_y);
    if(pc_look_active) pc_look_event(2,pc_look_x,pc_look_y);
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
    float len=sqrtf(x*x+y*y),lx=0,ly=0,rx=0,ry=0;
    if(len>1) { x/=len; y/=len; }
    pad_read(&lx,&ly,&rx,&ry);
    if(lx!=0 || ly!=0) { x=lx; y=ly; }
    if (getenv("DS_INPUT_DIAG") && (lx!=0 || ly!=0 || rx!=0 || ry!=0)) {
        if (++pc_input_trace_frames % 120 == 1)
            fprintf(stderr,"[CONTROLLER_AXES] left=%.3f,%.3f right=%.3f,%.3f touch=%d finger=%d\n",
                    lx,ly,rx,ry,pc_move_active,pc_move_finger());
    } else pc_input_trace_frames=0;
    /* Stick camera does not need desktop mouse capture. */
    dx+=rx*4.0f; dy+=ry*4.0f;
    if(x!=0 || y!=0) {
        float bx=g_w*.25f,by=g_h*(500.0f/720.0f);
        if(!pc_move_active) {
            /* The game must consume the down position before the drag.
             * Same-frame down/move can lose the joystick's anchor. */
            pc_move_x=bx; pc_move_y=by;
            host_pointer_id(0,pc_move_finger(),bx,by); pc_move_active=1;
        } else {
            /* A virtual joystick measures displacement from its touch-down
             * anchor. The previous implementation moved 0.25 pixel/frame:
             * on real DualShock hardware this stays near the neutral zone.
             * Send a stable proportional offset while held. The first frame
             * is intentionally down-only so the engine sees the anchor. */
            float radius_x=g_w*.14f, radius_y=g_h*.18f;
            pc_move_x=bx+x*radius_x;
            pc_move_y=by+y*radius_y;
            host_pointer_id(1,pc_move_finger(),pc_move_x,pc_move_y);
        }
    } else if(pc_move_active) {
        host_pointer_id(2,pc_move_finger(),pc_move_x,pc_move_y); pc_move_active=0;
    }
    int look_mode=pc_camera_mode();
    if((look_mode==DS_CAM_PULSE || look_mode==DS_CAM_TOUCHPAD) && pc_look_active) {
        /* Diagnostic compatibility paths send one short gesture per frame. */
        pc_look_event(2,pc_look_x,pc_look_y);
        pc_look_active=0;
    }
    if((pc_capture || rx!=0 || ry!=0) && (dx!=0 || dy!=0)) {
        float bx=g_w*.55f,by=g_h*.50f;
        dx=fmaxf(-g_w*.20f,fminf(g_w*.20f,dx*g_sx));
        dy=fmaxf(-g_h*.40f,fminf(g_h*.40f,dy*g_sy));
        if(look_mode==DS_CAM_DRAG) {
            /* One sustained drag, not a series of taps: touch DOWN only
             * when the stick starts, MOVE while held, UP at neutral.
             * In the original pulse approach every tiny DOWN/UP was
             * interpreted as a shot while panning. */
            if(!pc_look_active) {
                pc_look_x=bx; pc_look_y=by;
                pc_look_event(0,pc_look_x,pc_look_y);
                pc_look_active=1;
            }
            pc_look_x=fmaxf(g_w*.06f,fminf(g_w*.94f,pc_look_x+dx));
            pc_look_y=fmaxf(g_h*.06f,fminf(g_h*.94f,pc_look_y+dy));
            pc_look_event(1,pc_look_x,pc_look_y);
        } else {
            pc_look_x=bx+dx;
            pc_look_y=by+dy;
            if(look_mode!=DS_CAM_MOVE_ONLY)
                pc_look_event(0,bx,by);
            pc_look_event(1,pc_look_x,pc_look_y);
            pc_look_active=look_mode!=DS_CAM_MOVE_ONLY;
        }
    } else if(pc_look_active) {
        pc_look_event(2,pc_look_x,pc_look_y);
        pc_look_active=0;
        if(getenv("DS_INPUT_DIAG"))
            fprintf(stderr,"[CAMERA_STATS] mode=%s downs=%u moves=%u ups=%u\n",
                    pc_camera_mode_name(),pc_look_downs,pc_look_moves,pc_look_ups);
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
        /* Cross, R1, L2 and R2 are contextual in pad_read. Android BACK
         * pauses in this APK, so Circle must not emit Android BACK. */
        case SDL_CONTROLLER_BUTTON_A: case SDL_CONTROLLER_BUTTON_B: return 0;
        case SDL_CONTROLLER_BUTTON_X: return 99; case SDL_CONTROLLER_BUTTON_Y: return 100;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return 0;
        case SDL_CONTROLLER_BUTTON_START: *alt=1; return 4;  /* Options -> pause */
        case SDL_CONTROLLER_BUTTON_BACK: return 109;
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return 19; case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return 20;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return 21; case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return 22;
    }
    return 0;
}
/* SDL normalizes DualShock 4 USB/Bluetooth and other mapped controllers.
 * Only one device owns gameplay input; release it before adopting another. */
static void pad_open(int index) {
    if(pad || !SDL_IsGameController(index)) return;
    pad=SDL_GameControllerOpen(index);
    if(!pad) { fprintf(stderr,"[CONTROLLER] open failed: %s\n",SDL_GetError()); return; }
    pad_id=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad));
    fprintf(stderr,"[CONTROLLER] connected: %s id=%d, SDL touchpads=%d\n",
            SDL_GameControllerName(pad),(int)pad_id,SDL_GameControllerGetNumTouchpads(pad));
}
static void pad_init(void) {
    if(getenv("DS_CONTROLLER_AXES") || getenv("DS_CONTROLLER_BUTTONS") || getenv("DS_CONTROLLER_TRIGGERS")) {
        int index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                                          SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,0);
        if(index>=0) test_pad=SDL_JoystickOpen(index);
        if(test_pad) {
            SDL_JoystickSetVirtualAxis(test_pad,SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768);
            SDL_JoystickSetVirtualAxis(test_pad,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);
        }
        if(!test_pad) fprintf(stderr,"[INPUT_SCRIPT_ERROR] virtual controller: %s\n",SDL_GetError());
    }
    for(int i=0;i<SDL_NumJoysticks();i++) pad_open(i);
}
static void pad_release(void) {
    for(int b=0;b<SDL_CONTROLLER_BUTTON_MAX;b++) if(pad_buttons[b]) {
        int alt,k=map_button(b,&alt); if(k) host_key(0,k,alt);
        pad_buttons[b]=0;
    }
    pad_special_release();
    pc_release();
}
static void pad_stick(Sint16 rawx,Sint16 rawy,float* x,float* y) {
    float ax=rawx/32768.0f,ay=rawy/32768.0f;
    float length=sqrtf(ax*ax+ay*ay),deadzone=.18f;
    if(length<=deadzone) { *x=*y=0; return; }
    float magnitude=(fminf(length,1.0f)-deadzone)/(1.0f-deadzone);
    *x=ax/length*magnitude; *y=ay/length*magnitude;
}
static void pad_read(float* lx,float* ly,float* rx,float* ry) {
    if(!pad || !pad_focused || !SDL_GameControllerGetAttached(pad)) return;
    pad_stick(SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTX),
              SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTY),lx,ly);
    pad_stick(SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_RIGHTX),
              SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_RIGHTY),rx,ry);
    /* Xperia JNI physical keyboard actions: L1=aim, R1=fire.
     * The PC binding makes L2 aim and R2 context-dependent. */
    int aiming=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_TRIGGERLEFT)>8192;
    int r2=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)>8192;
    int firing=aiming && r2;
    int saw=SDL_GameControllerGetButton(pad,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) && !aiming;
    int cross=SDL_GameControllerGetButton(pad,SDL_CONTROLLER_BUTTON_A)!=0;
    if(aiming!=pad_aim) { pad_aim=aiming; host_key(aiming,102,0); pad_input_log("aim-L2",aiming); }
    if(firing!=pad_fire) { pad_fire=firing; host_key(firing,103,0); pad_input_log("fire-R2",firing); }
    /* Touch edition: plasma saw is a swipe UP in the top-left quadrant.
     * DPAD_UP was a menu key and did not trigger the real melee action. */
    if(saw && !pad_r1_held) pad_begin_swipe(-1,"R1");
    if(saw!=pad_r1_held) pad_input_log("plasma-saw-R1",saw);
    pad_r1_held=saw;
    /* Native touchscreen game requires tapping doors and objects.
     * Retain DPAD_CENTER for menus, but supply a centre-screen tap too. */
    if(cross!=pad_cross) {
        if(cross) {
            pad_cross_x=g_w*.50f; pad_cross_y=g_h*.50f;
            host_key(1,23,0);
            host_pointer_id(0,3,pad_cross_x,pad_cross_y);
        } else {
            host_key(0,23,0);
            host_pointer_id(2,3,pad_cross_x,pad_cross_y);
        }
        pad_cross=cross; pad_input_log("interact-X",cross);
    }
    /* Stomp uses the same upper-left swipe area, but downward. */
    int stomp=r2 && !aiming;
    if(stomp && !pad_stomp_held) pad_begin_swipe(1,"R2-without-L2");
    if(stomp!=pad_stomp_held) pad_input_log("stomp-R2",stomp);
    pad_stomp_held=stomp;
    pad_swipe_tick();
    for(int b=0;b<SDL_CONTROLLER_BUTTON_MAX;b++) {
        int down=SDL_GameControllerGetButton(pad,(SDL_GameControllerButton)b)!=0;
        if(b==SDL_CONTROLLER_BUTTON_A || b==SDL_CONTROLLER_BUTTON_B ||
           b==SDL_CONTROLLER_BUTTON_LEFTSHOULDER ||
           b==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
            pad_buttons[b]=down; continue;
        }
        if(down!=pad_buttons[b]) {
            int alt,k=map_button(b,&alt);
            if(k) host_key(down,k,alt);
            if(k && getenv("DS_INPUT_DIAG"))
                fprintf(stderr,"[PAD_KEY] button=%d key=%d down=%d\n",b,k,down);
            pad_buttons[b]=down;
        }
    }
}
/* Virtual axes go through SDL's device/mapping/state pipeline, then the same
 * polling path as hardware. The game executable and assets remain genuine. */
static void pad_script(int frame) {
    if(!test_pad) return;
    int axes[4]={0,0,0,0};
    const char* p=getenv("DS_CONTROLLER_AXES");
    while(p && *p) {
        int start,end,lx,ly,rx,ry,n=0;
        if(sscanf(p,"%d:%d:%d:%d:%d:%d%n",&start,&end,&lx,&ly,&rx,&ry,&n)!=6 || !n) break;
        if(frame>=start && frame<end) {
            axes[0]=lx; axes[1]=ly; axes[2]=rx; axes[3]=ry;
            if(frame==start) fprintf(stderr,"[INPUT_SCRIPT] controller frame=%d left=%d,%d right=%d,%d\n",frame,lx,ly,rx,ry);
        }
        p+=n; if(*p==',') ++p; else break;
    }
    for(int i=0;i<4;i++) SDL_JoystickSetVirtualAxis(test_pad,i,(Sint16)fmaxf(-32768,fminf(32767,axes[i])));
    /* Script physical SDL buttons with genuine APK execution.
     * DS_CONTROLLER_BUTTONS=21000:21005:a,22000:22015:rightshoulder */
    int buttons[SDL_CONTROLLER_BUTTON_MAX]={0};
    p=getenv("DS_CONTROLLER_BUTTONS");
    while(p && *p) {
        int start,end,n=0; char name[48]={0};
        if(sscanf(p,"%d:%d:%47[^,]%n",&start,&end,name,&n)!=3 || !n) break;
        SDL_GameControllerButton b=SDL_GameControllerGetButtonFromString(name);
        if(b!=SDL_CONTROLLER_BUTTON_INVALID && frame>=start && frame<end) {
            buttons[b]=1;
            if(frame==start) fprintf(stderr,"[INPUT_SCRIPT] button=%s down at frame=%d\n",name,frame);
        }
        p+=n; if(*p==',') ++p; else break;
    }
    for(int i=0;i<SDL_CONTROLLER_BUTTON_MAX;i++)
        SDL_JoystickSetVirtualButton(test_pad,i,(Uint8)buttons[i]);
    const char* swipes=getenv("DS_PAD_SWIPES");
    while(swipes && *swipes) {
        int start,dir,n=0;
        if(sscanf(swipes,"%d:%d%n",&start,&dir,&n)!=2 || !n) break;
        if(frame==start) pad_begin_swipe(dir<0 ? -1 : 1,"CI-physical-gesture-path");
        swipes+=n;
        if(*swipes==',') ++swipes; else break;
    }
    int triggers[2]={-32768,-32768};
    p=getenv("DS_CONTROLLER_TRIGGERS");
    while(p && *p) {
        int start,end,l2,r2,n=0;
        if(sscanf(p,"%d:%d:%d:%d%n",&start,&end,&l2,&r2,&n)!=4 || !n) break;
        if(frame>=start && frame<end) {
            triggers[0]=l2; triggers[1]=r2;
            if(frame==start) fprintf(stderr,"[INPUT_SCRIPT] triggers L2=%d R2=%d frame=%d\n",l2,r2,frame);
        }
        p+=n; if(*p==',') ++p; else break;
    }
    SDL_JoystickSetVirtualAxis(test_pad,SDL_CONTROLLER_AXIS_TRIGGERLEFT,
        (Sint16)fmaxf(-32768,fminf(32767,triggers[0])));
    SDL_JoystickSetVirtualAxis(test_pad,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,
        (Sint16)fmaxf(-32768,fminf(32767,triggers[1])));
    SDL_JoystickUpdate();
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
    /* Genuine-game CI exercises the same SDL_FINGER* events as a real Linux
     * touch panel. Format: start:end:finger:x0:y0:x1:y1 (window pixels). */
    p=getenv("DS_TOUCH_GESTURES");
    while(touch_enabled() && p && *p) {
        int start,end,finger,x0,y0,x1,y1,n=0;
        if(sscanf(p,"%d:%d:%d:%d:%d:%d:%d%n",&start,&end,&finger,&x0,&y0,&x1,&y1,&n)!=7 || n<=0) break;
        if(start>0 && end>start && finger>=0 && frame>=start && frame<=end) {
            SDL_Event e; memset(&e,0,sizeof e);
            e.type=frame==start ? SDL_FINGERDOWN : frame==end ? SDL_FINGERUP : SDL_FINGERMOTION;
            e.tfinger.type=e.type;
            e.tfinger.touchId=(SDL_TouchID)771;
            e.tfinger.fingerId=(SDL_FingerID)(finger+100);
            e.tfinger.x=(x0+(x1-x0)*(float)(frame-start)/(end-start)) / (float)g_w;
            e.tfinger.y=(y0+(y1-y0)*(float)(frame-start)/(end-start)) / (float)g_h;
            e.tfinger.pressure=1.0f;
            if(SDL_PushEvent(&e)<0)
                fprintf(stderr,"[LINUX_TOUCH_ERROR] failed script event: %s\n",SDL_GetError());
            if(frame==start || frame==end) fprintf(stderr,
                "[LINUX_TOUCH_SCRIPT] frame=%d finger=%d kind=%d from=%d,%d to=%d,%d\n",
                frame,finger,e.type==SDL_FINGERDOWN?0:2,x0,y0,x1,y1);
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
    static int frame; sdl_script_input(++frame); pad_script(frame);
    SDL_Event e; float look_dx=0,look_dy=0; *resized = 0;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { if(touch_enabled()) ds_touch_release_all(); pad_release(); return 0; }
        if(touch_enabled() && (e.type==SDL_FINGERDOWN ||
                               e.type==SDL_FINGERMOTION || e.type==SDL_FINGERUP)) {
            ds_touch_input(&e.tfinger);
            continue;
        }
        /* Some compositors still send SDL_TOUCH_MOUSEID despite SDL hints. */
        if(touch_enabled() && ((e.type==SDL_MOUSEBUTTONDOWN ||
                               e.type==SDL_MOUSEBUTTONUP) &&
                              e.button.which==SDL_TOUCH_MOUSEID)) continue;
        if(touch_enabled() && e.type==SDL_MOUSEMOTION &&
           e.motion.which==SDL_TOUCH_MOUSEID) continue;
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
            pad_focused=0; pad_release(); pc_set_capture(0);
            if(touch_enabled()) ds_touch_release_all();
            else host_pointer(2,0,0);
        }
        if(pc_capture && (e.type==SDL_MOUSEBUTTONDOWN || e.type==SDL_MOUSEBUTTONUP)) {
            int down=e.type==SDL_MOUSEBUTTONDOWN;
            if(e.button.button==SDL_BUTTON_LEFT) { pc_fire=down; host_key(down,103,0); }
            if(e.button.button==SDL_BUTTON_RIGHT) { pc_aim=down; host_key(down,102,0); }
            continue;
        }
        if(pc_capture && e.type==SDL_MOUSEMOTION) { look_dx+=e.motion.xrel; look_dy+=e.motion.yrel; continue; }
        if(e.type==SDL_WINDOWEVENT && e.window.event==SDL_WINDOWEVENT_FOCUS_GAINED) pad_focused=1;
        if(e.type==SDL_CONTROLLERDEVICEADDED) pad_open(e.cdevice.which);
        if(e.type==SDL_CONTROLLERTOUCHPADDOWN ||
           e.type==SDL_CONTROLLERTOUCHPADMOTION ||
           e.type==SDL_CONTROLLERTOUCHPADUP) {
            pad_physical_touch(&e);
            continue;  /* no implicit screen tap or fire */
        }
        if(e.type==SDL_CONTROLLERDEVICEREMOVED && e.cdevice.which==pad_id) {
            pad_release(); SDL_GameControllerClose(pad); pad=NULL; pad_id=-1;
            fprintf(stderr,"[CONTROLLER] disconnected\n");
            for(int i=0;i<SDL_NumJoysticks();i++) pad_open(i);
        }
        /* A physical mouse remains a fallback, but cannot steal finger 0
         * from a running touchscreen movement gesture. */
        int mouse_id=touch_enabled()?12:0;
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) host_pointer_id(0,mouse_id, (float)e.button.x * g_sx, (float)e.button.y * g_sy);
        if (e.type == SDL_MOUSEMOTION && (e.motion.state & SDL_BUTTON_LMASK)) host_pointer_id(1,mouse_id, (float)e.motion.x * g_sx, (float)e.motion.y * g_sy);
        if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) host_pointer_id(2,mouse_id, (float)e.button.x * g_sx, (float)e.button.y * g_sy);
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            int w, h; SDL_GL_GetDrawableSize(g_win, &w, &h);
            if (w != g_w || h != g_h) { g_w = w; g_h = h; ds_touch_output_h = h>0?(u32)h:480u; *resized = 1; int ww, wh; SDL_GetWindowSize(g_win, &ww, &wh); g_sx = (float)w / ww; g_sy = (float)h / wh; }
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
