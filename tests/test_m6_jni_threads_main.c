/*
 * Asset-free 32-bit execution test of the ACTUAL milestone 6 JNI VM.
 * Compiled with the real host.c, copied to a temporary build directory.
 * Whole-game entry and untranslated EA functions are linker-GC'd.
 */
#define main deadspace_original_main_unused
#include "host.c"
#undef main
#include <assert.h>
#include <stdio.h>
#include <pthread.h>

_Static_assert(sizeof(void*) == 4, "JNI guest handles require native i686");

enum { NTHREADS=8, OBJS_PER_THREAD=256 };
static pthread_barrier_t phase;
static u32 handles[NTHREADS][OBJS_PER_THREAD];

static void* texture_worker_test(void* param) {
    int id = (int)(intptr_t)param;
    assert(g_exception == 0);
    assert(jni_exc_check(0, 0, 0, 0, 0) == 0);
    /* An actual fake Android AssetManager failure, as a texture worker sees. */
    u32 bad_path = new_obj(K_STRING, "this-file-does-not-exist-m6-texture", 0, 0);
    Mid method = {0};
    snprintf(method.cls, sizeof method.cls, "%s", "android/content/res/AssetManager");
    snprintf(method.name, sizeof method.name, "%s", "open");
    snprintf(method.sig, sizeof method.sig, "%s", "(Ljava/lang/String;)Ljava/io/InputStream;");
    JArg args[1] = {{0}}; args[0].i = bad_path;
    int handled = 0;
    u64 result = java_invoke(&method, 0, "L", 1, args, 0, &handled);
    assert(handled && result == 0);
    assert(g_exception == 1);
    assert(jni_exc_check(0, 0, 0, 0, 0) == 1);
    pthread_barrier_wait(&phase);
    /* The game/main thread verifies its exception state is still zero now. */
    pthread_barrier_wait(&phase);
    for (int i=0;i<OBJS_PER_THREAD;i++) {
        u32 h = new_obj(K_STRING, "texture-worker-object", 0, 0);
        assert(h);
        JObj* p = OBJ(h);
        assert(p && p->kind == K_STRING && p->s && !strcmp(p->s, "texture-worker-object"));
        handles[id][i]=h;
        assert(class_handle("com/ea/blast/TextureThread") ==
               class_handle("com/ea/blast/TextureThread"));
    }
    jni_exc_clear(0,0,0,0,0);
    assert(g_exception==0);
    return NULL;
}
int main(void) {
    g_log_java = 0;
    g_assets_root = "/unlikely-to-exist-deadspace-test-assets";
    assert(pthread_barrier_init(&phase, NULL, NTHREADS + 1)==0);
    pthread_t threads[NTHREADS];
    for(int i=0;i<NTHREADS;i++)
        assert(pthread_create(&threads[i],NULL,texture_worker_test,(void*)(intptr_t)i)==0);
    pthread_barrier_wait(&phase);
    assert(g_exception == 0); /* background thread exceptions must not leak */
    assert(jni_exc_check(0,0,0,0,0)==0);
    g_exception=1;
    assert(jni_exc_check(0,0,0,0,0)==1);
    jni_exc_clear(0,0,0,0,0);
    assert(jni_exc_check(0,0,0,0,0)==0);
    pthread_barrier_wait(&phase);
    for(int i=0;i<NTHREADS;i++) assert(pthread_join(threads[i],NULL)==0);
    for(int i=0;i<NTHREADS;i++) for(int k=0;k<OBJS_PER_THREAD;k++) {
        JObj* p = OBJ(handles[i][k]);
        assert(p && p->kind == K_STRING && !strcmp(p->s, "texture-worker-object"));
    }
    assert(pthread_barrier_destroy(&phase)==0);
    puts("PASS: real JNI AssetManager exception TLS / 8 texture workers / object publish");
    return 0;
}
