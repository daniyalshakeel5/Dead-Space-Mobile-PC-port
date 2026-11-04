/* Windows 32-bit process primitives for the Ahead-of-Time lifted ARM host.
 * New implementation: relies on Win32 VirtualAlloc, CRITICAL_SECTION,
 * CONDITION_VARIABLE, TLS, Semaphore and _beginthreadex.
 * No guest ARM execution/emulation, game content, or Linux runtime.
 * This is the process substrate; it is NOT yet a native Windows game build.
 */
#ifndef DS_WIN32_NATIVE_SUBSTRATE_H
#define DS_WIN32_NATIVE_SUBSTRATE_H

#if !defined(_WIN32)
#error This Windows implementation must only be built on Win32.
#endif
#if defined(_WIN64)
#error The original lifted libDeadSpace.so requires x86-32 pointer identity.
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* 4GB guest addresses and host pointers must be exactly the same width. */
typedef char ds_win32_pointer_width_must_be_4[(sizeof(void *) == 4) ? 1 : -1];

enum {
    DS_WIN32_PROT_READ  = 1,
    DS_WIN32_PROT_WRITE = 2,
    DS_WIN32_PROT_EXEC  = 4
};

void* ds_win32_map_anonymous(size_t length);
int ds_win32_unmap(void* base);
int ds_win32_protect(void* base, size_t length, int protection);
unsigned long ds_win32_page_size(void);

/* Underlying Win32 CRITICAL_SECTION is recursive (matches guest Bionic
 * pthread recursive mutexes). These types have stable addresses. */
typedef struct ds_win32_mutex { CRITICAL_SECTION handle; } ds_win32_mutex;
typedef struct ds_win32_cond { CONDITION_VARIABLE handle; } ds_win32_cond;
typedef struct ds_win32_sem { HANDLE handle; } ds_win32_sem;
typedef struct ds_win32_thread { HANDLE handle; } ds_win32_thread;
typedef struct ds_win32_tls { DWORD key; } ds_win32_tls;
typedef void* (*ds_win32_thread_fn)(void*);

int ds_win32_mutex_init(ds_win32_mutex* mutex);
void ds_win32_mutex_destroy(ds_win32_mutex* mutex);
void ds_win32_mutex_lock(ds_win32_mutex* mutex);
void ds_win32_mutex_unlock(ds_win32_mutex* mutex);
void ds_win32_cond_init(ds_win32_cond* cond);
void ds_win32_cond_signal(ds_win32_cond* cond);
void ds_win32_cond_broadcast(ds_win32_cond* cond);
/* Timeout in milliseconds, INFINITE supported. Returns 0 if awakened,
 * 1 on a timeout, -1 on an API error. Caller must hold mutex. */
int ds_win32_cond_wait(ds_win32_cond* cond, ds_win32_mutex* mutex,
                       unsigned long timeout_ms);

int ds_win32_sem_init(ds_win32_sem* sem, long initial, long maximum);
void ds_win32_sem_destroy(ds_win32_sem* sem);
int ds_win32_sem_post(ds_win32_sem* sem);
int ds_win32_sem_wait(ds_win32_sem* sem, unsigned long timeout_ms);

int ds_win32_tls_init(ds_win32_tls* key);
void ds_win32_tls_destroy(ds_win32_tls* key);
int ds_win32_tls_set(ds_win32_tls* key, void* value);
void* ds_win32_tls_get(const ds_win32_tls* key);

int ds_win32_thread_start(ds_win32_thread* thread,
                          ds_win32_thread_fn function, void* argument);
int ds_win32_thread_join(ds_win32_thread* thread, uintptr_t* result);

#if defined(__cplusplus)
}
#endif
#endif
