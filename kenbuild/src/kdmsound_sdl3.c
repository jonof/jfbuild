// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#if defined __APPLE__
# include <SDL3/SDL.h>
#else
# include "SDL3/SDL.h"
#endif

#define KDMSOUND_INTERNAL
#include "kdmsound.h"

#if (SDL_MAJOR_VERSION != 3)
#  error This must be built with SDL3
#endif

static SDL_AudioStream *dev;
static int bytespertic;

static void SDLCALL preparesndbuf(void *udata, SDL_AudioStream *stream, int need, int total);

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

void initsb(char dadigistat, char damusistat, int dasamplerate, char danumspeakers, char dabytespersample, char daintspersec, char daquality)
{
    SDL_AudioSpec want;

    (void)daintspersec; (void)daquality;

    if (dev) return;

    if ((dadigistat == 0) && (damusistat != 1))
        return;

    SDL_memset(&want, 0, sizeof(want));
    want.freq = dasamplerate;
    want.format = dabytespersample == 1 ? SDL_AUDIO_U8 : SDL_AUDIO_S16;
    want.channels = max(1, min(2, danumspeakers));

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("Failed to initialise SDL audio subsystem: %s", SDL_GetError());
        return;
    }

    dev = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &want, preparesndbuf, NULL);
    if (!dev) {
        SDL_Log("Failed to open audio: %s", SDL_GetError());
        return;
    }
    if (initkdm(dadigistat, damusistat, want.freq, want.channels, SDL_AUDIO_BITSIZE(want.format)>>3)) {
        SDL_DestroyAudioStream(dev);
        dev = NULL;
        SDL_Log("Failed to initialise KDM");
        return;
    }

    loadwaves("waves.kwv");
    bytespertic = (SDL_AUDIO_BITSIZE(want.format)>>3)*want.channels*(((want.freq/120)+1)&~1);

    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(dev));
}

void uninitsb(void)
{
    if (dev) SDL_DestroyAudioStream(dev);
    dev = NULL;

    uninitkdm();
}

int lockkdm(void)
{
    if (!dev) return -1;
    SDL_LockAudioStream(dev);
    return 0;
}

void unlockkdm(void)
{
    if (!dev) return;
    SDL_UnlockAudioStream(dev);
}

void refreshaudio(void)
{
}

void SDLCALL preparesndbuf(void *udata, SDL_AudioStream *stream, int need, int total)
{
    Uint8 *sndoffsplc;
    int sndbufsiz = bytespertic;

    (void)udata; (void)total;

    if (need > 0) {
        while (sndbufsiz < need) sndbufsiz+=bytespertic;
        if ((sndoffsplc = SDL_stack_alloc(Uint8, sndbufsiz))) {
            preparekdmsndbuf(sndoffsplc, sndbufsiz);
            SDL_PutAudioStreamData(stream, sndoffsplc, sndbufsiz);
            SDL_stack_free(sndoffsplc);
        }
    }
}
