// Native host for the translated libDeadSpace.so (32-bit x86, guest pointer == host pointer).
// Loads the original ELF image as data, relocates it, binds imports to the shims below,
// runs the static constructors and JNI_OnLoad through the translated code.
//
// -DCOMPARE_MODE restricts the shims to exactly what the Python/Unicorn reference implements,
// so import-call traces can be compared one to one.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#if defined(_WIN32)
#include "windows_lifted_compat.h"
#else
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/syscall.h>
#include <dirent.h>
#include <execinfo.h>
#include <ucontext.h>
#endif
#include "protos.h"

#define STR2(x) #x
typedef struct { u8 ident[16]; u16 type, machine; u32 version, entry, phoff, shoff, flags; u16 ehsize, phentsize, phnum, shentsize, shnum, shstrndx; } Ehdr;
typedef struct { u32 type, offset, vaddr, paddr, filesz, memsz, flags, align; } Phdr;
typedef struct { s32 tag; u32 val; } Dyn;
typedef struct { u32 name, value, size; u8 info, other; u16 shndx; } Sym;
typedef struct { u32 offset, info; } Rel;

u32 g_base;
static u32 g_top;
static fn_t* dispatch;                       // one slot per 4 bytes of image
#define STUB_SLOTS 4096
static u8* stub_region;
static fn_t stub_fns[STUB_SLOTS];
static const char* stub_names[STUB_SLOTS];
static u32 nstubs;
static __thread u32 g_stub_idx;                     // which stub is being called (per thread: JNI calls come from several threads)
static FILE* trace_f;
static u32 g_sp0;                            // guest stack top (used for callbacks from shims)

static void tr(const char* n) { if (trace_f) fprintf(trace_f, "%s\n", n); }

u64 unlifted_called(u32 addr, const char* name) {
    fprintf(stderr, "!! call into untranslated function %#x (%s)\n", addr, name); fflush(stderr); exit(3);
}
void trap_unsupported(u32 addr, const char* why) {
    fprintf(stderr, "!! translated code hit an unsupported/unreachable site at %#x (%s)\n", addr, why); fflush(stderr); exit(3);
}
static struct { const char* name; int n; } unimpl[512];
static int nunimpl;
u64 unimplemented_import(const char* name) {
    tr(name);
    for (int i = 0; i < nunimpl; i++) if (!strcmp(unimpl[i].name, name)) { unimpl[i].n++; return 0; }
    if (nunimpl < 512) { unimpl[nunimpl].name = name; unimpl[nunimpl++].n = 1; printf("   [unimplemented import] %s\n", name); }
    return 0;
}

