/* Dead Space Mobile native SDL2 controller remap for the user's original
 * symbolized Linux i686 native-controller build. This is an SDL input shim,
 * not an alternate-fire simulation or Android emulator.
 */
typedef unsigned char Uint8;
typedef short Sint16;
typedef Uint8 (*ButtonFn)(void*,int);
typedef Sint16 (*AxisFn)(void*,int);
extern void *dlsym(void *,const char *);
#define RTLD_NEXT ((void *)-1L)

/* SDL2: A=0 B=1 X=2 Y=3 LEFTSTICK=7 RIGHTSTICK=8
 * LEFTSHOULDER=9 RIGHTSHOULDER=10 DPAD_UP=11, LEFT TRIGGER AXIS=4.
 * Native action dispatch remains exactly as in the supplied game ELF. */
Uint8 SDL_GameControllerGetButton(void *controller, int button) {
    static ButtonFn raw_button;
    static AxisFn raw_axis;
    if (!raw_button) raw_button=(ButtonFn)dlsym(RTLD_NEXT,"SDL_GameControllerGetButton");
    if (!raw_axis) raw_axis=(AxisFn)dlsym(RTLD_NEXT,"SDL_GameControllerGetAxis");
    if (!raw_button || !raw_axis) return 0;
    const int aiming = raw_axis(controller,4)>8192;
    switch (button) {
    case 3:  /* native Triangle alternate fire <- R1 while aiming */
        return (Uint8)(aiming && raw_button(controller,10));
    case 7:  /* native L3 run <- physical L1 */
        return raw_button(controller,9);
    case 9:  /* native L1 quick turn/stasis <- L3, Triangle when aiming */
        return (Uint8)((!aiming && raw_button(controller,7)) ||
                       (aiming && raw_button(controller,3)));
    case 10: /* native R1 Plasma Saw <- physical R1 without aiming */
        return (Uint8)(!aiming && raw_button(controller,10));
    case 11: /* native D-pad Up locator <- D-pad Up or R3 */
        return (Uint8)(raw_button(controller,11) || raw_button(controller,8));
    case 8:
        return 0; /* R3 already folded into locator */
    default:
        return raw_button(controller,button);
    }
}
