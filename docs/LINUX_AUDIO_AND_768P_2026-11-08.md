# Linux audio and 768p touch coordinates

The device's fullscreen drawable was 1366x768 while the game's initial
DisplayAndroidDelegate query returned the launcher's old 1280x720 defaults.
The launcher now requests 1366x768, and the delegate reports the actual SDL
drawable. Rendering and touch coordinates therefore use the same dimensions
from startup, including fullscreen or HiDPI changes.

The port ran the original native game without running its Java activity.
That activity calls AndroidEAAudioCore.Startup immediately after
MainActivity.onCreate. Without this step, the original audio engine had no
AudioTrack destination. The host now recreates that initialization with the
original exported JNI Init function and routes AudioTrack.play/stop/write
through SDL. The original EA engine still performs decoding and mixing.

Audio is 48 kHz signed 16-bit stereo. The sink's queue is capped at 4096
frames, with blocking writes to pace the original mixer. Stop/close interrupts
writers; Java short-array bounds are validated before accessing PCM.
SDL converts to the hardware's format as necessary. No substitute sounds,
modified game data, or proprietary audio assets are included in the patch.

The asset-free test tests queue limits, interrupted blocking writes, invalid
parameters, reopening with mono/stereo formats, and repeated close against
SDL's dummy device. The existing JNI worker and host syntax checks also pass.

Run `python3 source/tests/test_audio_sdl.py` with SDL2 development headers installed.
The runner accepts CC, CFLAGS, SDL_CFLAGS and SDL_LIBS for cross builds.

SDL API references: [queue ownership and threading](https://wiki.libsdl.org/SDL2/SDL_QueueAudio)
and [device format negotiation](https://wiki.libsdl.org/SDL2/SDL_OpenAudioDevice).

## Device validation

A 1200-frame run with the original assets completed normally. SDL opened
PulseAudio at 48000 Hz stereo; the original mixer delivered 366592 samples,
including 285872 nonzero samples. The first nonzero audio arrived after
79872 samples. The main GL viewport, drawable and requested window all
reported 1366x768. The 256x256 viewport in the trace is an offscreen render
pass, not the display surface. Live Linux accelerometer events continued
to reach NativeOnAcceleration. The installed fullscreen build has also
been launched for the user's listening and touch-alignment test.

This verifies audio transport and non-silent game output; subjective sound
quality and all gameplay sound cues are not inferred from sample counts.

The user confirmed “audio OK!” in the installed fullscreen build. Live
fullscreen logs likewise show a 1366x768 drawable and main viewport.
