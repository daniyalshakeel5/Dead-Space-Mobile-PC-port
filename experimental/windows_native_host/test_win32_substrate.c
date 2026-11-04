/* Executable Windows x86 ABI test, no original game code or data.
 * Demonstrates pointer identity, Win32 memory, threads, recursive locks,
 * condition variables, semaphores and per-thread guest TLS. */
#include "win32_substrate.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL at %s:%d: %s (Win32=%lu)\n", \
            __FILE__, __LINE__, #x, GetLastError()); return 2; } \
    } while (0)

enum { WORKERS = 8, LOOPS = 1500 };
typedef struct test_shared {
    ds_win32_mutex mutex;
    ds_win32_cond cond;
    ds_win32_sem sem;
    ds_win32_tls tls;
    int counter;
    int done;
    int invalid;
} test_shared;
typedef struct test_arg {
    test_shared* shared;
    unsigned thread_num;
} test_arg;

static void* worker(void* ctx) {
    test_arg* arg = (test_arg*)ctx;
    test_shared* s = arg->shared;
    const uintptr_t cookie = 0x1000U + arg->thread_num;
    unsigned i;
    if (ds_win32_tls_get(&s->tls) != NULL) s->invalid = 1;
    if (ds_win32_tls_set(&s->tls, (void*)cookie)) s->invalid = 1;
    for (i = 0; i < LOOPS; ++i) {
        ds_win32_mutex_lock(&s->mutex);
        /* Recursive mutex semantics expected by translated guest. */
        ds_win32_mutex_lock(&s->mutex);
        ++s->counter;
        ds_win32_mutex_unlock(&s->mutex);
        ds_win32_mutex_unlock(&s->mutex);
    }
    if ((uintptr_t)ds_win32_tls_get(&s->tls) != cookie) s->invalid = 1;
    ds_win32_mutex_lock(&s->mutex);
    ++s->done;
    ds_win32_cond_broadcast(&s->cond);
    ds_win32_mutex_unlock(&s->mutex);
    if (ds_win32_sem_post(&s->sem)) s->invalid = 1;
    return (void*)(uintptr_t)(0x4000U + arg->thread_num);
}

static int test_memory(void) {
    unsigned char* mem = (unsigned char*)ds_win32_map_anonymous(3U * 1024 * 1024);
    MEMORY_BASIC_INFORMATION mbi;
    unsigned i;
    CHECK(sizeof(void*) == 4);
    CHECK(mem != NULL);
    CHECK(ds_win32_page_size() >= 4096);
    for (i=0;i<3U*1024*1024;i+=4096) mem[i] = (unsigned char)(i >> 12);
    CHECK(VirtualQuery(mem, &mbi, sizeof(mbi)) == sizeof(mbi));
    CHECK(mbi.State == MEM_COMMIT);
    CHECK(ds_win32_protect(mem, 4096, DS_WIN32_PROT_READ) == 0);
    CHECK(VirtualQuery(mem, &mbi, sizeof(mbi)) == sizeof(mbi));
    CHECK(mbi.Protect == PAGE_READONLY);
    CHECK(ds_win32_protect(mem, 4096, DS_WIN32_PROT_READ | DS_WIN32_PROT_WRITE) == 0);
    mem[0] = 42;
    CHECK(mem[0] == 42);
    CHECK(ds_win32_unmap(mem) == 0);
    CHECK(ds_win32_map_anonymous(0) == NULL);
    return 0;
}

int main(void) {
    test_shared s;
    ds_win32_thread th[WORKERS];
    test_arg args[WORKERS];
    unsigned i;
    uintptr_t ret;
    int main_sentinel = 123;
    CHECK(test_memory() == 0);
    memset(&s, 0, sizeof(s));
    CHECK(ds_win32_mutex_init(&s.mutex) == 0);
    ds_win32_cond_init(&s.cond);
    CHECK(ds_win32_sem_init(&s.sem, 0, WORKERS) == 0);
    CHECK(ds_win32_tls_init(&s.tls) == 0);
    CHECK(ds_win32_tls_set(&s.tls, &main_sentinel) == 0);
    for (i=0; i<WORKERS; ++i) {
        args[i].shared = &s;
        args[i].thread_num = i;
        CHECK(ds_win32_thread_start(&th[i], worker, &args[i]) == 0);
    }
    ds_win32_mutex_lock(&s.mutex);
    while(s.done < WORKERS)
        CHECK(ds_win32_cond_wait(&s.cond, &s.mutex, 30000) == 0);
    ds_win32_mutex_unlock(&s.mutex);
    for (i=0;i<WORKERS;++i) {
        CHECK(ds_win32_sem_wait(&s.sem, 30000) == 0);
        CHECK(ds_win32_thread_join(&th[i], &ret) == 0);
        CHECK(ret == 0x4000U + i);
    }
    CHECK(s.counter == WORKERS * LOOPS);
    CHECK(s.invalid == 0);
    CHECK(ds_win32_tls_get(&s.tls) == &main_sentinel);
    /* The semaphore and condition variable timeout paths are real. */
    CHECK(ds_win32_sem_wait(&s.sem, 0) == 1);
    ds_win32_mutex_lock(&s.mutex);
    CHECK(ds_win32_cond_wait(&s.cond, &s.mutex, 0) == 1);
    ds_win32_mutex_unlock(&s.mutex);
    ds_win32_tls_destroy(&s.tls);
    ds_win32_sem_destroy(&s.sem);
    ds_win32_mutex_destroy(&s.mutex);
    puts("PASS: 32-bit native Windows guest memory, mutex/cond, semaphore, TLS and 8 worker threads");
    return 0;
}
