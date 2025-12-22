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
extern int displaycnt;

extern void (*baselayer_videomodewillchange)(void);
extern void (*baselayer_videomodedidchange)(void);

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
extern int joyaxis[8], joyb;
extern char joynumaxes, joynumbuttons;



void initputs(const char *);
void debugprintf(const char *,...) PRINTF_FORMAT(1, 2);

int handleevents(void);

int initinput(void);
void uninitinput(void);
void releaseallbuttons(void);
const char *getkeyname(int num);
const char *getjoyname(int what, int num);	// what: 0=axis, 1=button, 2=hat

unsigned char bgetchar(void);
int bkbhit(void);
void bflushchars(void);
int bgetkey(void);  // >0 = press, <0 = release
int bkeyhit(void);
void bflushkeys(void);

int initmouse(void);
void uninitmouse(void);
void grabmouse(int a);
void readmousexy(int *x, int *y);
void readmousebstatus(int *b);

int inittimer(int, void(*)(void));
void uninittimer(void);
void sampletimer(void);
unsigned int getticks(void);
unsigned int getusecticks(void);
int gettimerfreq(void);

// If 'strict' is zero and 'fullsc' is zero, only 'bitspp' needs be valid
// and VIDEOMODE_RELAXED is returned, otherwise the mode matched, or -1 if none.
#define VIDEOMODE_RELAXED 0x7fffffff
int checkvideomode(int *xdim, int *ydim, int bitspp, int fullsc, int strict);
int setvideomode(int xdim, int ydim, int bitspp, int fullsc);
void getvalidmodes(void);
void resetvideomode(void);
const char *getdisplayname(int display);

// 'search' is a FINDVIDEOMODE_SEARCH_xxx value.
// 'refmode' is a validmode index used as the search reference, or -1 to use the current mode.
// 'dir' is a FINDVIDEOMODE_DIR_xxx value.
// Returns the found mode number.
#define FINDVIDEOMODE_SEARCH_ANY (0)    // No constraints.
#define FINDVIDEOMODE_SEARCH_XYDIM (1)  // Preserve bpp and fullscreen.
#define FINDVIDEOMODE_SEARCH_BITSPP (2) // Preserve x/ydim and fullscreen.
#define FINDVIDEOMODE_SEARCH_FULLSC (3) // Preserve x/ydim and bpp.
#define FINDVIDEOMODE_DIR_FIRST (-0x7fffffff)
#define FINDVIDEOMODE_DIR_PREV  (-1)
#define FINDVIDEOMODE_DIR_NEXT  (1)
#define FINDVIDEOMODE_DIR_LAST  (0x7fffffff)
int findvideomode(int search, int refmode, int dir);

void showframe(void);

int setpalette(int start, int num, unsigned char *dapal);
int setsysgamma(float shadergamma, float sysgamma);     // sysgamma < 0.f == restore system gamma, return < 0 == system gamma set error

int wm_msgbox(const char *name, const char *fmt, ...) PRINTF_FORMAT(2, 3);
int wm_ynbox(const char *name, const char *fmt, ...) PRINTF_FORMAT(2, 3);

// initialdir - the initial directory
// initialfile - the initial filename
// type - the file extension to choose (e.g. "map")
// foropen - boolean true, or false if for saving
// choice - the file chosen by the user to be free()'d when done
// Returns -1 if not supported, 0 if cancelled, 1 if accepted
int wm_filechooser(const char *initialdir, const char *initialfile, const char *type, int foropen, char **choice);

int wm_idle(void *);
void wm_setapptitle(const char *name);
void wm_setwindowtitle(const char *name);

// baselayer.c
void makeasmwriteable(void);

#ifdef __cplusplus
}
#endif

#endif // __baselayer_h__

