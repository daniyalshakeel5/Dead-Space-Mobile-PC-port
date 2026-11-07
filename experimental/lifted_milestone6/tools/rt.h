// Runtime for lifted ARM code. Target: 32-bit x86 (guest pointer == host pointer).
#pragma once
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
typedef uint8_t u8; typedef int8_t s8; typedef uint16_t u16; typedef int16_t s16;
typedef uint32_t u32; typedef int32_t s32; typedef uint64_t u64; typedef int64_t s64;

#if defined(__GNUC__) || defined(__clang__)
typedef u32 __attribute__((may_alias, aligned(1))) u32u;
typedef u16 __attribute__((may_alias, aligned(1))) u16u;
#else
typedef u32 u32u; typedef u16 u16u;
#endif
#define LD32(a) (*(u32u*)(uintptr_t)(u32)(a))
#define LD16(a) ((u32)*(u16u*)(uintptr_t)(u32)(a))
#define LD8(a)  ((u32)*(u8*)(uintptr_t)(u32)(a))
#define LDS16(a) ((u32)(s32)*(s16*)(uintptr_t)(u32)(a))
#define LDS8(a)  ((u32)(s32)*(s8*)(uintptr_t)(u32)(a))
#define ST32(a,v) (*(u32u*)(uintptr_t)(u32)(a) = (u32)(v))
#define ST16(a,v) (*(u16u*)(uintptr_t)(u32)(a) = (u16)(v))
#define ST8(a,v)  (*(u8*)(uintptr_t)(u32)(a) = (u8)(v))

extern u32 g_base;                       // load address of the (relocated) library image
u64 call_ind(u32 target, u32 r0, u32 r1, u32 r2, u32 r3, u32 sp);

static inline u32 sh_lsl(u32 x, u32 n)  { return n < 32 ? x << n : 0; }
static inline u32 sh_lsr(u32 x, u32 n)  { return n < 32 ? x >> n : 0; }
static inline u32 sh_asr(u32 x, u32 n)  { return n < 32 ? (u32)((s32)x >> n) : (u32)((s32)x >> 31); }
static inline u32 sh_ror(u32 x, u32 n)  { n &= 31; return n ? (x >> n) | (x << (32 - n)) : x; }
static inline u32 sh_lsl_c(u32 x, u32 n, u32 c) { if (!n) return c; if (n < 32) return (x >> (32 - n)) & 1; if (n == 32) return x & 1; return 0; }
static inline u32 sh_lsr_c(u32 x, u32 n, u32 c) { if (!n) return c; if (n < 32) return (x >> (n - 1)) & 1; if (n == 32) return x >> 31; return 0; }
static inline u32 sh_asr_c(u32 x, u32 n, u32 c) { if (!n) return c; if (n < 32) return (x >> (n - 1)) & 1; return x >> 31; }
static inline u32 sh_ror_c(u32 x, u32 n, u32 c) { if (!n) return c; n &= 31; if (!n) return x >> 31; return (x >> (n - 1)) & 1; }
static inline u32 clz32(u32 x) { return x ? (u32)__builtin_clz(x) : 32; }
#define PACK(lo, hi) (((u64)(u32)(hi) << 32) | (u32)(lo))

// ---- VFP helpers (VFP registers are modelled as u32 V[32]; D<n> = V[2n] (low), V[2n+1] (high))
#include <math.h>
/* Actual GL drawable height, set by SDL. Reanimated scales 2011 taps
 * for dense displays; keep the original 480p behaviour as baseline. */
