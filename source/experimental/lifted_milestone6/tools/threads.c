// threads.c - real threads and synchronization for the translated game (included by host.c, not in COMPARE_MODE).
// Guest pthread objects are only a few bytes, so guest address -> host object tables stand in for them.
#include <semaphore.h>
#include <errno.h>
#include <sched.h>

enum { SK_MUTEX = 1, SK_COND = 2, SK_SEM = 3, SK_TOMB = 255 };
typedef struct { u32 addr; u8 kind; void* obj; } SSlot;
#define NSLOTS 16384
static SSlot sslots[NSLOTS];
static pthread_mutex_t sslock = PTHREAD_MUTEX_INITIALIZER;

static void* sobj_get(u32 addr, int kind, int create) {
    pthread_mutex_lock(&sslock);
    u32 i = ((addr >> 2) * 2654435761u) >> 18, tomb = ~0u; void* res = 0;
    for (u32 n = 0; n < NSLOTS; n++, i = (i + 1) & (NSLOTS - 1)) {
        SSlot* s = &sslots[i];
        if (s->kind == 0) { if (tomb != ~0u) i = tomb; break; }
        if (s->kind == SK_TOMB) { if (tomb == ~0u) tomb = i; continue; }
        if (s->addr == addr && s->kind == kind) { res = s->obj; pthread_mutex_unlock(&sslock); return res; }
    }
    if (create) {
        void* o;
        if (kind == SK_MUTEX) { pthread_mutexattr_t at; pthread_mutexattr_init(&at); pthread_mutexattr_settype(&at, PTHREAD_MUTEX_RECURSIVE);
                                o = malloc(sizeof(pthread_mutex_t)); pthread_mutex_init(o, &at); }
        else if (kind == SK_COND) { o = malloc(sizeof(pthread_cond_t)); pthread_cond_init(o, 0); }
        else { o = malloc(sizeof(sem_t)); sem_init(o, 0, 0); }
        sslots[i].addr = addr; sslots[i].kind = (u8)kind; sslots[i].obj = o; res = o;
    }
    pthread_mutex_unlock(&sslock); return res;
}
static void* sobj_remove(u32 addr, int kind) {
    pthread_mutex_lock(&sslock);
    u32 i = ((addr >> 2) * 2654435761u) >> 18; void* res = 0;
    for (u32 n = 0; n < NSLOTS; n++, i = (i + 1) & (NSLOTS - 1)) {
        SSlot* s = &sslots[i];
        if (s->kind == 0) break;
        if (s->kind == kind && s->addr == addr) { res = s->obj; s->kind = SK_TOMB; break; }
    }
    pthread_mutex_unlock(&sslock); return res;
}

IMP(pthread_mutex_init)    { T(pthread_mutex_init); void* o = sobj_remove(r0, SK_MUTEX); free(o); sobj_get(r0, SK_MUTEX, 1); RET(0); }
IMP(pthread_mutex_lock)    { T(pthread_mutex_lock); RET(pthread_mutex_lock(sobj_get(r0, SK_MUTEX, 1))); }
IMP(pthread_mutex_unlock)  { T(pthread_mutex_unlock); RET(pthread_mutex_unlock(sobj_get(r0, SK_MUTEX, 1))); }
IMP(pthread_mutex_trylock) { T(pthread_mutex_trylock); RET(pthread_mutex_trylock(sobj_get(r0, SK_MUTEX, 1))); }
IMP(pthread_mutex_destroy) { T(pthread_mutex_destroy); void* o = sobj_remove(r0, SK_MUTEX); if (o) { pthread_mutex_destroy(o); free(o); } RET(0); }
IMP(pthread_mutexattr_init)       { T(pthread_mutexattr_init); *(u32*)P(r0) = 0; RET(0); }
IMP(pthread_mutexattr_destroy)    { T(pthread_mutexattr_destroy); RET(0); }
IMP(pthread_mutexattr_settype)    { T(pthread_mutexattr_settype); *(u32*)P(r0) = r1; RET(0); }
IMP(pthread_mutexattr_setpshared) { T(pthread_mutexattr_setpshared); RET(0); }

