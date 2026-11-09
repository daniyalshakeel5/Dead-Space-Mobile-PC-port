/* Android AudioTrack's streaming PCM sink. EA's original native mixer owns
 * decoding, mixing and spatialization; this only supplies the platform output. */
#ifndef DS_AUDIO_SDL_H
#define DS_AUDIO_SDL_H
#ifdef DS_SDL
#include <SDL.h>
static pthread_mutex_t ds_audio_lock = PTHREAD_MUTEX_INITIALIZER;
static SDL_AudioDeviceID ds_audio_device;
static unsigned ds_audio_epoch, ds_audio_limit;
static int ds_audio_playing;
static unsigned long long ds_audio_samples, ds_audio_nonzero;
static int ds_audio_open(int rate, int channels, int frames) {
    if (rate < 8000 || rate > 192000 || channels < 1 || channels > 2 || frames < 512 || frames > rate) return 0;
    pthread_mutex_lock(&ds_audio_lock);
    if (ds_audio_device) { pthread_mutex_unlock(&ds_audio_lock); return 1; }
    SDL_AudioSpec want = {0}, got;
    want.freq = rate; want.format = AUDIO_S16SYS; want.channels = channels; want.samples = 1024;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0)
        ds_audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
    if (!ds_audio_device) fprintf(stderr, "[AUDIO_ERROR] open: %s\n", SDL_GetError());
    else {
        ds_audio_epoch++; ds_audio_playing = 0;
        ds_audio_limit = (unsigned)frames * channels * sizeof(short);
        ds_audio_samples = ds_audio_nonzero = 0;
        fprintf(stderr, "[AUDIO_OPEN] driver=%s rate=%d channels=%d format=S16 buffer_frames=%d\n", SDL_GetCurrentAudioDriver(), rate, channels, frames);
    }
    int ok = ds_audio_device != 0;
    pthread_mutex_unlock(&ds_audio_lock); return ok;
}
static void ds_audio_control(int play, int clear) {
    pthread_mutex_lock(&ds_audio_lock);
    if (ds_audio_device) {
        ds_audio_playing = play;
        SDL_PauseAudioDevice(ds_audio_device, !play);
        if (clear) { SDL_ClearQueuedAudio(ds_audio_device); ds_audio_epoch++; }
    }
    pthread_mutex_unlock(&ds_audio_lock);
}
static void ds_audio_close(void) {
    pthread_mutex_lock(&ds_audio_lock);
    ds_audio_playing = 0; ds_audio_epoch++;
    if (ds_audio_device) {
        SDL_CloseAudioDevice(ds_audio_device); ds_audio_device = 0;
        fprintf(stderr, "[AUDIO_CLOSE] samples=%llu nonzero=%llu\n", ds_audio_samples, ds_audio_nonzero);
    }
    pthread_mutex_unlock(&ds_audio_lock);
}
/* Blocking Android writes pace the original mixer. Never let its producer
 * accumulate an unbounded SDL queue. Stop/close can interrupt the wait. */
static int ds_audio_write(const short* samples, int count) {
    if (!samples || count < 0) return -2;
    unsigned epoch;
    pthread_mutex_lock(&ds_audio_lock); epoch = ds_audio_epoch;
    int written = 0;
    while (written < count) {
        if (!ds_audio_device || ds_audio_epoch != epoch || !ds_audio_playing) break;
        unsigned queued = SDL_GetQueuedAudioSize(ds_audio_device);
        unsigned room = queued < ds_audio_limit ? (ds_audio_limit - queued) / sizeof(short) : 0;
        if (!room) {
            pthread_mutex_unlock(&ds_audio_lock); SDL_Delay(2); pthread_mutex_lock(&ds_audio_lock); continue;
        }
        unsigned n = (unsigned)(count - written); if (n > room) n = room;
        if (SDL_QueueAudio(ds_audio_device, samples + written, n * sizeof(short)) < 0) {
            fprintf(stderr, "[AUDIO_ERROR] write: %s\n", SDL_GetError()); break;
        }
        unsigned nz = 0;
        for (unsigned i = 0; i < n; i++) nz += samples[written + i] != 0;
        if (nz && !ds_audio_nonzero) fprintf(stderr, "[AUDIO_PCM] first nonzero mixer output after %llu samples\n", ds_audio_samples);
        ds_audio_samples += n; ds_audio_nonzero += nz; written += n;
    }
    pthread_mutex_unlock(&ds_audio_lock);
    return written || !count ? written : -3;
}
#endif
#endif
