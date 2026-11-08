/* Native Win32/x86 substrate for the AOT lifted Dead Space Mobile host.
 * Copyright: new independently written portability code, no EA assets.
 */
#include "win32_substrate.h"
#include <limits.h>
#include <process.h>
#include <stdlib.h>

/* VirtualAlloc commits demand-zero pages lazily. This is suitable for the
 * existing host's anonymous guest image, 16MB guest stacks and sbrk heap.
 * The Win32 port will call this instead of Linux mmap(MAP_ANONYMOUS).
 * No fixed-address mappings are provided or silently emulated. */
void* ds_win32_map_anonymous(size_t length) {
    if (!length) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    return VirtualAlloc(NULL, length, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

int ds_win32_unmap(void* base) {
    if (!base) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }
    return VirtualFree(base, 0, MEM_RELEASE) ? 0 : -1;
}

static DWORD ds_page_protection(int protection) {
    const int r = (protection & DS_WIN32_PROT_READ) != 0;
    const int w = (protection & DS_WIN32_PROT_WRITE) != 0;
    const int x = (protection & DS_WIN32_PROT_EXEC) != 0;
    if (w && x) return PAGE_EXECUTE_READWRITE;
    if (x && r) return PAGE_EXECUTE_READ;
    if (x) return PAGE_EXECUTE;
    if (w) return PAGE_READWRITE; /* Win32 has no write-only pages. */
    if (r) return PAGE_READONLY;
    return PAGE_NOACCESS;
}
int ds_win32_protect(void* base, size_t length, int protection) {
    DWORD old = 0;
    if (!base || !length || (protection & ~7)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }
    return VirtualProtect(base, length, ds_page_protection(protection), &old)
           ? 0 : -1;
}
unsigned long ds_win32_page_size(void) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (unsigned long)info.dwPageSize;
}

int ds_win32_mutex_init(ds_win32_mutex* mutex) {
    if (!mutex) return -1;
    InitializeCriticalSection(&mutex->handle);
    return 0;
}
void ds_win32_mutex_destroy(ds_win32_mutex* mutex) {
    if (mutex) DeleteCriticalSection(&mutex->handle);
}
void ds_win32_mutex_lock(ds_win32_mutex* mutex) {
    EnterCriticalSection(&mutex->handle);
}
void ds_win32_mutex_unlock(ds_win32_mutex* mutex) {
    LeaveCriticalSection(&mutex->handle);
}

void ds_win32_cond_init(ds_win32_cond* cond) {
    InitializeConditionVariable(&cond->handle);
}
void ds_win32_cond_signal(ds_win32_cond* cond) {
    WakeConditionVariable(&cond->handle);
}
void ds_win32_cond_broadcast(ds_win32_cond* cond) {
    WakeAllConditionVariable(&cond->handle);
}
int ds_win32_cond_wait(ds_win32_cond* cond, ds_win32_mutex* mutex,
                       unsigned long timeout_ms) {
    if (SleepConditionVariableCS(&cond->handle, &mutex->handle,
                                 (DWORD)timeout_ms)) return 0;
    return GetLastError() == ERROR_TIMEOUT ? 1 : -1;
}

int ds_win32_sem_init(ds_win32_sem* sem, long initial, long maximum) {
    if (!sem || initial < 0 || maximum < 1 || initial > maximum) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }
    sem->handle = CreateSemaphoreA(NULL, initial, maximum, NULL);
    return sem->handle ? 0 : -1;
}
void ds_win32_sem_destroy(ds_win32_sem* sem) {
    if (sem && sem->handle) {
        CloseHandle(sem->handle);
        sem->handle = NULL;
    }
}
int ds_win32_sem_post(ds_win32_sem* sem) {
    return ReleaseSemaphore(sem->handle, 1, NULL) ? 0 : -1;
}
int ds_win32_sem_wait(ds_win32_sem* sem, unsigned long timeout_ms) {
    DWORD rc = WaitForSingleObject(sem->handle, (DWORD)timeout_ms);
    return rc == WAIT_OBJECT_0 ? 0 : rc == WAIT_TIMEOUT ? 1 : -1;
}

/* Explicit Windows TLS keys are used for dynamically-created guest TLS.
 * Compiler-managed __declspec(thread) handles fixed per-thread variables
 * such as the guest's JNI exception and emulated Bionic errno state. */
int ds_win32_tls_init(ds_win32_tls* key) {
    if (!key) return -1;
    key->key = TlsAlloc();
    return key->key == TLS_OUT_OF_INDEXES ? -1 : 0;
}
void ds_win32_tls_destroy(ds_win32_tls* key) {
    if (key && key->key != TLS_OUT_OF_INDEXES) {
        TlsFree(key->key);
        key->key = TLS_OUT_OF_INDEXES;
    }
}
int ds_win32_tls_set(ds_win32_tls* key, void* value) {
    return TlsSetValue(key->key, value) ? 0 : -1;
}
void* ds_win32_tls_get(const ds_win32_tls* key) {
    return TlsGetValue(key->key);
}

typedef struct ds_worker_start {
    ds_win32_thread_fn function;
    void* argument;
} ds_worker_start;
static unsigned __stdcall ds_worker_entry(void* user) {
    ds_worker_start* start = (ds_worker_start*)user;
    ds_win32_thread_fn fn = start->function;
    void* arg = start->argument;
    free(start);
    /* This host is deliberately 32-bit; return pointer fits DWORD. */
    return (unsigned)(uintptr_t)fn(arg);
}
int ds_win32_thread_start(ds_win32_thread* thread,
                          ds_win32_thread_fn function, void* argument) {
    ds_worker_start* task;
    uintptr_t h;
    if (!thread || !function) return -1;
    thread->handle = NULL;
    task = (ds_worker_start*)malloc(sizeof(*task));
    if (!task) return -1;
    task->function = function;
    task->argument = argument;
    h = _beginthreadex(NULL, 0, ds_worker_entry, task, 0, NULL);
    if (!h) {
        free(task);
        return -1;
    }
    thread->handle = (HANDLE)h;
    return 0;
}
int ds_win32_thread_join(ds_win32_thread* thread, uintptr_t* result) {
    DWORD exit_code = 0;
    DWORD rc;
    if (!thread || !thread->handle) return -1;
    rc = WaitForSingleObject(thread->handle, INFINITE);
    if (rc != WAIT_OBJECT_0 ||
        !GetExitCodeThread(thread->handle, &exit_code)) return -1;
    CloseHandle(thread->handle);
    thread->handle = NULL;
    if (result) *result = (uintptr_t)exit_code;
    return 0;
}
