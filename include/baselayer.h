// Base services interface declaration
// for the Build Engine
// by Jonathon Fowler (jonof@edgenetwk.com)

#ifndef __baselayer_h__
#define __baselayer_h__

extern int _buildargc;
extern char **_buildargv;

extern char quitevent, appactive;

// video
extern long xres, yres, bpp, fullscreen, bytesperline, imageSize, frameplace;
extern char offscreenrendering;

extern void (*baselayer_onvideomodechange)(int);

#ifdef USE_OPENGL
struct glinfo {
	const char *vendor;
	const char *renderer;
	const char *version;
	const char *extensions;

	float maxanisotropy;
	char bgra, clamptoedge, texcompr;
};
extern struct glinfo glinfo;
#endif

extern char inputdevices;

// keys
#define KEYFIFOSIZ 64
extern char keystatus[256], keyfifo[KEYFIFOSIZ], keyfifoplc, keyfifoend;
extern unsigned char keyasciififo[KEYFIFOSIZ], keyasciififoplc, keyasciififoend;
extern unsigned char keynames[256][24];

// mouse
extern short mousex, mousey, mouseb;

// joystick
extern long joyaxis[16], joyb;
extern char joyisgamepad, joynumaxes, joynumbuttons, joynumhats;
extern long joyaxespresent;



int initsystem(void);
void uninitsystem(void);
unsigned int getsysmemsize(void);

void initprintf(const char *, ...);
void debugprintf(const char *,...);

int handleevents(void);

typedef void (*KeyPressCallback)(long,long);
typedef void (*MousePressCallback)(long,long);
typedef void (*JoyPressCallback)(long,long);
int initinput(void);
void uninitinput(void);
void releaseallbuttons(void);
void setkeypresscallback(void (*callback)(long,long));
void setmousepresscallback(void (*callback)(long,long));
void setjoypresscallback(void (*callback)(long,long));

unsigned char bgetchar(void);
int bkbhit(void);
void bflushchars(void);

int initmouse(void);
void uninitmouse(void);
void grabmouse(char a);
void readmousexy(short *x, short *y);
void readmousebstatus(short *b);
void setjoydeadzone(int axis, unsigned short dead, unsigned short satur);
void getjoydeadzone(int axis, unsigned short *dead, unsigned short *satur);

int inittimer(int);
void uninittimer(void);
void sampletimer(void);
unsigned long getticks(void);
int gettimerfreq(void);
void (*installusertimercallback(void (*callback)(void)))(void);

int checkvideomode(long *x, long *y, int c, int fs);
int setvideomode(int x, int y, int c, int fs);
void getvalidmodes(void);
void resetvideomode(void);

void begindrawing(void);
void enddrawing(void);
void showframe(int);

int setpalette(int start, int num, char *dapal);
//int getpalette(int start, int num, char *dapal);
int setgamma(float ro, float go, float bo);

int switchrendermethod(int,int);	// 0 = software, 1 = opengl | bool = reinit

int wm_msgbox(char *name, char *fmt, ...);
int wm_ynbox(char *name, char *fmt, ...);

// baselayer.c
int baselayer_init();

#endif // __baselayer_h__

