// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#ifndef __kdmsound_h__
#define __kdmsound_h__

#ifdef KDMSOUND_INTERNAL
int initkdm(char dadigistat, char damusistat, int dasamplerate, char danumspeakers, char dabytespersample);
void uninitkdm(void);
void preparekdmsndbuf(unsigned char *sndoffsplc, int sndbufsiz);

// Implemented in the per-platform interface.
int lockkdm(void);
void unlockkdm(void);
#endif

// Implemented in the per-platform interface.
void initsb(char dadigistat, char damusistat, int dasamplerate, char danumspeakers, char dabytespersample, char daintspersec, char daquality);
void uninitsb(void);
void refreshaudio(void);

void setears(int daposx, int daposy, int daxvect, int dayvect);
void wsayfollow(char *dafilename, int dafreq, int davol, int *daxplc, int *dayplc, char followstat);
void wsay(char *dafilename, int dafreq, int volume1, int volume2);
void loadwaves(char *wavename);
int loadsong(char *songname);
void musicon(void);
void musicoff(void);

#endif
