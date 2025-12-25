// Base services interface declaration, private-facing
// for the Build Engine
// by Jonathon Fowler (jf@jonof.id.au)

#ifndef __baselayer_priv_h__
#define __baselayer_priv_h__

extern char videomodereset;
extern unsigned maxrefreshfreq;

int baselayer_init(void);

int initsystem(void);
void uninitsystem(void);

void addvalidmode(int w, int h, int bpp, int fs, int display, unsigned refresh, int extra);
void addstandardvalidmodes(int maxx, int maxy, int bpp, int fs, int display, unsigned refresh, int extra);
void sortvalidmodes(void);

#if USE_OPENGL
extern int glunavailable;

int loadgldriver(const char *driver);   // or NULL for platform default
void *getglprocaddress(const char *name, int ext);
int unloadgldriver(void);
#endif

#endif // __baselayer_priv_h__
