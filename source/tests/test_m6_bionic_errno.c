// Asset-free execution test of the actual milestone6 file shims on i386 Linux.
// No original executable, extracted assets, or generated lifted ARM C required.
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../experimental/lifted_milestone6/tools/rt.h"

#define IMP(n) u64 imp_##n(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp)
#define T(n) ((void)0)
#define RET(v) return (u64)(u32)(v)
#define P(x) ((void*)(uintptr_t)(x))
#define U(p) ((u32)(uintptr_t)(p))
typedef struct { const u32* regs; int nregs, ri; const u32* mem; } AR;
static __thread int g_errno;
static u32 data_object(const char* name) { (void)name; return 0u; }
static int gformat(char* out, size_t size, const char* fmt, AR* a) {
    (void)out; (void)size; (void)fmt; (void)a; return 0;
}
#include "../experimental/lifted_milestone6/tools/libc_files.c"

static const char* missing = "/dead_space_resource_probe/does-not-exist.m3g";

static void* worker(void* arg) {
    (void)arg;
    g_errno = 2718;
    assert((u32)imp_open(U(missing), O_RDONLY, 0, 0, 0) == UINT32_MAX);
    assert(g_errno == ENOENT);
    return 0;
}

int main(void) {
    // Missing native resource must set Bionic guest errno, not just host errno.
    g_errno=0;
    assert((u32)imp_open(U(missing), O_RDONLY, 0, 0, 0) == UINT32_MAX);
    assert(g_errno==ENOENT);
    g_errno=0;
    assert((u32)imp_fopen(U(missing), U("rb"), 0, 0, 0) == 0);
    assert(g_errno==ENOENT);
    g_errno=0;
    assert((u32)imp_opendir(U(missing), 0, 0, 0, 0) == 0);
    assert(g_errno==ENOENT);
    g_errno=0;
    assert((u32)imp_stat(U(missing), 0, 0, 0, 0) == UINT32_MAX);
    assert(g_errno==ENOENT);
    g_errno=0;
    assert((u32)imp_read((u32)-1, 0, 0, 0, 0) == UINT32_MAX);
    assert(g_errno==EBADF);

    // Successful native calls must not erase a previous guest errno.
    g_errno=314;
    u32 fd = (u32)imp_open(U("/dev/null"), O_RDONLY, 0, 0, 0);
    assert((int)fd >= 0);
    assert(g_errno==314);
    assert((u32)imp_close(fd, 0, 0, 0, 0) == 0);
    assert(g_errno==314);

    // Guest errno is thread-local, just as Android/Bionic expects.
    pthread_t tid;
    g_errno=54321;
    assert(pthread_create(&tid, 0, worker, 0) == 0);
    assert(pthread_join(tid, 0) == 0);
    assert(g_errno==54321);

    puts("PASS Bionic file I/O errno on i686: missing/valid files and per-thread state");
    return 0;
}