IMP(pthread_cond_init)      { T(pthread_cond_init); void* o = sobj_remove(r0, SK_COND); free(o); sobj_get(r0, SK_COND, 1); RET(0); }
IMP(pthread_cond_destroy)   { T(pthread_cond_destroy); void* o = sobj_remove(r0, SK_COND); if (o) { pthread_cond_destroy(o); free(o); } RET(0); }
IMP(pthread_cond_signal)    { T(pthread_cond_signal); RET(pthread_cond_signal(sobj_get(r0, SK_COND, 1))); }
IMP(pthread_cond_broadcast) { T(pthread_cond_broadcast); RET(pthread_cond_broadcast(sobj_get(r0, SK_COND, 1))); }
IMP(pthread_cond_wait)      { T(pthread_cond_wait); RET(pthread_cond_wait(sobj_get(r0, SK_COND, 1), sobj_get(r1, SK_MUTEX, 1))); }
IMP(pthread_cond_timedwait) {
    T(pthread_cond_timedwait);
    struct timespec ts; ts.tv_sec = (time_t)((u32*)P(r2))[0]; ts.tv_nsec = (long)((u32*)P(r2))[1];
    RET(pthread_cond_timedwait(sobj_get(r0, SK_COND, 1), sobj_get(r1, SK_MUTEX, 1), &ts));
}

IMP(sem_init)    { T(sem_init); void* o = sobj_remove(r0, SK_SEM); free(o); sem_t* s = sobj_get(r0, SK_SEM, 1); sem_init(s, 0, r2); RET(0); }
IMP(sem_destroy) { T(sem_destroy); void* o = sobj_remove(r0, SK_SEM); if (o) { sem_destroy(o); free(o); } RET(0); }
IMP(sem_post)    { T(sem_post); RET(sem_post(sobj_get(r0, SK_SEM, 1))); }
IMP(sem_trywait) { T(sem_trywait); int rc = sem_trywait(sobj_get(r0, SK_SEM, 1)); RET(rc ? -1 : 0); }
IMP(sem_wait)    { T(sem_wait); sem_t* s = sobj_get(r0, SK_SEM, 1); int rc; while ((rc = sem_wait(s)) && errno == EINTR) {} RET(rc ? -1 : 0); }
IMP(sem_getvalue){ T(sem_getvalue); int v = 0; sem_getvalue(sobj_get(r0, SK_SEM, 1), &v); *(s32*)P(r1) = v; RET(0); }
IMP(sem_timedwait) {
    T(sem_timedwait); struct timespec ts; ts.tv_sec = (time_t)((u32*)P(r1))[0]; ts.tv_nsec = (long)((u32*)P(r1))[1];
    sem_t* s = sobj_get(r0, SK_SEM, 1); int rc; while ((rc = sem_timedwait(s, &ts)) && errno == EINTR) {} RET(rc ? -1 : 0);
}

