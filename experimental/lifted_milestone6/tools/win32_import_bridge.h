/* Preincluded when compiling extra Win32 guest-import modules separately.
 * These shims use the exact translated game ABI: five u32 args, u64 result.
 * All pointers are guest pointers in the native 32-bit address space.
 */
#pragma once
#ifndef _WIN32
#error Win32 import bridge must not be used for Linux
#endif
#include <winsock2.h>  /* MUST come before windows.h or any header that loads it */
#include <ws2tcpip.h>
#include <pthread.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "protos.h"
#define IMP(n) u64 imp_##n(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp)
#define P(v) ((void*)(uintptr_t)(u32)(v))
#define U(p) ((u32)(uintptr_t)(p))
#define RET(v) return (u64)(u32)(v)
/* Trace is internal to host.c. Do not expose private host state here. */
#define T(n) ((void)0)
/* Guest Bionic errno is TLS-owned by host.c; do not use a separate variable. */
u64 imp___errno(u32,u32,u32,u32,u32);
#define g_errno (*(int*)(uintptr_t)(u32)imp___errno(0,0,0,0,0))
