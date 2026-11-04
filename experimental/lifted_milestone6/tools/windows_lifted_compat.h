/* Native Windows x86 glue for the ACTUAL lifted host.
 * This only adapts OS services; it does not emulate ARM instructions.
 * Original guest call ABI and ELF data loader remain in host.c.
 */
#pragma once
#if !defined(_WIN32) || defined(_WIN64)
#error windows_lifted_compat.h requires native 32-bit Windows x86
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <process.h>
#include <pthread.h>
#include <semaphore.h>
#include <io.h>
#include <direct.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "win32_substrate.h"

#ifndef PROT_NONE
#define PROT_NONE 0
#endif
#ifndef PROT_READ
#define PROT_READ 1
#endif
#ifndef PROT_WRITE
#define PROT_WRITE 2
#endif
#ifndef MAP_PRIVATE
#define MAP_PRIVATE 2
#endif
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif
#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0x4000
#endif
#ifndef MAP_FAILED
#define MAP_FAILED ((void*)-1)
#endif

static inline void* ds_win32_guest_mmap(void* hint, size_t len, int prot,
                                         int flags, int fd, size_t off) {
    void* p;
    (void)hint; (void)prot; (void)off;
    /* This lifted host only needs anonymous guest image/heap/stack and
     * reservation for fake import stub addresses; no file-backed mappings. */
    if (!(flags & MAP_ANONYMOUS) || fd != -1)
        return MAP_FAILED;
    p=ds_win32_map_anonymous(len);
    if (p && !(prot & (PROT_READ|PROT_WRITE)))
        if (ds_win32_protect(p,len,0)) {
            ds_win32_unmap(p);
            return MAP_FAILED;
        }
    return p ? p : MAP_FAILED;
}
static inline int ds_win32_guest_munmap(void* addr, size_t len) {
    (void)len; /* The host only unmaps whole virtual allocations. */
    return ds_win32_unmap(addr);
}
#define mmap ds_win32_guest_mmap
#define munmap ds_win32_guest_munmap

static inline int ds_win32_guest_mkdir(const char* p, int mode) {
    (void)mode;
    return _mkdir(p);
}
#define mkdir ds_win32_guest_mkdir

static inline char* ds_win32_guest_realpath(const char* src, char* dst) {
    return _fullpath(dst,src,4096);
}
#define realpath ds_win32_guest_realpath

/* Windows uses MSVCRT file descriptors; explicit binary mode is essential
 * for M3G texture data and serialized game resources. */
#ifndef O_BINARY
#define O_BINARY _O_BINARY
#endif

/* Guest Bionic open flags originate on ARM. The standard open shim already
 * forwards only these flags. O_BINARY must be added on the host side. */

static inline unsigned long ds_win32_guest_tid(void) {
    return (unsigned long)GetCurrentThreadId();
}

static LONG WINAPI ds_windows_crash_filter(EXCEPTION_POINTERS* ep) {
    fprintf(stderr,
      "\n!! Windows SEH exception %#lx pc=%p guest x86 host\n",
      (unsigned long)ep->ExceptionRecord->ExceptionCode,
      ep->ExceptionRecord->ExceptionAddress);
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION
        && ep->ExceptionRecord->NumberParameters >= 2)
        fprintf(stderr, "   access kind=%lu fault address=%p\n",
          (unsigned long)ep->ExceptionRecord->ExceptionInformation[0],
          (void*)(uintptr_t)ep->ExceptionRecord->ExceptionInformation[1]);
    fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}