extern u32 ds_touch_output_h;
extern u32 ds_touch_output_w;
static inline float ds_reanimated_touch_scale(void) {
    const char* enabled=getenv("DS_REANIMATED_TOUCH");
    if(!enabled || !*enabled || !strcmp(enabled,"0")) return 1.0f;
    float scale=(float)ds_touch_output_h / 480.0f;
    if(scale<1.0f) scale=1.0f;
    if(scale>3.0f) scale=3.0f; /* prevent distant accidental interactions */
    return scale;
}
static inline u32 ds_reanimated_tap_px(u32 value,u32 original_2011_px) {
    /* Preserve other game versions/unknown settings rather than patching
     * unrelated values. The original lib uses 10 and 30 pixels. */
    if(value!=original_2011_px) return value;
    return (u32)((float)value*ds_reanimated_touch_scale()+0.5f);
}
static inline float  u2f(u32 x) { float f; memcpy(&f, &x, 4); return f; }
static inline u32    f2u(float f) { u32 x; memcpy(&x, &f, 4); return x; }
static inline double u2d(u32 lo, u32 hi) { u64 b = ((u64)hi << 32) | lo; double d; memcpy(&d, &b, 8); return d; }
static inline void   d2u(double d, u32* lo, u32* hi) { u64 b; memcpy(&b, &d, 8); *lo = (u32)b; *hi = (u32)(b >> 32); }
static inline u32 cvt_f2s(float f)  { if (f != f) return 0; if (f >= 2147483648.0f) return 0x7fffffffu; if (f <= -2147483648.0f) return 0x80000000u; return (u32)(s32)f; }
static inline u32 cvt_f2u(float f)  { if (f != f || f <= 0) return 0; if (f >= 4294967296.0f) return 0xffffffffu; return (u32)f; }
static inline u32 cvt_d2s(double f) { if (f != f) return 0; if (f >= 2147483648.0) return 0x7fffffffu; if (f <= -2147483648.0) return 0x80000000u; return (u32)(s32)f; }
static inline u32 cvt_d2u(double f) { if (f != f || f <= 0) return 0; if (f >= 4294967296.0) return 0xffffffffu; return (u32)f; }
static inline u32 vcmp_f(float a, float b)  { return (a != a || b != b) ? 3u : a == b ? 6u : a < b ? 8u : 2u; }   // NZCV nibble
static inline u32 vcmp_d(double a, double b){ return (a != a || b != b) ? 3u : a == b ? 6u : a < b ? 8u : 2u; }

// Float results. ARM turns an invalid operation into the default NaN 0x7fc00000; x86 SSE makes 0xffc00000.
// The fix-up costs a compare per float op and only matters for NaN-producing (buggy) input, so it is optional.
static inline u32  f2ur(float f) { u32 x = f2u(f);
#ifdef ARM_NAN_FIXUP
    if (x == 0xffc00000u) x = 0x7fc00000u;
#endif
    return x; }
static inline void d2ur(double d, u32* lo, u32* hi) { d2u(d, lo, hi);
#ifdef ARM_NAN_FIXUP
    if (*hi == 0xfff80000u && *lo == 0) *hi = 0x7ff80000u;
#endif
}

// ---- traps and 64-bit division helpers
void trap_unsupported(u32 addr, const char* why) __attribute__((noreturn));
static inline void rt_divmod64(int sgn, u32 a0, u32 a1, u32 b0, u32 b1, u32* o) {
    u64 q, r;
    if (sgn) {
        s64 a = (s64)PACK(a0, a1), b = (s64)PACK(b0, b1), sq, sr;
        if (b == 0) { sq = 0; sr = a; } else if (a == INT64_MIN && b == -1) { sq = a; sr = 0; } else { sq = a / b; sr = a % b; }
        q = (u64)sq; r = (u64)sr;
    } else {
        u64 a = PACK(a0, a1), b = PACK(b0, b1);
        if (b == 0) { q = 0; r = a; } else { q = a / b; r = a % b; }
    }
    o[0] = (u32)q; o[1] = (u32)(q >> 32); o[2] = (u32)r; o[3] = (u32)(r >> 32);
}
static inline u32 sat32(s64 v) { return v > 2147483647LL ? 0x7fffffffu : v < -2147483648LL ? 0x80000000u : (u32)(s32)v; }
static inline s32 hlf(u32 v, int top) { return top ? (s32)(s16)(v >> 16) : (s32)(s16)v; }
