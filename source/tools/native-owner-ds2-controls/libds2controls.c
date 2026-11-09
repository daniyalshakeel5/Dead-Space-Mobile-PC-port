/* Dead Space Mobile native SDL2 gamepad remap -- Linux i686 (ELF32).
 * Owner-requested controller layout. No Android gestures or alternate-fire
 * emulation: only remaps the SDL button states read by the genuine native
 * controller dispatch path. See PATCH-TECHNICAL-NOTES.md.
 *
 * Minimal declarations avoid adding 32-bit libc development headers to the
 * game-patching environment. The installed 32-bit runtime supplies dlsym.
 */
typedef unsigned char Uint8;
typedef short Sint16;
typedef Uint8 (*ButtonFn)(void*,int);
typedef Sint16 (*AxisFn)(void*,int);
extern void *dlsym(void *,const char *);
#define RTLD_NEXT ((void *)-1L)

/* SDL2 ControllerButton enum: A0 B1 X2 Y3 BACK4 GUIDE5 START6
 * LEFTSTICK7 RIGHTSTICK8 LEFTSHOULDER9 RIGHTSHOULDER10,
 * DPAD_UP11 DOWN12 LEFT13 RIGHT14.
 * ControllerAxis trigger-left=4.
 */
Uint8 SDL_GameControllerGetButton(void *controller, int button) {
    static ButtonFn raw_button;
    static AxisFn raw_axis;
    if (!raw_button) raw_button=(ButtonFn)dlsym(RTLD_NEXT,"SDL_GameControllerGetButton");
    if (!raw_axis) raw_axis=(AxisFn)dlsym(RTLD_NEXT,"SDL_GameControllerGetAxis");
    if (!raw_button || !raw_axis) return 0;
    const int aiming = raw_axis(controller,4)>8192;
    switch(button) {
    case 3:  /* virtual Triangle -> original native ALTERNATE FIRE */
        return (Uint8)(aiming && raw_button(controller,10));  /* L2+R1 */
    case 7:  /* virtual L3 -> native run flag */
        return raw_button(controller,9); /* physical L1 sprint */
    case 9:  /* virtual L1 -> original quick turn/stasis action */
        return (Uint8)((!aiming && raw_button(controller,7)) ||
                       (aiming && raw_button(controller,3)));
                  /* L3 quick turn, Triangle+L2 native stasis */
    case 10: /* physical R1 is Plasma Saw MELEE only when not aiming */
        return (Uint8)(!aiming && raw_button(controller,10));
    case 11: /* native locator: R3 only, freeing D-pad Up for Zero-G jump */
        return raw_button(controller,8);
    case 12: /* native Zero-G jump: physical D-pad Up (previously Down) */
        return raw_button(controller,11);
    case 8:  /* R3 input already folded into locator; avoid duplicate input */
        return 0;
    default:
        return raw_button(controller,button); /* preserve all other bindings */
    }
}