// ---- threads: every guest thread gets its own guest stack; the host thread runs the translated entry function
typedef struct { u32 start, arg, stack; } ThrStart;
static void* thread_entry(void* p) {
    ThrStart t = *(ThrStart*)p; free(p);
    u32 sz = t.stack >= 0x10000 ? t.stack : (1u << 20);
    u8* st = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    u32 sp = (U(st) + sz - 0x200) & ~7u;
    u64 r = call_ind(t.start, t.arg, 0, 0, 0, sp);
    munmap(st, sz);
    return (void*)(uintptr_t)(u32)r;
}
IMP(pthread_create) {
    T(pthread_create);
    u32* at = r1 ? (u32*)P(r1) : 0;
    ThrStart* t = malloc(sizeof *t); t->start = r2; t->arg = r3; t->stack = at ? at[2] : 0;
    pthread_attr_t ha; pthread_attr_init(&ha); pthread_attr_setstacksize(&ha, 16u << 20);
    if (at && (at[0] & 1)) pthread_attr_setdetachstate(&ha, PTHREAD_CREATE_DETACHED);
    pthread_t th; int rc = pthread_create(&th, &ha, thread_entry, t);
    if (rc) { free(t); RET(rc); }
    *(u32*)P(r0) = (u32)th; RET(0);
}
IMP(pthread_join)   { T(pthread_join); void* ret = 0; int rc = pthread_join((pthread_t)r0, &ret); if (r1) *(u32*)P(r1) = U(ret); RET(rc); }
IMP(pthread_detach) { T(pthread_detach); RET(pthread_detach((pthread_t)r0)); }
IMP(pthread_exit)   { T(pthread_exit); pthread_exit(P(r0)); return 0; }
IMP(pthread_self)   { T(pthread_self); RET((u32)pthread_self()); }
IMP(pthread_equal)  { T(pthread_equal); RET(r0 == r1); }
IMP(sched_yield)    { T(sched_yield); sched_yield(); RET(0); }

IMP(pthread_attr_init)           { T(pthread_attr_init); u32* a = P(r0); memset(a, 0, 24); a[2] = 1u << 20; RET(0); }
IMP(pthread_attr_destroy)        { T(pthread_attr_destroy); RET(0); }
IMP(pthread_attr_setstacksize)   { T(pthread_attr_setstacksize); ((u32*)P(r0))[2] = r1; RET(0); }
IMP(pthread_attr_setdetachstate) { T(pthread_attr_setdetachstate); u32* a = P(r0); if (r1) a[0] |= 1; else a[0] &= ~1u; RET(0); }
IMP(pthread_attr_setschedparam)  { T(pthread_attr_setschedparam); RET(0); }
IMP(pthread_setschedparam)       { T(pthread_setschedparam); RET(0); }

IMP(pthread_key_create)  { T(pthread_key_create); pthread_key_t k; int rc = pthread_key_create(&k, 0); *(u32*)P(r0) = (u32)k; RET(rc); }
IMP(pthread_key_delete)  { T(pthread_key_delete); RET(pthread_key_delete((pthread_key_t)r0)); }
IMP(pthread_getspecific) { T(pthread_getspecific); RET(U(pthread_getspecific((pthread_key_t)r0))); }
IMP(pthread_setspecific) { T(pthread_setspecific); RET(pthread_setspecific((pthread_key_t)r0, P(r1))); }
static pthread_mutex_t once_lock = PTHREAD_MUTEX_INITIALIZER;
IMP(pthread_once) {
    T(pthread_once);
    static pthread_mutexattr_t at; static int inited;
    pthread_mutex_lock(&sslock); if (!inited) { pthread_mutexattr_init(&at); pthread_mutexattr_settype(&at, PTHREAD_MUTEX_RECURSIVE); pthread_mutex_init(&once_lock, &at); inited = 1; } pthread_mutex_unlock(&sslock);
    pthread_mutex_lock(&once_lock);
    if (*(volatile u32*)P(r0) == 0) { call_ind(r1, 0, 0, 0, 0, sp); *(volatile u32*)P(r0) = 1; }
    pthread_mutex_unlock(&once_lock); RET(0);
}
IMP(nanosleep) {
    T(nanosleep); struct timespec ts; ts.tv_sec = (time_t)((u32*)P(r0))[0]; ts.tv_nsec = (long)((u32*)P(r0))[1];
    struct timespec rem; int rc = nanosleep(&ts, &rem);
    if (rc && r1) { ((u32*)P(r1))[0] = (u32)rem.tv_sec; ((u32*)P(r1))[1] = (u32)rem.tv_nsec; }
    RET(rc);
}
IMP(longjmp) { T(longjmp); longjmp(*(jmp_buf*)P(r0), r1 ? (int)r1 : 1); return 0; }
