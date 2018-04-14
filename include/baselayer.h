// Base services interface declaration
// for the Build Engine
// by Jonathon Fowler (jf@jonof.id.au)

#ifndef __baselayer_h__
#define __baselayer_h__

#ifdef __cplusplus
extern "C" {
#endif

extern int _buildargc;
extern const char **_buildargv;

extern char quitevent, appactive;

enum {
    STARTWIN_CANCEL = 0,
    STARTWIN_RUN = 1,
};
struct startwin_settings;

// NOTE: these are implemented in game-land so they may be overridden in game specific ways
extern int app_main(int argc, char const * const argv[]);
extern int startwin_open(void);
extern int startwin_close(void);
extern int startwin_puts(const char *);
extern int startwin_settitle(const char *);
extern int startwin_idle(void *);
extern int startwin_run(struct startwin_settings *);

// video
extern int xres, yres, bpp, fullscreen, bytesperline, imageSize;
extern char offscreenrendering;
extern intptr_t frameplace;

extern void (*baselayer_onvideomodechange)(int);

#ifdef USE_OPENGL
struct glinfo {
	int loaded;
	const char *vendor;
	const char *renderer;
	const char *version;
	const char *extensions;

	float maxanisotropy;
	char bgra;
	char clamptoedge;
	char texcompr;
	char texnpot;
	char multisample;
	char nvmultisamplehint;
	char multitex;
	char envcombine;

	char hack_nofog;
};
extern struct glinfo glinfo;
#endif

extern int inputdevices;

// keys
#define KEYFIFOSIZ 64
extern char keystatus[256];
extern int keyfifo[KEYFIFOSIZ];
extern unsigned char keyasciififo[KEYFIFOSIZ];
extern int keyfifoplc, keyfifoend;
extern int keyasciififoplc, keyasciififoend;

// mouse
extern int mousex, mousey, mouseb;

// joystick
extern int joyaxis[], joyb;
extern char joynumaxes, joynumbuttons;



int initsystem(void);
void uninitsystem(void);

void initputs(const char *);
void debugprintf(const char *,...) PRINTF_FORMAT(1, 2);

int handleevents(void);

typedef void (*KeyPressCallback)(int,int);
typedef void (*MousePressCallback)(int,int);
typedef void (*JoyPressCallback)(int,int);
int initinput(void);
void uninitinput(void);
void releaseallbuttons(void);
void setkeypresscallback(void (*callback)(int,int));
void setmousepresscallback(void (*callback)(int,int));
void setjoypresscallback(void (*callback)(int,int));
const char *getkeyname(int num);
const char *getjoyname(int what, int num);	// what: 0=axis, 1=button, 2=hat

unsigned char bgetchar(void);
int bkbhit(void);
void bflushchars(void);

int initmouse(void);
void uninitmouse(void);
void grabmouse(int a);
void readmousexy(int *x, int *y);
void readmousebstatus(int *b);

int inittimer(int);
void uninittimer(void);
void sampletimer(void);
unsigned int getticks(void);
unsigned int getusecticks(void);
int gettimerfreq(void);
void (*installusertimercallback(void (*callback)(void)))(void);

int checkvideomode(int *x, int *y, int c, int fs, int forced);
int setvideomode(int x, int y, int c, int fs);
void getvalidmodes(void);
void resetvideomode(void);

void begindrawing(void);
void enddrawing(void);
void showframe(int);

int setpalette(int start, int num, unsigned char *dapal);
int setgamma(float gamma);

int wm_msgbox(const char *name, const char *fmt, ...) PRINTF_FORMAT(2, 3);
int wm_ynbox(const char *name, const char *fmt, ...) PRINTF_FORMAT(2, 3);
void wm_setapptitle(const char *name);

#ifdef USE_OPENGL
int loadgldriver(const char *driver);   // or NULL for platform default
void *getglprocaddress(const char *name, int ext);
int unloadgldriver(void);
#endif

// baselayer.c
int baselayer_init(void);
#ifdef USE_OPENGL
int baselayer_setupopengl(void);
void baselayer_dumpglinfo(void);
#endif

void makeasmwriteable(void);

#ifdef __cplusplus
}
#endif

#endif // __baselayer_h__

