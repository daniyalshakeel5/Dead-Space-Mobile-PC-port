/* Asset-free exercise of pacing and shutdown against SDL's real dummy sink. */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include "audio_sdl.h"
static short pcm[48000];
static int result;
static void* writer(void* unused) { result = ds_audio_write(pcm, 48000); return NULL; }
int main(void) {
    setenv("SDL_AUDIODRIVER", "dummy", 1);
    assert(!ds_audio_open(0, 2, 4096));
    assert(ds_audio_open(48000, 2, 4096));
    assert(ds_audio_write(pcm, 1) == -3);
    ds_audio_control(1, 0);
    assert(ds_audio_write(pcm, 0) == 0);
    assert(ds_audio_write(NULL, 1) == -2);
    for (int i = 0; i < 48000; i++) pcm[i] = (short)(i % 1000);
    pthread_t t;
    assert(!pthread_create(&t, NULL, writer, NULL));
    SDL_Delay(50);
    pthread_mutex_lock(&ds_audio_lock);
    assert(SDL_GetQueuedAudioSize(ds_audio_device) <= ds_audio_limit);
    pthread_mutex_unlock(&ds_audio_lock);
    ds_audio_control(0, 1);
    pthread_join(t, NULL);
    assert(result > 0 && result < 48000);
    assert(ds_audio_nonzero > 0);
    ds_audio_close();
    assert(ds_audio_open(44100, 1, 2048));
    ds_audio_control(1, 0);
    assert(ds_audio_write(pcm, 1024) == 1024);
    ds_audio_close(); ds_audio_close();
    puts("PASS: bounded PCM output, blocked-write cancellation, mono/stereo reopen");
    return 0;
}
