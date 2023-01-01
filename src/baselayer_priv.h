// Base services interface declaration, private-facing
// for the Build Engine
// by Jonathon Fowler (jf@jonof.id.au)

#ifndef __baselayer_priv_h__
#define __baselayer_priv_h__

extern char modechange;
extern char videomodereset;

// undefine to restrict windowed resolutions to conventional sizes
#define ANY_WINDOWED_SIZE

int baselayer_init(void);

int initsystem(void);
void uninitsystem(void);

#if USE_OPENGL
extern int glunavailable;

int loadgldriver(const char *driver);   // or NULL for platform default
void *getglprocaddress(const char *name, int ext);
int unloadgldriver(void);
#endif

#endif // __baselayer_priv_h__