u64 call_ind(u32 t, u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {
    u32 a = t & ~1u, off = a - g_base;
    fn_t fn = 0;
    if (off < g_top) fn = dispatch[off >> 2];
    else {
        u32 so = t - (u32)(uintptr_t)stub_region;
        if (so < STUB_SLOTS * 4u) { g_stub_idx = so >> 2; fn = stub_fns[g_stub_idx]; }
    }
    if (!fn) { fprintf(stderr, "!! call_ind: bad target %#x\n", t); exit(3); }
    return fn(r0, r1, r2, r3, sp);
}

static u32 make_stub(const char* name, fn_t fn) {
    if (nstubs >= STUB_SLOTS) { fprintf(stderr, "stub table full\n"); exit(2); }
    stub_fns[nstubs] = fn; stub_names[nstubs] = name;
    return (u32)(uintptr_t)stub_region + 4 * nstubs++;
}

// ------------------------------------------------------------------------------------------ shims
#define IMP(n) u64 imp_##n(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp)
#define T(n) tr(#n)
#define RET(v) return (u64)(u32)(v)
#define P(x) ((void*)(uintptr_t)(x))
#define U(p) ((u32)(uintptr_t)(p))
#ifdef COMPARE_MODE
#define EXTRA(...)
#else
#define EXTRA(...) __VA_ARGS__
#endif

// ---- memory and strings (the reference implements these)
IMP(malloc)  { T(malloc);  RET(U(malloc(r0))); }
IMP(calloc)  { T(calloc);  RET(U(calloc(r0, r1))); }
IMP(free)    { T(free);    free(P(r0)); RET(0); }
IMP(realloc) { T(realloc); RET(U(realloc(P(r0), r1))); }
IMP(memcpy)  { T(memcpy);  memmove(P(r0), P(r1), r2); RET(r0); }
IMP(memmove) { T(memmove); memmove(P(r0), P(r1), r2); RET(r0); }
IMP(memset)  { T(memset);  memset(P(r0), (int)r1, r2); RET(r0); }
IMP(memcmp)  { T(memcmp);  int c = memcmp(P(r0), P(r1), r2); RET(c < 0 ? -1 : c > 0); }
IMP(strlen)  { T(strlen);  RET(strlen(P(r0))); }
IMP(strcmp)  { T(strcmp);  int c = strcmp(P(r0), P(r1)); RET(c < 0 ? -1 : c > 0); }
IMP(strncmp) { T(strncmp); int c = strncmp(P(r0), P(r1), r2); RET(c < 0 ? -1 : c > 0); }
IMP(strcpy)  { T(strcpy);  strcpy(P(r0), P(r1)); RET(r0); }
IMP(strncpy) { T(strncpy); strncpy(P(r0), P(r1), r2); RET(r0); }
IMP(strcat)  { T(strcat);  strcat(P(r0), P(r1)); RET(r0); }
IMP(strchr)  { T(strchr);  RET(U(strchr(P(r0), (int)r1))); }
IMP(strrchr) { T(strrchr); RET(U(strrchr(P(r0), (int)r1))); }
IMP(strstr)  { T(strstr);  RET(U(strstr(P(r0), P(r1)))); }
IMP(strdup)  { T(strdup);  RET(U(strdup(P(r0)))); }

// ---- C++ runtime
IMP(__aeabi_atexit) { T(__aeabi_atexit); RET(0); }
IMP(__cxa_guard_acquire) { T(__cxa_guard_acquire); RET((*(u8*)P(r0) & 1) ? 0 : 1); }
IMP(__cxa_guard_release) { T(__cxa_guard_release); *(u8*)P(r0) = 1; RET(0); }
IMP(__stack_chk_fail) { T(__stack_chk_fail); fprintf(stderr, "!! stack check failed\n"); exit(3); }
IMP(abort)   { T(abort); fprintf(stderr, "!! guest called abort()\n"); exit(3); }

#ifdef COMPARE_MODE
// ---- threads: single-threaded for now (every lock succeeds)
#define ZERO_IMPS(X) X(pthread_mutex_init) X(pthread_mutex_lock) X(pthread_mutex_unlock) X(pthread_mutex_destroy) \
    X(pthread_mutex_trylock) X(pthread_mutexattr_init) X(pthread_mutexattr_destroy) X(pthread_mutexattr_settype) \
    X(pthread_mutexattr_setpshared) X(pthread_cond_init) X(pthread_cond_signal) X(pthread_cond_broadcast) \
    X(pthread_cond_destroy) X(pthread_attr_init) X(pthread_attr_destroy) X(pthread_attr_setstacksize) \
    X(pthread_attr_setdetachstate) X(sem_init) X(sem_post) X(sem_destroy) X(sched_yield)
#define DEF_ZERO(n) IMP(n) { T(n); RET(0); }
ZERO_IMPS(DEF_ZERO)
static u32 g_tls[64]; static u32 g_nkey = 1;
IMP(pthread_key_create)   { T(pthread_key_create); *(u32*)P(r0) = g_nkey++; RET(0); }
IMP(pthread_getspecific)  { T(pthread_getspecific); RET(r0 < 64 ? g_tls[r0] : 0); }
IMP(pthread_setspecific)  { T(pthread_setspecific); if (r0 < 64) g_tls[r0] = r1; RET(0); }
IMP(pthread_self)         { T(pthread_self); RET(1); }
IMP(pthread_once) {
    T(pthread_once);
    if (*(u32*)P(r0) == 0) { *(u32*)P(r0) = 1; call_ind(r1, 0, 0, 0, 0, sp); }
    RET(0);
}

#endif

// ---- bionic atomics
IMP(__atomic_inc)  { T(__atomic_inc);  u32 v = *(u32*)P(r0); *(u32*)P(r0) = v + 1; RET(v); }
IMP(__atomic_dec)  { T(__atomic_dec);  u32 v = *(u32*)P(r0); *(u32*)P(r0) = v - 1; RET(v); }
IMP(__atomic_swap) { T(__atomic_swap); u32 v = *(u32*)P(r1); *(u32*)P(r1) = r0; RET(v); }
IMP(__atomic_cmpxchg) { T(__atomic_cmpxchg); u32 v = *(u32*)P(r2); if (v == r0) { *(u32*)P(r2) = r1; RET(0); } RET(1); }

// ---- address space (anonymous mappings and a private brk area, like the reference)
static u32 g_brk, g_brk_end;
IMP(mmap) {
    T(mmap);
    u32 len = (r1 + 0xFFF) & ~0xFFFu, flags = r3, fd = sp ? *(u32*)P(sp) : 0;
    if (!(flags & 0x20) && fd != 0xFFFFFFFFu) { printf("   [mmap file-backed fd=%d]\n", (int)fd); RET(0xFFFFFFFFu); }
    void* p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    RET(p == MAP_FAILED ? 0xFFFFFFFFu : U(p));
}
IMP(munmap) { T(munmap); RET(0); }
IMP(sbrk) {
    T(sbrk);
    if (!g_brk) { void* p = mmap(NULL, 0x10000000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0); g_brk = U(p); }
    u32 old = g_brk; g_brk += r0; RET(old);
}

// ---- misc libc
/* Bionic __errno() is thread-local, not shared between guest pthreads. */
static __thread int g_errno;
static u32 data_object(const char* name);
IMP(__errno)  { T(__errno); RET(U(&g_errno)); }
IMP(sysconf)  { T(sysconf); RET(r0 == 39 ? 4096 : 1); }
IMP(getenv)   { T(getenv);  RET(0); }
IMP(time)     { T(time);    RET(1700000000u); }
IMP(clock_gettime) {
    T(clock_gettime);
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    ((u32*)P(r1))[0] = (u32)ts.tv_sec; ((u32*)P(r1))[1] = (u32)ts.tv_nsec; RET(0);
}
IMP(__android_log_print) { T(__android_log_print); RET(0); }
IMP(__android_log_write) { T(__android_log_write); RET(0); }
#ifdef COMPARE_MODE
IMP(fopen)    { T(fopen); printf("   [fopen] %s (%s) -> NULL\n", (char*)P(r0), (char*)P(r1)); RET(0); }
#endif

// ---- everything below is not implemented by the reference: only in the real build
EXTRA(
IMP(clock)  { T(clock);  RET((u32)(clock())); }
IMP(gettimeofday) { T(gettimeofday); struct timeval tv; gettimeofday(&tv, 0); if (r0) { ((u32*)P(r0))[0] = (u32)tv.tv_sec; ((u32*)P(r0))[1] = (u32)tv.tv_usec; } RET(0); }
IMP(usleep) { T(usleep); usleep(r0); RET(0); }
IMP(atoi)   { T(atoi);   RET(atoi(P(r0))); }
IMP(strtol) { T(strtol); char* e; long v = strtol(P(r0), &e, (int)r2); if (r1) *(u32*)P(r1) = U(e); RET(v); }
IMP(strtoul){ T(strtoul);char* e; unsigned long v = strtoul(P(r0), &e, (int)r2); if (r1) *(u32*)P(r1) = U(e); RET(v); }
IMP(strtod) { T(strtod); char* e; double v = strtod(P(r0), &e); if (r1) *(u32*)P(r1) = U(e); u32 lo, hi; d2u(v, &lo, &hi); return PACK(lo, hi); }
IMP(strtok) { T(strtok); RET(U(strtok(P(r0), P(r1)))); }
IMP(lrand48){ T(lrand48);RET(lrand48()); }
IMP(exit)   { T(exit);   printf("guest exit(%d)\n", (int)r0); fflush(stdout); exit((int)r0); }
)

// ---- math (softfp: floats travel as bits in core registers, doubles as lo/hi pairs)
#define F1(n, fn) IMP(n) { T(n); RET(f2u(fn(u2f(r0)))); }
#define F2(n, fn) IMP(n) { T(n); RET(f2u(fn(u2f(r0), u2f(r1)))); }
#define D1(n, fn) IMP(n) { T(n); u32 lo, hi; d2u(fn(u2d(r0, r1)), &lo, &hi); return PACK(lo, hi); }
#define D2(n, fn) IMP(n) { T(n); u32 lo, hi; d2u(fn(u2d(r0, r1), u2d(r2, r3)), &lo, &hi); return PACK(lo, hi); }
EXTRA(
F1(sinf, sinf) F1(cosf, cosf) F1(tanf, tanf) F1(asinf, asinf) F1(acosf, acosf) F1(atanf, atanf)
F1(expf, expf) F1(logf, logf) F1(log10f, log10f) F1(sqrtf, sqrtf) F1(floorf, floorf) F1(ceilf, ceilf)
F2(atan2f, atan2f) F2(powf, powf) F2(fmodf, fmodf)
D1(sin, sin) D1(cos, cos) D1(tan, tan) D1(acos, acos) D1(atan, atan) D1(exp, exp) D1(log, log)
D1(log10, log10) D1(sqrt, sqrt) D1(floor, floor) D1(ceil, ceil) D2(atan2, atan2) D2(pow, pow)
IMP(ldexp) { T(ldexp); u32 lo, hi; d2u(ldexp(u2d(r0, r1), (int)r2), &lo, &hi); return PACK(lo, hi); }
IMP(modf)  { T(modf);  double ip; double f = modf(u2d(r0, r1), &ip); u32 l, h; d2u(ip, &l, &h); ((u32*)P(r2))[0] = l; ((u32*)P(r2))[1] = h; d2u(f, &l, &h); return PACK(l, h); }
IMP(__isinf) { T(__isinf); RET(isinf(u2d(r0, r1)) ? (u2d(r0, r1) < 0 ? -1 : 1) : 0); }
)

// ---- printf family: reads varargs the way the ARM EABI passes them
typedef struct { const u32* regs; int nregs, ri; const u32* mem; } AR;
static u32 ar32(AR* a) { return a->ri < a->nregs ? a->regs[a->ri++] : *a->mem++; }
static u64 ar64(AR* a) {
    if (a->ri < a->nregs) { if (a->ri & 1) a->ri++; }
    if (a->ri + 1 < a->nregs) { u64 v = a->regs[a->ri] | ((u64)a->regs[a->ri + 1] << 32); a->ri += 2; return v; }
    if (a->ri < a->nregs) a->ri = a->nregs;                       // last register is skipped
    if ((uintptr_t)a->mem & 7) a->mem++;
    u64 v = a->mem[0] | ((u64)a->mem[1] << 32); a->mem += 2; return v;
}
static int gformat(char* out, size_t cap, const char* fmt, AR* a) {
    size_t n = 0;
#define PUT(c) do { if (n + 1 < cap) out[n] = (char)(c); n++; } while (0)
    for (; *fmt; fmt++) {
        if (*fmt != '%') { PUT(*fmt); continue; }
        char spec[48]; int sl = 0; spec[sl++] = *fmt++;
        while (strchr("-+ #0", *fmt) && *fmt) spec[sl++] = *fmt++;
        if (*fmt == '*') { sl += snprintf(spec + sl, 12, "%d", (int)ar32(a)); fmt++; } else while (*fmt >= '0' && *fmt <= '9') spec[sl++] = *fmt++;
        if (*fmt == '.') { spec[sl++] = *fmt++; if (*fmt == '*') { sl += snprintf(spec + sl, 12, "%d", (int)ar32(a)); fmt++; } else while (*fmt >= '0' && *fmt <= '9') spec[sl++] = *fmt++; }
        int ll = 0;
        while (strchr("hlzjtL", *fmt) && *fmt) { if (*fmt == 'l' && fmt[1] == 'l') { ll = 1; fmt++; } if (*fmt == 'j') ll = 1; fmt++; }
        char c = *fmt; char tmp[512]; int tl = 0;
        spec[sl] = 0;
        if (c == '%') { PUT('%'); continue; }
        else if (c == 'd' || c == 'i') { strcat(spec, "lld"); long long v = ll ? (long long)ar64(a) : (long long)(s32)ar32(a); tl = snprintf(tmp, sizeof tmp, spec, v); }
        else if (c == 'u' || c == 'x' || c == 'X' || c == 'o') { char cs[4] = {'l', 'l', c, 0}; strcat(spec, cs); unsigned long long v = ll ? ar64(a) : (unsigned long long)ar32(a); tl = snprintf(tmp, sizeof tmp, spec, v); }
        else if (c == 'c') { strcat(spec, "c"); tl = snprintf(tmp, sizeof tmp, spec, (int)ar32(a)); }
        else if (c == 's') { strcat(spec, "s"); const char* s = P(ar32(a)); tl = snprintf(tmp, sizeof tmp, spec, s ? s : "(null)"); }
        else if (c == 'p') { u32 v = ar32(a); tl = snprintf(tmp, sizeof tmp, v ? "0x%x" : "(nil)", v); }
        else if (strchr("fFeEgGaA", c)) { char cs[2] = {c, 0}; strcat(spec, cs); u64 b = ar64(a); double d; memcpy(&d, &b, 8); tl = snprintf(tmp, sizeof tmp, spec, d); }
        else if (c == 'n') { ar32(a); continue; }
        else { if (!c) break; PUT('%'); PUT(c); continue; }
        if (tl > (int)sizeof tmp - 1) tl = sizeof tmp - 1;
        for (int i = 0; i < tl; i++) PUT(tmp[i]);
    }
    if (cap) out[n < cap ? n : cap - 1] = 0;
    return (int)n;
}
EXTRA(
static int fmt_regs(char* out, size_t cap, u32 fmtp, u32 r2, u32 r3, u32 sp, int first_reg) {
    u32 regs[4] = {0, 0, r2, r3};
    AR a = {regs, 4, first_reg, (const u32*)P(sp)};
    return gformat(out, cap, P(fmtp), &a);
}
IMP(snprintf) { T(snprintf); u32 regs[4] = {0, 0, 0, r3}; AR a = {regs, 4, 3, (const u32*)P(sp)}; RET(gformat(P(r0), r1, P(r2), &a)); }
IMP(sprintf)  { T(sprintf);  RET(fmt_regs(P(r0), 0x7fffffff, r1, r2, r3, sp, 2)); }
IMP(vsnprintf){ T(vsnprintf); AR a = {0, 0, 0, *(const u32**)&(u32){r3}}; RET(gformat(P(r0), r1, P(r2), &a)); }
IMP(vsprintf) { T(vsprintf);  AR a = {0, 0, 0, *(const u32**)&(u32){r2}}; RET(gformat(P(r0), 0x7fffffff, P(r1), &a)); }
)


// ---- qsort with a guest comparator (own implementation: portable, no global state)
EXTRA(
static void gsort(u8* base, u32 n, u32 sz, u32 cmp, u32 sp) {
    if (n < 2) return;
    u8* tmp = malloc(sz);
    for (u32 gap = n / 2; gap > 0; gap /= 2)
        for (u32 i = gap; i < n; i++) {
            memcpy(tmp, base + (size_t)i * sz, sz);
            u32 j = i;
            while (j >= gap && (s32)call_ind(cmp, U(base + (size_t)(j - gap) * sz), U(tmp), 0, 0, sp) > 0) {
                memcpy(base + (size_t)j * sz, base + (size_t)(j - gap) * sz, sz); j -= gap;
            }
            memcpy(base + (size_t)j * sz, tmp, sz);
        }
    free(tmp);
}
IMP(qsort) { T(qsort); gsort(P(r0), r1, r2, r3, sp); RET(0); }
)

#ifndef COMPARE_MODE
#define DATA_OBJECT_DECL
static u32 data_object(const char* name);
#endif
// ------------------------------------------------------------------------------------------ JNI
static int g_natives; static u32 g_env_obj[2], g_vm_obj[2];
#include "jni_vm.c"
#ifndef COMPARE_MODE
#include "gl_shim.c"
#ifdef DS_SDL
#include "front_sdl.c"
#endif
#endif

// ------------------------------------------------------------------------------------------ loader
static const char* data_syms[] = {"__sF", "__stack_chk_guard", "_ctype_", "_tolower_tab_", "_toupper_tab_", "__page_size", "__dso_handle", "tzname"};
static u32 data_object(const char* name) {
    static struct { char* name; u32 addr; } cache[256]; static int n;
    for (int i = 0; i < n; i++) if (!strcmp(cache[i].name, name)) return cache[i].addr;
    size_t sz = 0x100;
    if (!strcmp(name, "__sF")) sz = 3 * 88; else if (!strcmp(name, "_ctype_")) sz = 257;
    else if (!strncmp(name, "_tolower_tab_", 13) || !strncmp(name, "_toupper_tab_", 13)) sz = 257 * 2;
    u8* p = calloc(1, sz + 16);
    if (!strcmp(name, "__stack_chk_guard")) *(u32*)p = 0xA5A5C0DEu;
    if (!strcmp(name, "__page_size")) *(u32*)p = 4096;
    if (!strcmp(name, "_ctype_")) for (int c = 0; c < 256; c++) {
        u8 f = 0;
        if (c >= 'A' && c <= 'Z') f |= 0x01; if (c >= 'a' && c <= 'z') f |= 0x02; if (c >= '0' && c <= '9') f |= 0x04;
        if ((c >= 9 && c <= 13) || c == 32) f |= 0x08; if (c > 32 && c < 127 && !(f & 7)) f |= 0x10;
        if (c < 32 || c == 127) f |= 0x20; if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) f |= 0x40;
        if (c == 32) f |= 0x80; p[c + 1] = f;
    }
    if (!strncmp(name, "_tolower_tab_", 13) || !strncmp(name, "_toupper_tab_", 13)) for (int c = -1; c < 256; c++) {
        int up = !strncmp(name, "_toupper_tab_", 13); int v = c < 0 ? c : (up ? ((c >= 'a' && c <= 'z') ? c - 32 : c) : ((c >= 'A' && c <= 'Z') ? c + 32 : c));
        ((s16*)p)[c + 1] = (s16)v;
    }
    if (!strcmp(name, "_ctype_") || !strncmp(name, "_tolower_tab_", 13) || !strncmp(name, "_toupper_tab_", 13)) {
        u8* cell = calloc(1, 16); *(u32*)cell = U(p); p = cell;       // bionic declares these as pointer variables
    }
    cache[n].name = strdup(name); cache[n].addr = U(p); return cache[n++].addr;
}
static int is_data_sym(const char* n) {
    for (size_t i = 0; i < sizeof data_syms / sizeof *data_syms; i++) if (!strcmp(n, data_syms[i])) return 1;
    return !strncmp(n, "_ZTV", 4) || !strncmp(n, "_ZTI", 4) || !strncmp(n, "_ZTS", 4);
}
static u32 import_addr(const char* name) {
    if (is_data_sym(name)) return data_object(name);
    for (u32 i = 0; i < nstubs; i++) if (!strcmp(stub_names[i], name)) return (u32)(uintptr_t)stub_region + 4 * i;
    for (u32 i = 0; i < IMPTAB_N; i++) if (!strcmp(IMPTAB[i].name, name)) return make_stub(IMPTAB[i].name, IMPTAB[i].fn);
    fprintf(stderr, "!! no shim entry for import %s\n", name); exit(2);
}

