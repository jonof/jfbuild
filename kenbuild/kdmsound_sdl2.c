// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#if defined __APPLE__
# include <SDL2/SDL.h>
#else
# include "SDL.h"
#endif

#define KDMSOUND_INTERNAL
#include "kdmsound.h"

#if (SDL_MAJOR_VERSION != 2)
#  error This must be built with SDL2
#endif

static SDL_AudioDeviceID dev;

static void preparesndbuf(void *udata, Uint8 *sndoffsplc, int sndbufsiz);

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

void initsb(char dadigistat, char damusistat, int dasamplerate, char danumspeakers, char dabytespersample, char daintspersec, char daquality)
{
    SDL_AudioSpec want, have;

    if (dev) return;

    if ((dadigistat == 0) && (damusistat != 1))
        return;

    SDL_memset(&want, 0, sizeof(want));
    want.freq = dasamplerate;
    want.format = dabytespersample == 1 ? AUDIO_U8 : AUDIO_S16SYS;
    want.channels = max(1, min(2, danumspeakers));
    want.samples = (((want.freq/120)+1)&~1);
    want.callback = preparesndbuf;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        SDL_Log("Failed to initialise SDL audio subsystem: %s", SDL_GetError());
        return;
    }

    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE |
            SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
    if (dev == 0) {
        SDL_Log("Failed to open audio: %s", SDL_GetError());
        return;
    }

    if (initkdm(dadigistat, damusistat, have.freq, have.channels, SDL_AUDIO_BITSIZE(have.format)>>3)) {
        SDL_CloseAudioDevice(dev);
        dev = 0;
        SDL_Log("Failed to initialise KDM");
        return;
    }

    loadwaves("waves.kwv");

    SDL_PauseAudioDevice(dev, 0);
}

void uninitsb(void)
{
    if (dev) SDL_CloseAudioDevice(dev);
    dev = 0;

    uninitkdm();
}

int lockkdm(void)
{
    if (!dev) return -1;
    SDL_LockAudioDevice(dev);
    return 0;
}

void unlockkdm(void)
{
    if (!dev) return;
    SDL_UnlockAudioDevice(dev);
}

void refreshaudio(void)
{
}

static void preparesndbuf(void *udata, Uint8 *sndoffsplc, int sndbufsiz)
{
    preparekdmsndbuf(sndoffsplc, sndbufsiz);
}