#ifndef COMPARE_MODE
#include "threads.c"
#include "libc_files.c"
#endif
static u32 init_fns[1024], n_init, onload_addr;
static void load_image(const char* path) {
    FILE* f = fopen(path, "rb"); if (!f) { perror(path); exit(2); }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    u8* file = malloc(sz); if (fread(file, 1, sz, f) != (size_t)sz) exit(2); fclose(f);
    Ehdr* eh = (Ehdr*)file; if (memcmp(file, "\x7f" "ELF", 4) || eh->machine != 40) { fprintf(stderr, "not an ARM ELF\n"); exit(2); }
    Phdr* ph = (Phdr*)(file + eh->phoff); u32 top = 0; Phdr* dynp = 0;
    for (int i = 0; i < eh->phnum; i++) { if (ph[i].type == 1 && ph[i].vaddr + ph[i].memsz > top) top = ph[i].vaddr + ph[i].memsz; if (ph[i].type == 2) dynp = &ph[i]; }
    top = (top + 0xFFF) & ~0xFFFu; g_top = top;
    u8* img = mmap(NULL, top, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (img == MAP_FAILED) { perror("mmap image"); exit(2); }
    g_base = U(img);
    for (int i = 0; i < eh->phnum; i++) if (ph[i].type == 1) memcpy(img + ph[i].vaddr, file + ph[i].offset, ph[i].filesz);
    stub_region = mmap(NULL, STUB_SLOTS * 4, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    dispatch = calloc(top / 4, sizeof(fn_t));
    for (u32 i = 0; i < FNTAB_N; i++) dispatch[FNTAB[i].addr >> 2] = FNTAB[i].fn;

    u32 dyn[64][2]; int nd = 0; Dyn* d = (Dyn*)(file + dynp->offset);
    u32 tab[40] = {0}; (void)dyn; (void)nd;
    for (; d->tag; d++) if (d->tag < 40) tab[d->tag] = d->val;
    Sym* symtab = (Sym*)(img + tab[6]); const char* strtab = (const char*)(img + tab[5]); u32 nsyms = ((u32*)(img + tab[4]))[1];
    u32 tabs[2][2] = {{tab[17], tab[18]}, {tab[23], tab[2]}};
    for (int k = 0; k < 2; k++) {
        if (!tabs[k][0]) continue;
        Rel* r = (Rel*)(img + tabs[k][0]);
        for (u32 i = 0; i < tabs[k][1] / 8; i++) {
            u32 type = r[i].info & 0xff, si = r[i].info >> 8; u32* where = (u32*)(img + r[i].offset);
            if (type == 23) { *where += g_base; continue; }
            Sym* s = &symtab[si]; u32 S;
            if (s->shndx) S = g_base + s->value; else S = import_addr(strtab + s->name);
            if (type == 21 || type == 22) *where = S; else if (type == 2) *where += S;
            else fprintf(stderr, "!! unhandled reloc %u\n", type);
        }
    }
    for (u32 i = 0; i < nsyms; i++) if (symtab[i].shndx && symtab[i].name) {
        const char* nm = strtab + symtab[i].name;
        if (!strcmp(nm, "JNI_OnLoad")) onload_addr = g_base + symtab[i].value;
        if (!strncmp(nm, "Java_", 5) && g_exports_n < 2048) { g_exports[g_exports_n].name = strdup(nm); g_exports[g_exports_n++].addr = g_base + symtab[i].value; }
    }
    if (tab[25]) for (u32 i = 0; i < tab[27] / 4 && n_init < 1024; i++) { u32 fn = ((u32*)(img + tab[25]))[i]; if (fn && fn != 0xffffffffu) init_fns[n_init++] = fn; }
    free(file);
}

#ifndef _WIN32
static void segv_handler(int sig, siginfo_t* si, void* uc_) {
    ucontext_t* uc = uc_; void* bt[40];
    fprintf(stderr, "\n!! signal %d at pc=%#x, fault address %p\n", sig, (u32)uc->uc_mcontext.gregs[REG_EIP], si->si_addr);
    int n = backtrace(bt, 40);
    fprintf(stderr, "backtrace (use nm -n dspc_native to map):");
    for (int i = 0; i < n; i++) fprintf(stderr, " %p", bt[i]);
    fprintf(stderr, "\nguest image base %#x\n", g_base); fflush(stdout); fflush(stderr); if (trace_f) fflush(trace_f); _exit(139);
}
#endif
static u32 g_sp0_for_input, g_id_down, g_id_move, g_id_up, g_id_touchscreen; static int g_ids_ready;
static u32 call_static_int(const char* name, u32 sp) { u32 a = find_export(name); return a ? (u32)call_ind(a, U(g_env_obj), 0, 0, 0, sp) : 0; }
static void host_pointer(int kind, float x, float y) {
    if (!g_ids_ready) {
        g_id_down = call_static_int("Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerDown", g_sp0_for_input);
        g_id_move = call_static_int("Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerMove", g_sp0_for_input);
        g_id_up = call_static_int("Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerUp", g_sp0_for_input);
        g_id_touchscreen = call_static_int("Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdTouchScreen", g_sp0_for_input);
        g_ids_ready = 1;
    }
    u32 addr = find_export("Java_com_ea_blast_TouchSurfaceAndroid_NativeOnPointerEvent"); if (!addr) return;
    JArg a[5]; memset(a, 0, sizeof a);
    a[0].i = kind == 0 ? g_id_down : kind == 1 ? g_id_move : g_id_up; a[1].i = g_id_touchscreen; a[2].i = 0; a[3].f = x; a[4].f = y;
    call_java_native(addr, class_handle("com/ea/blast/TouchSurfaceAndroid"), "IIIFF", 5, a, g_sp0_for_input);
}
static volatile u32 g_frame_counter;
#ifndef _WIN32
static void usr2_handler(int s) {
    void* bt[40]; int n = backtrace(bt, 40);
    fprintf(stderr, "THREAD %ld backtrace:", (long)syscall(SYS_gettid));
    for (int i = 0; i < n; i++) fprintf(stderr, " %p", bt[i]);
    fprintf(stderr, "\n");
}
static void* watchdog(void* a) {                     // DS_WATCHDOG=<seconds>: dump all threads if no frame completes in that time
    int secs = (int)(intptr_t)a; u32 last = (u32)-1;
    for (;;) {
        sleep(secs);
        if (g_frame_counter == last) {
            fprintf(stderr, "\n!! watchdog: no frame for %d s (frame %u). Thread stacks:\n", secs, g_frame_counter);
            DIR* d = opendir("/proc/self/task"); struct dirent* e;
            while (d && (e = readdir(d))) if (e->d_name[0] != '.') syscall(SYS_tgkill, getpid(), atoi(e->d_name), SIGUSR2);
            if (d) closedir(d);
            sleep(1); fprintf(stderr, "guest image base %#x\n", g_base); fflush(stdout); fflush(stderr); _exit(124);
        }
        last = g_frame_counter;
    }
    return 0;
}
// keyboard / gamepad keys use the Android key-code protocol the game was built for (KeyboardAndroid.NativeOnKeyDown/Up).
// The Xperia Play mapping (KeyboardAndroidXperiaPlay::RawToStdKey): 23=Cross 99=Square 100=Triangle, 4(+alt)=Circle, 19-22=D-pad.
static void host_key(int down, int keycode, int alt) {
    static u32 module_id; static int have;
    if (!have) { module_id = call_static_int("Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdPhysicalKeyboard", g_sp0_for_input); have = 1; }
    u32 addr = find_export(down ? "Java_com_ea_blast_KeyboardAndroid_NativeOnKeyDown" : "Java_com_ea_blast_KeyboardAndroid_NativeOnKeyUp");
    if (!addr) return;
    JArg a[3]; memset(a, 0, sizeof a); a[0].i = module_id; a[1].i = (u32)keycode; a[2].i = (u32)alt;
    static u32 kb_obj; if (!kb_obj) kb_obj = new_obj(K_OBJECT, "com/ea/blast/PhysicalKeyboardAndroid", 0, 0);
    call_java_native(addr, kb_obj, "III", 3, a, g_sp0_for_input);
}
typedef struct { const char* so; } Args;
#endif
static void* run(void* p) {
    Args* a = p;
    load_image(a->so);
    build_jni();
    void* guest_stack = mmap(NULL, 0x1000000, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (guest_stack == MAP_FAILED) {
        fprintf(stderr, "!! Unable to allocate 16MB guest x86 stack\n");
        exit(2);
    }
    u32 sp0 = U(guest_stack) + 0x1000000 - 0x100;
    g_sp0 = sp0; g_sp0_for_input = sp0 - 0x40000;
    printf("loaded %s at %#x (%u KB), %u translated functions, %u imports\n", a->so, g_base, g_top / 1024, FNTAB_N, IMPTAB_N);
    u32 done = 0;
    for (u32 i = 0; i < n_init; i++) { call_ind(init_fns[i], 0, 0, 0, 0, sp0); done++; }
    printf("%u/%u initializers completed\n", done, n_init);
    u32 v = (u32)call_ind(onload_addr, U(g_vm_obj), 0, 0, 0, sp0);
    printf("JNI_OnLoad returned %#x\n", v);
    printf("registered natives (RegisterNatives): %d\n", g_natives);
    const char* steps = getenv("DS_STEPS") ? getenv("DS_STEPS") : "";
    if (getenv("DS_ASSETS")) g_assets_root = getenv("DS_ASSETS");
    fprintf(stderr, "[RESOURCE_ROOTS] apk_assets=%s save_directory=%s\n",
            g_assets_root, getenv("DS_DATA") ? getenv("DS_DATA") : "userdata");
    g_activity = new_obj(K_OBJECT, "com/ea/blast/MainActivity", 0, 0);
    g_assetmgr = new_obj(K_ASSETMGR, "android/content/res/AssetManager", 0, 0);
    u32 renderer = new_obj(K_OBJECT, "com/ea/blast/AndroidRenderer", 0, 0);
    #define CALLX(name, a1, a2, a3) do { u32 ad = find_export(name); if (!ad) printf("no export %s\n", name); else { printf(">> %s\n", name); fflush(stdout); \
        u32 rr = (u32)call_ind(ad, U(g_env_obj), a1, a2, a3, sp0); printf("<< %s returned %#x\n", name, rr); fflush(stdout); } } while (0)
    int have_window = 0; (void)have_window;
#ifndef COMPARE_MODE
    if (getenv("DS_GLLOG")) gl_install_log_backend(fopen(getenv("DS_GLLOG"), "w"));
#ifdef DS_SDL
    else if (getenv("DS_WINDOW")) {
        int ww = getenv("DS_WIDTH") ? atoi(getenv("DS_WIDTH")) : 1280, wh = getenv("DS_HEIGHT") ? atoi(getenv("DS_HEIGHT")) : 720;
        have_window = sdl_open(ww, wh);
    }
#endif
#endif
    if (strstr(steps, "create")) CALLX("Java_com_ea_blast_MainActivity_NativeOnCreate", g_activity, 0, 0);
    int sw = 1280, sh = 720;
#ifdef DS_SDL
    if (have_window) { sw = g_w; sh = g_h; }
#endif
    if (strstr(steps, "surface")) { CALLX("Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceCreated", renderer, 0, 0);
                                    CALLX("Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceChanged", renderer, sw, sh); }
    if (strstr(steps, "frame")) {
        int nf = getenv("DS_FRAMES") ? atoi(getenv("DS_FRAMES")) : 1;
        int shot_at = -1; const char* shot_path = 0;
        if (getenv("DS_SHOT")) { shot_at = atoi(getenv("DS_SHOT")); shot_path = strchr(getenv("DS_SHOT"), ':') ? strchr(getenv("DS_SHOT"), ':') + 1 : "shot.ppm"; }
        u32 ad = find_export("Java_com_ea_blast_AndroidRenderer_NativeOnDrawFrame");
        for (int i = 0; nf <= 0 ? have_window : i < nf; i++) {
#ifdef DS_SDL
            if (have_window) {
                int resized; if (!sdl_pump(&resized)) break;
                if (resized) call_ind(find_export("Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceChanged"), U(g_env_obj), renderer, g_w, g_h, sp0);
            }
#endif
            { const char* ks = getenv("DS_KEYS");            // "frame:keycode:alt,..." scripted key presses (down at frame, up two frames later)
              while (ks && *ks) { int f, k, alt, n = 0; if (sscanf(ks, "%d:%d:%d%n", &f, &k, &alt, &n) < 3) break;
                if (i + 1 == f) host_key(1, k, alt); if (i + 1 == f + 2) host_key(0, k, alt);
                ks += n; if (*ks == ',') ks++; } }
            { const char* cl = getenv("DS_CLICKS");
              while (cl && *cl) { int f, x, y, n = 0; if (sscanf(cl, "%d:%d:%d%n", &f, &x, &y, &n) < 3) break;
                if (i + 1 == f) host_pointer(0, (float)x, (float)y); if (i + 1 == f + 2) host_pointer(2, (float)x, (float)y);
                cl += n; if (*cl == ',') cl++; } }
            call_ind(ad, U(g_env_obj), renderer, 0, 0, sp0); g_frame_counter++;
#ifdef DS_SDL
            if (have_window) {
                if (i + 1 == shot_at) { sdl_shot(shot_path); printf("saved %s (frame %d)\n", shot_path, i + 1); }
                if (getenv("DS_SHOT_EVERY") && (i + 1) % atoi(getenv("DS_SHOT_EVERY")) == 0) { char pth[256]; snprintf(pth, sizeof pth, "%s/f%05d.ppm", getenv("DS_SHOT_DIR") ? getenv("DS_SHOT_DIR") : "/tmp", i + 1); sdl_shot(pth); }
                sdl_present(); }
#endif
        }
        printf("ran frames\n");
    }
    return 0;
}

int main(int argc, char** argv) {
    const char* so = argc > 1 ? argv[1] : "libDeadSpace.so";
    setvbuf(stdout, NULL, _IONBF, 0);
    if (getenv("TRACE")) trace_f = fopen(getenv("TRACE"), "w");
#ifdef _WIN32
    SetUnhandledExceptionFilter(ds_windows_crash_filter);
    if (getenv("DS_WATCHDOG")) fprintf(stderr,
       "[WIN32 HOST] DS_WATCHDOG Linux stack enumerator not available; "
       "Windows unhandled exception report is enabled.\n");
#else
    { struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_sigaction = segv_handler; sa.sa_flags = SA_SIGINFO;
      sigaction(SIGSEGV, &sa, 0); sigaction(SIGBUS, &sa, 0); sigaction(SIGFPE, &sa, 0); sigaction(SIGILL, &sa, 0); sigaction(SIGABRT, &sa, 0); }
    if (getenv("DS_WATCHDOG")) {
        struct sigaction su; memset(&su, 0, sizeof su); su.sa_handler = usr2_handler; sigaction(SIGUSR2, &su, 0);
        pthread_t wt; pthread_create(&wt, 0, watchdog, (void*)(intptr_t)atoi(getenv("DS_WATCHDOG")));
    }
#endif
    pthread_attr_t at; pthread_attr_init(&at); pthread_attr_setstacksize(&at, 256u << 20);
    pthread_t th; Args a = {so};
    pthread_create(&th, &at, run, &a); pthread_join(th, 0);
    if (trace_f) fclose(trace_f);
    return 0;
}
