// SDL interface layer
// for the Build Engine
// by Jonathon Fowler (jf@jonof.id.au)
//
// Use SDL2 from http://www.libsdl.org

/*
 * Re: SDL surface blit vs renderer.
 *
 * Experiments on a Raspberry Pi 3 Model B+ have demonstrated that
 * there's merit in both 8-bit mode blitting techniques depending on
 * the Pi's configuration. With the GL driver disabled, the surface blit
 * path is significantly faster. With the GL driver enabled, the SDL
 * renderer interface is slightly quicker. However, faster than both is
 * the forthcoming GLSL 2 blitter. Also, on a Mac Mini G4 with hampered
 * AGP under Debian 8, surface blit is fast and the renderer slower.
 */
//#define SDLAYER_USE_RENDERER

// have stdio.h declare vasprintf
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif

#if defined __APPLE__
# include <SDL2/SDL.h>
#else
# include "SDL.h"
#endif

#if (SDL_MAJOR_VERSION != 2)
#  error This must be built with SDL2
#endif

#include <stdlib.h>
#include <math.h>

#include "build.h"
#include "sdlayer.h"
#include "cache1d.h"
#include "pragmas.h"
#include "a.h"
#include "osd.h"
#include "glbuild.h"

#if defined(__APPLE__)
# include "osxbits.h"
#elif defined(HAVE_GTK)
# include "gtkbits.h"
#else
int startwin_open(void) { return 0; }
int startwin_close(void) { return 0; }
int startwin_puts(const char *UNUSED(s)) { return 0; }
int startwin_idle(void *s) { return 0; }
int startwin_settitle(const char *s) { s=s; return 0; }
#endif

// undefine to restrict windowed resolutions to conventional sizes
#define ANY_WINDOWED_SIZE

int   _buildargc = 1;
const char **_buildargv = NULL;

char quitevent=0, appactive=1;

static char apptitle[256] = "Build Engine";
static char wintitle[256] = "";

// video
static SDL_Window *sdl_window;
#ifdef SDLAYER_USE_RENDERER
static SDL_Renderer *sdl_renderer;	// For non-GL 8-bit mode output.
static SDL_Texture *sdl_texture;	// For non-GL 8-bit mode output.
#else
static SDL_Surface *sdl_surface;	// For non-GL 8-bit mode output.
#endif
static unsigned char *frame;
int xres=-1, yres=-1, bpp=0, fullscreen=0, bytesperline, imageSize;
intptr_t frameplace=0;
char modechange=1;
char offscreenrendering=0;
char videomodereset = 0;
extern int gammabrightness;
extern float curgamma;

#if USE_OPENGL
static SDL_GLContext sdl_glcontext;
static glbuild8bit gl8bit;
static char nogl=0;
static int glswapinterval = 1;

static int set_glswapinterval(const osdfuncparm_t *parm);
#endif

// input
int inputdevices=0;
char keystatus[256];
int keyfifo[KEYFIFOSIZ];
unsigned char keyasciififo[KEYFIFOSIZ];
int keyfifoplc, keyfifoend;
int keyasciififoplc, keyasciififoend;
static char keynames[256][24];
int mousex=0,mousey=0,mouseb=0;
int joyaxis[SDL_CONTROLLER_AXIS_MAX], joyb=0;
char joynumaxes=0, joynumbuttons=0;
static char mouseacquired=0,moustat=0;
static SDL_GameController *controller = NULL;

void (*keypresscallback)(int,int) = 0;
void (*mousepresscallback)(int,int) = 0;
void (*joypresscallback)(int,int) = 0;

struct keytranslate {
	unsigned char normal;
	unsigned char controlchar;  // an ASCII control character to insert into the character fifo
};
#define WITH_CONTROL_KEY 0x80
static struct keytranslate keytranslation[SDL_NUM_SCANCODES];
static int buildkeytranslationtable(void);

static void shutdownvideo(void);

#ifndef __APPLE__
static SDL_Surface * loadappicon(void);
#endif

int wm_msgbox(const char *name, const char *fmt, ...)
{
	char *buf = NULL;
	int rv;
	va_list va;

	if (!name) {
		name = apptitle;
	}

	va_start(va,fmt);
	rv = vasprintf(&buf,fmt,va);
	va_end(va);

	if (rv < 0) return -1;

	do {
		rv = 0;

#if defined HAVE_GTK
		if (wmgtk_msgbox(name, buf) >= 0) {
			rv = 1;
			break;
		}
#endif
		if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, name, buf, sdl_window) >= 0) {
			rv = 1;
			break;
		}

		puts(buf);
	} while(0);

	free(buf);

	return rv;
}

int wm_ynbox(const char *name, const char *fmt, ...)
{
	char *buf = NULL, ch;
	int rv;
	va_list va;

	if (!name) {
		name = apptitle;
	}

	va_start(va,fmt);
	rv = vasprintf(&buf,fmt,va);
	va_end(va);

	if (rv < 0) return -1;

	SDL_MessageBoxButtonData buttons[2] = {
		{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes" },
		{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No" },
	};
	SDL_MessageBoxData msgbox = {
		SDL_MESSAGEBOX_INFORMATION,
		sdl_window,
		name,
		buf,
		SDL_arraysize(buttons),
		buttons,
		NULL
	};

	do {
		rv = 0;

#if defined HAVE_GTK
		if ((rv = wmgtk_ynbox(name, buf)) >= 0) {
			break;
		}
#endif
		if (SDL_ShowMessageBox(&msgbox, &rv) >= 0) {
			rv = (rv == 1);
			break;
		}

		puts(buf);
		puts("   (assuming 'No')");
		rv = 0;
	} while(0);

	free(buf);

	return rv;
}

int wm_filechooser(const char *initialdir, const char *initialfile, const char *type, int foropen, char **choice)
{
#if defined __APPLE__ || defined HAVE_GTK
    int rv;
    if (mouseacquired && moustat) {
        SDL_SetRelativeMouseMode(SDL_FALSE);
    }
#if defined __APPLE__
    rv = wmosx_filechooser(initialdir, initialfile, type, foropen, choice);
#elif defined HAVE_GTK
    rv = wmgtk_filechooser(initialdir, initialfile, type, foropen, choice);
#endif
    SDL_RaiseWindow(sdl_window);
    if (mouseacquired && moustat) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }
    return rv;
#endif
	return -1;
}

int wm_idle(void *ptr)
{
#if defined HAVE_GTK
    return wmgtk_idle(ptr);
#else
    (void)ptr;
    return 0;
#endif
}

void wm_setapptitle(const char *name)
{
	if (name) {
		Bstrncpy(apptitle, name, sizeof(apptitle)-1);
		apptitle[ sizeof(apptitle)-1 ] = 0;
	}
#if defined HAVE_GTK
	wmgtk_setapptitle(apptitle);
#endif
}

void wm_setwindowtitle(const char *name)
{
	if (name) {
		Bstrncpy(wintitle, name, sizeof(wintitle)-1);
		wintitle[ sizeof(wintitle)-1 ] = 0;
	}

	if (sdl_window) {
		SDL_SetWindowTitle(sdl_window, wintitle);
	}
}


//
//
// ---------------------------------------
//
// System
//
// ---------------------------------------
//
//

int main(int argc, char *argv[])
{
	int r;

	buildkeytranslationtable();

	// SDL must be initialised before GTK or else crashing will ensue.
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)) {
		buildprintf("Early initialisation of SDL failed! (%s)\n", SDL_GetError());
		return 1;
	}

#ifdef HAVE_GTK
	wmgtk_init(&argc, &argv);
#endif

#ifdef __APPLE__
	// consume Xcode's "-NSDocumentRevisionsDebugMode xx" parameter
	_buildargv = calloc(argc+1, sizeof(char *));
	for (r = _buildargc = 0; r < argc; r++) {
		if (strcmp(argv[r], "-NSDocumentRevisionsDebugMode") == 0) {
			r++;
		} else {
			_buildargv[_buildargc++] = argv[r];
		}
	}
	_buildargv[_buildargc] = 0;
#else
	_buildargc = argc;
	_buildargv = (const char **)argv;
#endif

	startwin_open();
	baselayer_init();

	// This avoids doubled character input in the (OSX) startup window's edit fields.
	SDL_EventState(SDL_TEXTINPUT, SDL_IGNORE);

	r = app_main(_buildargc, (char const * const*)_buildargv);

#ifdef __APPLE__
	free(_buildargv);
#endif

	startwin_close();
#ifdef HAVE_GTK
	wmgtk_exit();
#endif

	SDL_Quit();

	return r;
}


//
// initsystem() -- init SDL systems
//
int initsystem(void)
{
	SDL_version linked;
	SDL_version compiled;

	SDL_GetVersion(&linked);
	SDL_VERSION(&compiled);

	buildprintf("SDL2 system interface "
		  "(compiled with SDL version %d.%d.%d, runtime version %d.%d.%d)\n",
		linked.major, linked.minor, linked.patch,
		compiled.major, compiled.minor, compiled.patch);

	atexit(uninitsystem);

#if USE_OPENGL
	if (getenv("BUILD_NOGL")) {
		buildputs("OpenGL disabled.\n");
		nogl = 1;
	} else {
		nogl = loadgldriver(getenv("BUILD_GLDRV"));
		if (nogl) {
			buildputs("Failed loading OpenGL driver. GL modes will be unavailable.\n");
		}
	}

	OSD_RegisterFunction("glswapinterval", "glswapinterval: frame swap interval for OpenGL modes (0 = no vsync, max 2)", set_glswapinterval);
#endif

	return 0;
}


//
// uninitsystem() -- uninit SDL systems
//
void uninitsystem(void)
{
	uninitinput();
	uninitmouse();
	uninittimer();

	shutdownvideo();
#if USE_OPENGL
	glbuild_unloadfunctions();
	unloadgldriver();
#endif
}


//
// initputs() -- prints a string to the intitialization window
//
void initputs(const char *str)
{
	startwin_puts(str);
	startwin_idle(NULL);
	wm_idle(NULL);
}


//
// debugprintf() -- prints a debug string to stderr
//
void debugprintf(const char *f, ...)
{
#ifdef DEBUGGINGAIDS
	va_list va;

	va_start(va,f);
	Bvfprintf(stderr, f, va);
	va_end(va);
#endif
}


//
//
// ---------------------------------------
//
// All things Input
//
// ---------------------------------------
//
//

//
// initinput() -- init input system
//
int initinput(void)
{
	int i,j;

	inputdevices = 1|2; // keyboard (1) and mouse (2)
	mouseacquired = 0;

	memset(keynames,0,sizeof(keynames));
	for (i=0; i<SDL_NUM_SCANCODES; i++) {
		if (!keytranslation[i].normal) continue;
		strncpy(keynames[ keytranslation[i].normal ], SDL_GetScancodeName(i), sizeof(keynames[i])-1);
	}

	if (!SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)) {
		int flen, fh;
		char *dbuf = NULL;
		SDL_RWops *rwops = NULL;

		// Load an available controller mappings file.
		fh = kopen4load("gamecontrollerdb.txt", 0);
		if (fh >= 0) {
			flen = kfilelength(fh);
			if (flen >= 0) {
				dbuf = (char *)malloc(flen + 1);
			}
		}
		if (dbuf) {
			flen = kread(fh, dbuf, flen);
			dbuf[flen+1] = 0;
			kclose(fh);
			if (flen >= 0) {
				rwops = SDL_RWFromConstMem(dbuf, flen);
			}
		}
		if (rwops) {
			i = SDL_GameControllerAddMappingsFromRW(rwops, 0);
			buildprintf("Added %d game controller mappings\n", i);
			free(dbuf);
			SDL_free(rwops);
		}

		// Enumerate game controllers.
		if (SDL_NumJoysticks() < 1) {
			buildputs("No game controllers found\n");
		} else {
			int numjoysticks = SDL_NumJoysticks();
			buildputs("Game controllers:\n");
			for (i = 0; i < numjoysticks; i++) {
				if (SDL_IsGameController(i)) {
					buildprintf("  - %s\n", SDL_GameControllerNameForIndex(i));
					if (!controller) {
						controller = SDL_GameControllerOpen(i);
					}
				}
			}
			if (controller) {
				buildprintf("Using controller %s\n", SDL_GameControllerName(controller));

				inputdevices |= 4;
				joynumaxes    = SDL_CONTROLLER_AXIS_MAX;
				joynumbuttons = SDL_CONTROLLER_BUTTON_MAX;
			} else {
				buildprintf("No controllers are usable\n");
			}
		}
	}

	return 0;
}

//
// uninitinput() -- uninit input system
//
void uninitinput(void)
{
	uninitmouse();

	if (controller) {
		SDL_GameControllerClose(controller);
		controller = NULL;
	}
}

const char *getkeyname(int num)
{
	if ((unsigned)num >= 256) return NULL;
	return keynames[num];
}

const char *getjoyname(int what, int num)
{
	switch (what) {
		case 0: // axis
			return SDL_GameControllerGetStringForAxis(num);
		case 1: // button
			return SDL_GameControllerGetStringForButton(num);
		default:
			return NULL;
	}
}

//
// bgetchar, bkbhit, bflushchars -- character-based input functions
//
unsigned char bgetchar(void)
{
	unsigned char c;
	if (keyasciififoplc == keyasciififoend) return 0;
	c = keyasciififo[keyasciififoplc];
	keyasciififoplc = ((keyasciififoplc+1)&(KEYFIFOSIZ-1));
#ifdef __APPLE__
	if (c == SDL_SCANCODE_DELETE) c = SDL_SCANCODE_BACKSPACE;
#endif
	return c;
}

int bkbhit(void)
{
	return (keyasciififoplc != keyasciififoend);
}

void bflushchars(void)
{
	keyasciififoplc = keyasciififoend = 0;
}


//
// set{key|mouse|joy}presscallback() -- sets a callback which gets notified when keys are pressed
//
void setkeypresscallback(void (*callback)(int, int)) { keypresscallback = callback; }
void setmousepresscallback(void (*callback)(int, int)) { mousepresscallback = callback; }
void setjoypresscallback(void (*callback)(int, int)) { joypresscallback = callback; }

//
// initmouse() -- init mouse input
//
int initmouse(void)
{
	moustat=1;
	grabmouse(1);
	return 0;
}

//
// uninitmouse() -- uninit mouse input
//
void uninitmouse(void)
{
	grabmouse(0);
	moustat=0;
}


//
// grabmouse() -- show/hide mouse cursor
//
void grabmouse(int a)
{
	if (appactive && moustat) {
		if (a != mouseacquired) {
			SDL_SetRelativeMouseMode(a ? SDL_TRUE : SDL_FALSE);
			mouseacquired = a;
		}
	} else {
		mouseacquired = a;
	}
	mousex = mousey = 0;
}


//
// readmousexy() -- return mouse motion information
//
void readmousexy(int *x, int *y)
{
	if (!mouseacquired || !appactive || !moustat) { *x = *y = 0; return; }
	*x = mousex;
	*y = mousey;
	mousex = mousey = 0;
}

//
// readmousebstatus() -- return mouse button information
//
void readmousebstatus(int *b)
{
	if (!mouseacquired || !appactive || !moustat) *b = 0;
	else *b = mouseb;
	// clear mousewheel events - the game has them now (in *b)
	// the other mousebuttons are cleared when there's a "button released"
	// event, but for the mousewheel that doesn't work, as it's released immediately
	mouseb &= ~(1<<4 | 1<<5);
}

//
// releaseallbuttons()
//
void releaseallbuttons(void)
{
}


//
//
// ---------------------------------------
//
// All things Timer
// Ken did this
//
// ---------------------------------------
//
//

static Uint64 timerfreq=0;
static Uint32 timerlastsample=0;
static Uint32 timerticspersec=0;
static void (*usertimercallback)(void) = NULL;

//
// inittimer() -- initialise timer
//
int inittimer(int tickspersecond)
{
	if (timerfreq) return 0;    // already installed

	buildputs("Initialising timer\n");

	timerfreq = SDL_GetPerformanceFrequency();
	timerticspersec = tickspersecond;
	timerlastsample = (Uint32)(SDL_GetPerformanceCounter() * timerticspersec / timerfreq);

	usertimercallback = NULL;

	return 0;
}

//
// uninittimer() -- shut down timer
//
void uninittimer(void)
{
	if (!timerfreq) return;

	timerfreq=0;
}

//
// sampletimer() -- update totalclock
//
void sampletimer(void)
{
	int n;

	if (!timerfreq) return;

	n = (int)(SDL_GetPerformanceCounter() * timerticspersec / timerfreq) - timerlastsample;
	if (n>0) {
		totalclock += n;
		timerlastsample += n;
	}

	if (usertimercallback) for (; n>0; n--) usertimercallback();
}

//
// getticks() -- returns a millisecond ticks count
//
unsigned int getticks(void)
{
	return (unsigned int)SDL_GetTicks();
}

//
// getusecticks() -- returns a microsecond ticks count
//
unsigned int getusecticks(void)
{
	return (unsigned int)SDL_GetTicks() * 1000;
}


//
// gettimerfreq() -- returns the number of ticks per second the timer is configured to generate
//
int gettimerfreq(void)
{
	return timerticspersec;
}


//
// installusertimercallback() -- set up a callback function to be called when the timer is fired
//
void (*installusertimercallback(void (*callback)(void)))(void)
{
	void (*oldtimercallback)(void);

	oldtimercallback = usertimercallback;
	usertimercallback = callback;

	return oldtimercallback;
}



//
//
// ---------------------------------------
//
// All things Video
//
// ---------------------------------------
//
//


//
// getvalidmodes() -- figure out what video modes are available
//
static int sortmodes(const struct validmode_t *a, const struct validmode_t *b)
{
	int x;

	if ((x = a->fs   - b->fs)   != 0) return x;
	if ((x = a->bpp  - b->bpp)  != 0) return x;
	if ((x = a->xdim - b->xdim) != 0) return x;
	if ((x = a->ydim - b->ydim) != 0) return x;

	return 0;
}
static char modeschecked=0;
void getvalidmodes(void)
{
	static int defaultres[][2] = {
		{1920,1200},{1920,1080},{1600,1200},{1680,1050},{1600,900},{1400,1050},{1440,900},{1366,768},
		{1280,1024},{1280,960},{1280,800},{1280,720},{1152,864},{1024,768},{800,600},{640,480},
		{640,400},{512,384},{480,360},{400,300},{320,240},{320,200},{0,0}
	};
	SDL_DisplayMode mode, desktop;
	int i, j, maxx=0, maxy=0;

	if (modeschecked) return;

	validmodecnt=0;

	if (SDL_GetNumVideoDisplays() < 1) {
		buildputs("No video displays available!\n");
		return;
	}

	buildputs("Detecting video modes:\n");

#define ADDMODE(x,y,c,f) if (validmodecnt<MAXVALIDMODES) { \
	int mn; \
	for(mn=0;mn<validmodecnt;mn++) \
		if (validmode[mn].xdim==x && validmode[mn].ydim==y && \
			validmode[mn].bpp==c  && validmode[mn].fs==f) break; \
	if (mn==validmodecnt) { \
		validmode[validmodecnt].xdim=x; \
		validmode[validmodecnt].ydim=y; \
		validmode[validmodecnt].bpp=c; \
		validmode[validmodecnt].fs=f; \
		validmodecnt++; \
		buildprintf("  - %dx%d %d-bit %s\n", x, y, c, (f&1)?"fullscreen":"windowed"); \
	} \
}

#define CHECKL(w,h) if ((w < maxx) && (h < maxy))
#define CHECKLE(w,h) if ((w <= maxx) && (h <= maxy))

	SDL_GetDesktopDisplayMode(0, &desktop);
	maxx = desktop.w;
	maxy = desktop.h;

	// Fullscreen 8-bit modes: upsamples to the desktop mode
	for (i=0; defaultres[i][0]; i++)
		CHECKLE(defaultres[i][0],defaultres[i][1])
			ADDMODE(defaultres[i][0],defaultres[i][1],8,1)

#if USE_POLYMOST && USE_OPENGL
	// Fullscreen >8-bit modes
	if (!nogl) {
		for (j = SDL_GetNumDisplayModes(0) - 1; j >= 0; j--) {
			SDL_GetDisplayMode(0, j, &mode);
			if ((mode.w > MAXXDIM) || (mode.h > MAXYDIM)) continue;
			if (SDL_BITSPERPIXEL(mode.format) < 8) continue;
			ADDMODE(mode.w, mode.h, SDL_BITSPERPIXEL(mode.format), 1)
		}
	}
#endif

	// Windowed 8-bit modes
	for (i=0; defaultres[i][0]; i++) {
		CHECKL(defaultres[i][0],defaultres[i][1]) {
			ADDMODE(defaultres[i][0], defaultres[i][1], 8, 0)
		}
	}

#if USE_POLYMOST && USE_OPENGL
	// Windowed >8-bit modes
	if (!nogl) {
		for (i=0; defaultres[i][0]; i++) {
			CHECKL(defaultres[i][0],defaultres[i][1]) {
				ADDMODE(defaultres[i][0], defaultres[i][1], SDL_BITSPERPIXEL(desktop.format), 0)
			}
		}
	}
#endif

#undef CHECK
#undef ADDMODE

	qsort((void*)validmode, validmodecnt, sizeof(struct validmode_t), (int(*)(const void*,const void*))sortmodes);

	modeschecked=1;
}


//
// checkvideomode() -- makes sure the video mode passed is legal
//
int checkvideomode(int *x, int *y, int c, int fs, int forced)
{
	int i, nearest=-1, dx, dy, odx=9999, ody=9999;

	getvalidmodes();

#if USE_POLYMOST && USE_OPENGL
	if (c > 8 && nogl) return -1;
#else
	if (c > 8) return -1;
#endif

	// fix up the passed resolution values to be multiples of 8
	// and at least 320x200 or at most MAXXDIMxMAXYDIM
	if (*x < 320) *x = 320;
	if (*y < 200) *y = 200;
	if (*x > MAXXDIM) *x = MAXXDIM;
	if (*y > MAXYDIM) *y = MAXYDIM;
	*x &= 0xfffffff8l;

	for (i=0; i<validmodecnt; i++) {
		if (validmode[i].bpp != c) continue;
		if (validmode[i].fs != fs) continue;
		dx = klabs(validmode[i].xdim - *x);
		dy = klabs(validmode[i].ydim - *y);
		if (!(dx | dy)) {   // perfect match
			nearest = i;
			break;
		}
		if ((dx <= odx) && (dy <= ody)) {
			nearest = i;
			odx = dx; ody = dy;
		}
	}

#ifdef ANY_WINDOWED_SIZE
	if (!forced && (fs&1) == 0 && (nearest < 0 || (validmode[nearest].xdim!=*x || validmode[nearest].ydim!=*y)))
		return 0x7fffffffl;
#endif

	if (nearest < 0) {
		// no mode that will match (eg. if no fullscreen modes)
		return -1;
	}

	*x = validmode[nearest].xdim;
	*y = validmode[nearest].ydim;

	return nearest;     // JBF 20031206: Returns the mode number
}


static void shutdownvideo(void)
{
	if (frame) {
		free(frame);
		frame = NULL;
	}
#if USE_OPENGL
	if (!nogl) {
		glbuild_delete_8bit_shader(&gl8bit);
	}
	if (sdl_glcontext) {
#if USE_POLYMOST
		polymost_glreset();
#endif

		SDL_GL_DeleteContext(sdl_glcontext);
		sdl_glcontext = NULL;
	}
#endif
#ifdef SDLAYER_USE_RENDERER
	if (sdl_texture) {
		SDL_DestroyTexture(sdl_texture);
		sdl_texture = NULL;
	}
	if (sdl_renderer) {
		SDL_DestroyRenderer(sdl_renderer);
		sdl_renderer = NULL;
	}
#else
	if (sdl_surface) {
		SDL_FreeSurface(sdl_surface);
		sdl_surface = NULL;
	}
#endif
	if (sdl_window) {
		SDL_DestroyWindow(sdl_window);
		sdl_window = NULL;
	}
}

//
// setvideomode() -- set SDL video mode
//
int setvideomode(int x, int y, int c, int fs)
{
	int modenum, regrab = 0;
	int flags;

	if ((fs == fullscreen) && (x == xres) && (y == yres) && (c == bpp) &&
		!videomodereset) {
		OSD_ResizeDisplay(xres,yres);
		return 0;
	}

	if (checkvideomode(&x,&y,c,fs,0) < 0) return -1;	// Will return if GL mode not available.

	if (mouseacquired) {
		regrab = 1;
		grabmouse(0);
	}

	shutdownvideo();

	buildprintf("Setting video mode %dx%d (%d-bpp %s)\n", x,y,c,
		(fs & 1) ? "fullscreen" : "windowed");

	do {
		flags = SDL_WINDOW_HIDDEN;

#if USE_OPENGL
		if (!nogl) {
#if (USE_OPENGL == USE_GLES2)
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif (USE_OPENGL == USE_GL3)
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#endif

			SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
			SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#if USE_POLYMOST
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, glmultisample > 0);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, glmultisample);
#endif

			flags |= SDL_WINDOW_OPENGL;
		}
#endif

		if (fs & 1) {
			if (c > 8) flags |= SDL_WINDOW_FULLSCREEN;
			else flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		}

		sdl_window = SDL_CreateWindow(wintitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, x, y, flags);
		if (!sdl_window) {
			buildprintf("Error creating window: %s\n", SDL_GetError());

#if USE_POLYMOST && USE_OPENGL
			if (glmultisample > 0) {
				buildprintf("Retrying without multisampling.\n");
				glmultisample = 0;
				continue;
			}
#endif

			return -1;
		}
		break;
	} while (1);

#ifndef __APPLE__
	{
		SDL_Surface *icon = loadappicon();
		SDL_SetWindowIcon(sdl_window, icon);
		SDL_FreeSurface(icon);
	}
#endif

	if (c == 8) {
		int i, j, pitch;

		// Round up to a multiple of 4.
		pitch = (((x|1) + 4) & ~3);

#if USE_OPENGL
		if (nogl) {
#endif
#ifdef SDLAYER_USE_RENDERER
			// 8-bit software with no GL shader blitting goes via the SDL rendering apparatus.
			SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

			sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_PRESENTVSYNC);
			if (!sdl_renderer) {
				buildprintf("Error creating renderer: %s\n", SDL_GetError());
				return -1;
			}
			SDL_RenderSetLogicalSize(sdl_renderer, x, y);
			SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 0, 255);
			SDL_RenderClear(sdl_renderer);

			sdl_texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, x, y);
			if (!sdl_texture) {
				buildprintf("Error creating texture: %s\n", SDL_GetError());
				return -1;
			}
#else
			// 8-bit software with no GL shader blitting goes via the SDL rendering apparatus.
			sdl_surface = SDL_CreateRGBSurface(0, x, y, 8, 0, 0, 0, 0);
			if (!sdl_surface) {
				buildprintf("Error creating surface: %s\n", SDL_GetError());
				return -1;
			}
#endif
#if USE_OPENGL
		} else {
			// Prepare the GLSL shader for 8-bit blitting.
			sdl_glcontext = SDL_GL_CreateContext(sdl_window);
			if (!sdl_glcontext) {
				buildprintf("Error creating OpenGL context: %s\n", SDL_GetError());
				nogl = 1;
			} else if (baselayer_setupopengl()) {
				nogl = 1;
			} else if (glbuild_prepare_8bit_shader(&gl8bit, x, y, pitch) < 0) {
				nogl = 1;
			}
			if (nogl) {
				// Try again but without OpenGL.
				buildputs("Falling back to non-OpenGL render.\n");
				return setvideomode(x, y, c, fs);
			}
		}
#endif

		frame = (unsigned char *) malloc(pitch * y);
		if (!frame) {
			buildputs("Unable to allocate framebuffer\n");
			return -1;
		}

		frameplace = (intptr_t) frame;
		bytesperline = pitch;
		imageSize = bytesperline * y;
		numpages = 1;

		setvlinebpl(bytesperline);
		for (i = j = 0; i <= y; i++) {
			ylookup[i] = j;
			j += bytesperline;
		}

	} else {
#if USE_OPENGL
		sdl_glcontext = SDL_GL_CreateContext(sdl_window);
		if (!sdl_glcontext) {
			buildprintf("Error creating OpenGL context: %s\n", SDL_GetError());
			return -1;
		}

		if (baselayer_setupopengl()) {
			shutdownvideo();
			return -1;
		}
#if USE_POLYMOST
		polymost_glreset();
#endif

		frameplace = 0;
		bytesperline = 0;
		imageSize = 0;
		numpages = 127;
#else
		return -1;
#endif
	}

	SDL_ShowWindow(sdl_window);

	xres = x;
	yres = y;
	bpp = c;
	fullscreen = fs;
	modechange = 1;
	videomodereset = 0;
	OSD_ResizeDisplay(xres,yres);
#if USE_OPENGL
	if (sdl_glcontext) {
		if (SDL_GL_SetSwapInterval(glswapinterval) < 0) {
			buildputs("note: OpenGL swap interval could not be changed\n");
		}
	}
#endif

	gammabrightness = (SDL_SetWindowBrightness(sdl_window, curgamma) == 0);

	// setpalettefade will set the palette according to whether gamma worked
	setpalettefade(palfadergb.r, palfadergb.g, palfadergb.b, palfadedelta);

	if (regrab) grabmouse(1);

	startwin_close();

	// Start listening for character input.
	SDL_EventState(SDL_TEXTINPUT, SDL_ENABLE);

	return 0;
}


//
// resetvideomode() -- resets the video system
//
void resetvideomode(void)
{
	videomodereset = 1;
	modeschecked = 0;
}


//
// begindrawing() -- locks the framebuffer for drawing
//
void begindrawing(void)
{
}


//
// enddrawing() -- unlocks the framebuffer
//
void enddrawing(void)
{
}


//
// showframe() -- update the display
//
void showframe(void)
{
	int i,j;

#if USE_OPENGL
	if (!nogl) {
		if (bpp == 8) {
			glbuild_update_8bit_frame(&gl8bit, frame, xres, yres, bytesperline);
			glbuild_draw_8bit_frame(&gl8bit);
		}

		SDL_GL_SwapWindow(sdl_window);
		return;
	}
#endif

	unsigned char *pixels, *in;
	int pitch, y, x;

#ifdef SDLAYER_USE_RENDERER
	if (SDL_LockTexture(sdl_texture, NULL, (void**)&pixels, &pitch)) {
		debugprintf("Could not lock texture: %s\n", SDL_GetError());
		return;
	}

	in = frame;
	for (y = yres - 1; y >= 0; y--) {
		for (x = xres - 1; x >= 0; x--) {
#if B_LITTLE_ENDIAN
			// RGBA -> BGRA, ignoring A
			/*
			pixels[(x<<2)+0] = curpalettefaded[in[x]].b;
			pixels[(x<<2)+1] = curpalettefaded[in[x]].g;
			pixels[(x<<2)+2] = curpalettefaded[in[x]].r;
			pixels[(x<<2)+3] = 0;
			*/
			((unsigned int *)pixels)[x] = B_SWAP32(*(unsigned int *)&curpalettefaded[in[x]]) >> 8;
#else
			pixels[(x<<2)+0] = 0;
			pixels[(x<<2)+1] = curpalettefaded[in[x]].r;
			pixels[(x<<2)+2] = curpalettefaded[in[x]].g;
			pixels[(x<<2)+3] = curpalettefaded[in[x]].b;
#endif
		}
		pixels += pitch;
		in += bytesperline;
	}

	SDL_UnlockTexture(sdl_texture);
	if (SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL)) {
		debugprintf("Could not copy render texture: %s\n", SDL_GetError());
	}
	SDL_RenderPresent(sdl_renderer);

#else //SDLAYER_USE_RENDERER
	SDL_Surface *winsurface;

	if (SDL_LockSurface(sdl_surface)) {
		debugprintf("Could not lock surface: %s\n", SDL_GetError());
		return;
	}
	pixels = (unsigned char *)sdl_surface->pixels;
	pitch = sdl_surface->pitch;

	in = frame;
	for (y = yres - 1; y >= 0; y--) {
		memcpy(pixels, in, xres);
		pixels += pitch;
		in += bytesperline;
	}

	SDL_UnlockSurface(sdl_surface);

	winsurface = SDL_GetWindowSurface(sdl_window);
	if (!winsurface) {
		debugprintf("Could not get window surface: %s\n", SDL_GetError());
		return;
	}
	SDL_BlitSurface(sdl_surface, NULL, winsurface, NULL);
	SDL_UpdateWindowSurface(sdl_window);
#endif //SDLAYER_USE_RENDERER
}


//
// setpalette() -- set palette values
//
int setpalette(int UNUSED(start), int UNUSED(num), unsigned char * UNUSED(dapal))
{
#if USE_OPENGL
	if (!nogl) {
		glbuild_update_8bit_palette(&gl8bit, curpalettefaded);
	}
#endif
#ifndef SDLAYER_USE_RENDERER
	if (sdl_surface) {
		if (SDL_SetPaletteColors(sdl_surface->format->palette, (const SDL_Color *)curpalettefaded, 0, 256)) {
			debugprintf("Could not set palette: %s\n", SDL_GetError());
		}
	}
#endif
	return 0;
}

//
// setgamma
//
int setgamma(float gamma)
{
	if (sdl_window) {
		return SDL_SetWindowBrightness(sdl_window, gamma) == 0;
	}
	return 0;
}


#if USE_OPENGL
//
// loadgldriver -- loads an OpenGL DLL
//
int loadgldriver(const char *soname)
{
	const char *name = soname;
	if (!name) {
		name = "system OpenGL library";
	}

	buildprintf("Loading %s\n", name);
	if (SDL_GL_LoadLibrary(soname)) return -1;
	return 0;
}

int unloadgldriver(void)
{
	SDL_GL_UnloadLibrary();
	return 0;
}

//
// getglprocaddress
//
void *getglprocaddress(const char *name, int UNUSED(ext))
{
	return (void*)SDL_GL_GetProcAddress(name);
}
#endif


#ifndef __APPLE__
extern struct sdlappicon sdlappicon;
static SDL_Surface * loadappicon(void)
{
	SDL_Surface *surf;

	surf = SDL_CreateRGBSurfaceFrom((void*)sdlappicon.pixels,
			sdlappicon.width, sdlappicon.height, 32, sdlappicon.width*4,
			0xffl,0xff00l,0xff0000l,0xff000000l);

	return surf;
}
#endif

//
//
// ---------------------------------------
//
// Miscellany
//
// ---------------------------------------
//
//


//
// handleevents() -- process the SDL message queue
//   returns !0 if there was an important event worth checking (like quitting)
//
int handleevents(void)
{
	int code, rv=0, j, control;
	SDL_Event ev;
	static int firstcall = 1;
	int eattextinput = 0;

#define SetKey(key,state) { \
	keystatus[key] = state; \
		if (state) { \
	keyfifo[keyfifoend] = key; \
	keyfifo[(keyfifoend+1)&(KEYFIFOSIZ-1)] = state; \
	keyfifoend = ((keyfifoend+2)&(KEYFIFOSIZ-1)); \
		} \
}

	while (SDL_PollEvent(&ev)) {
		switch (ev.type) {
			case SDL_TEXTINPUT:
				if (eattextinput) {
					eattextinput = 0;
					break;
				}
				for (j = 0; j < SDL_TEXTINPUTEVENT_TEXT_SIZE && ev.text.text[j]; j++) {
					if (ev.text.text[j] & 0x80) {
						continue;   // UTF8 character byte
					}
					code = ev.text.text[j];
					if (OSD_HandleChar(code)) {
						if (((keyasciififoend+1)&(KEYFIFOSIZ-1)) != keyasciififoplc) {
							keyasciififo[keyasciififoend] = code;
							keyasciififoend = ((keyasciififoend+1)&(KEYFIFOSIZ-1));
						}
					}
				}
				break;

			case SDL_KEYUP:
				// (un)grab mouse with ctrl-g
				if (ev.key.keysym.sym == SDLK_g
					&& (ev.key.keysym.mod & KMOD_CTRL)) {
					grabmouse(!mouseacquired);
					break;
				}
				// else: fallthrough
			case SDL_KEYDOWN:
				code = keytranslation[ev.key.keysym.scancode].normal;
				control = keytranslation[ev.key.keysym.scancode].controlchar;

				if (control && ev.key.type == SDL_KEYDOWN) {
					int needcontrol = (control & WITH_CONTROL_KEY) == WITH_CONTROL_KEY;
					int mod = ev.key.keysym.mod & ~(KMOD_CAPS|KMOD_NUM);
					control &= ~WITH_CONTROL_KEY;

					// May need to insert a control character into the ascii input
					// FIFO depending on what the state of the control keys are.
					if ((needcontrol  && mod && (mod & KMOD_CTRL) == mod) ||
						(!needcontrol && (mod == KMOD_NONE))) {
						if (OSD_HandleChar(control)) {
							if (((keyasciififoend+1)&(KEYFIFOSIZ-1)) != keyasciififoplc) {
								keyasciififo[keyasciififoend] = control;
								keyasciififoend = ((keyasciififoend+1)&(KEYFIFOSIZ-1));
							}
						}
					}
				}

				// hook in the osd
				if (code == OSD_CaptureKey(-1)) {
					if (ev.key.type == SDL_KEYDOWN) {
						// The character produced by the OSD toggle key needs to be ignored.
						eattextinput = 1;

						OSD_ShowDisplay(-1);
					}
					break;
				} else if (OSD_HandleKey(code, (ev.key.type == SDL_KEYDOWN)) == 0)
					break;

				if (ev.key.type == SDL_KEYDOWN) {
					if (!keystatus[code]) {
						SetKey(code, 1);
						if (keypresscallback)
							keypresscallback(code, 1);
					}
				} else {
					SetKey(code, 0);
					if (keypresscallback)
						keypresscallback(code, 0);
				}
				break;

			case SDL_WINDOWEVENT:
				if (ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED ||
						ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
					appactive = ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED;
					if (mouseacquired && moustat) {
						SDL_SetRelativeMouseMode(appactive ? SDL_TRUE : SDL_FALSE);
					}
					rv=-1;
				}
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				switch (ev.button.button) {
					case SDL_BUTTON_LEFT: j = 0; break;
					case SDL_BUTTON_RIGHT: j = 1; break;
					case SDL_BUTTON_MIDDLE: j = 2; break;
					default: j = -1; break;
				}
				if (j<0) break;

				if (ev.button.state == SDL_PRESSED)
					mouseb |= (1<<j);
				else
					mouseb &= ~(1<<j);

				if (mousepresscallback)
					mousepresscallback(j+1, ev.button.state == SDL_PRESSED);
				break;

			case SDL_MOUSEWHEEL:
				if (ev.wheel.y > 0) {   // Up
					j = 5;
				} else if (ev.wheel.y < 0) {    // Down
					j = 4;
				} else {
					break;
				}
				mouseb |= 1<<j;
				// 'release' is done in readmousebstatus()
				if (mousepresscallback) {
					mousepresscallback(j+1, 1);
					mousepresscallback(j+1, 0);
				}
				break;

			case SDL_MOUSEMOTION:
				if (!firstcall) {
					if (appactive) {
						mousex += ev.motion.xrel;
						mousey += ev.motion.yrel;
					}
				}
				break;

			case SDL_CONTROLLERAXISMOTION:
				if (appactive) {
					joyaxis[ ev.caxis.axis ] = ev.caxis.value;
				}
				break;

			case SDL_CONTROLLERBUTTONDOWN:
			case SDL_CONTROLLERBUTTONUP:
				if (appactive) {
					if (ev.cbutton.state == SDL_PRESSED)
						joyb |= 1 << ev.cbutton.button;
					else
						joyb &= ~(1 << ev.cbutton.button);
				}
				break;

			case SDL_QUIT:
				quitevent = 1;
				rv=-1;
				break;

			default:
				//buildprintf("Got event (%d)\n", ev.type);
				break;
		}
	}

	sampletimer();
	startwin_idle(NULL);
	wm_idle(NULL);
#undef SetKey

	firstcall = 0;

	return rv;
}


static int buildkeytranslationtable(void)
{
	memset(keytranslation,0,sizeof(keytranslation));

#define MAP(x,y) keytranslation[x].normal = y
#define MAPC(x,y,c) keytranslation[x].normal = y, keytranslation[x].controlchar = c

	MAPC(SDL_SCANCODE_BACKSPACE, 0xe, 0x8);
	MAPC(SDL_SCANCODE_TAB,       0xf, 0x9);
	MAPC(SDL_SCANCODE_RETURN,    0x1c, 0xd);
	MAP(SDL_SCANCODE_PAUSE,     0x59);  // 0x1d + 0x45 + 0x9d + 0xc5
	MAPC(SDL_SCANCODE_ESCAPE,    0x1, 0x1b);
	MAP(SDL_SCANCODE_SPACE,     0x39);
	MAP(SDL_SCANCODE_APOSTROPHE,     0x28);
	MAP(SDL_SCANCODE_COMMA,     0x33);
	MAP(SDL_SCANCODE_MINUS,     0xc);
	MAP(SDL_SCANCODE_PERIOD,    0x34);
	MAP(SDL_SCANCODE_SLASH,     0x35);
	MAP(SDL_SCANCODE_0,     0xb);
	MAP(SDL_SCANCODE_1,     0x2);
	MAP(SDL_SCANCODE_2,     0x3);
	MAP(SDL_SCANCODE_3,     0x4);
	MAP(SDL_SCANCODE_4,     0x5);
	MAP(SDL_SCANCODE_5,     0x6);
	MAP(SDL_SCANCODE_6,     0x7);
	MAP(SDL_SCANCODE_7,     0x8);
	MAP(SDL_SCANCODE_8,     0x9);
	MAP(SDL_SCANCODE_9,     0xa);
	MAP(SDL_SCANCODE_SEMICOLON, 0x27);
	MAP(SDL_SCANCODE_EQUALS,    0xd);
	MAPC(SDL_SCANCODE_LEFTBRACKET,   0x1a, 0x1b | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_BACKSLASH, 0x2b, 0x1c | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_RIGHTBRACKET,  0x1b, 0x1d | WITH_CONTROL_KEY);
	MAP(SDL_SCANCODE_GRAVE, 0x29);
	MAPC(SDL_SCANCODE_A,     0x1e, 0x1 | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_B,     0x30, 0x2 | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_C,     0x2e, 0x3 | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_D,     0x20, 0x4 | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_E,     0x12, 0x5 | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_F,     0x21, 0x6 | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_G,     0x22, 0x7 | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_H,     0x23, 0x8 | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_I,     0x17, 0x9 | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_J,     0x24, 0xa | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_K,     0x25, 0xb | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_L,     0x26, 0xc | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_M,     0x32, 0xd | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_N,     0x31, 0xe | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_O,     0x18, 0xf | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_P,     0x19, 0x10 | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_Q,     0x10, 0x11 | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_R,     0x13, 0x12 | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_S,     0x1f, 0x13 | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_T,     0x14, 0x14 | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_U,     0x16, 0x15 | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_V,     0x2f, 0x16 | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_W,     0x11, 0x17 | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_X,     0x2d, 0x18 | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_Y,     0x15, 0x19 | WITH_CONTROL_KEY);
	MAPC(SDL_SCANCODE_Z,     0x2c, 0x1a | WITH_CONTROL_KEY);
	MAP(SDL_SCANCODE_DELETE,    0xd3);
	MAP(SDL_SCANCODE_KP_0,       0x52);
	MAP(SDL_SCANCODE_KP_1,       0x4f);
	MAP(SDL_SCANCODE_KP_2,       0x50);
	MAP(SDL_SCANCODE_KP_3,       0x51);
	MAP(SDL_SCANCODE_KP_4,       0x4b);
	MAP(SDL_SCANCODE_KP_5,       0x4c);
	MAP(SDL_SCANCODE_KP_6,       0x4d);
	MAP(SDL_SCANCODE_KP_7,       0x47);
	MAP(SDL_SCANCODE_KP_8,       0x48);
	MAP(SDL_SCANCODE_KP_9,       0x49);
	MAP(SDL_SCANCODE_KP_PERIOD, 0x53);
	MAP(SDL_SCANCODE_KP_DIVIDE, 0xb5);
	MAP(SDL_SCANCODE_KP_MULTIPLY,   0x37);
	MAP(SDL_SCANCODE_KP_MINUS,  0x4a);
	MAP(SDL_SCANCODE_KP_PLUS,   0x4e);
	MAPC(SDL_SCANCODE_KP_ENTER,  0x9c, 0xd);
	MAP(SDL_SCANCODE_UP,        0xc8);
	MAP(SDL_SCANCODE_DOWN,      0xd0);
	MAP(SDL_SCANCODE_RIGHT,     0xcd);
	MAP(SDL_SCANCODE_LEFT,      0xcb);
	MAP(SDL_SCANCODE_INSERT,    0xd2);
	MAP(SDL_SCANCODE_HOME,      0xc7);
	MAP(SDL_SCANCODE_END,       0xcf);
	MAP(SDL_SCANCODE_PAGEUP,    0xc9);
	MAP(SDL_SCANCODE_PAGEDOWN,  0xd1);
	MAP(SDL_SCANCODE_F1,        0x3b);
	MAP(SDL_SCANCODE_F2,        0x3c);
	MAP(SDL_SCANCODE_F3,        0x3d);
	MAP(SDL_SCANCODE_F4,        0x3e);
	MAP(SDL_SCANCODE_F5,        0x3f);
	MAP(SDL_SCANCODE_F6,        0x40);
	MAP(SDL_SCANCODE_F7,        0x41);
	MAP(SDL_SCANCODE_F8,        0x42);
	MAP(SDL_SCANCODE_F9,        0x43);
	MAP(SDL_SCANCODE_F10,       0x44);
	MAP(SDL_SCANCODE_F11,       0x57);
	MAP(SDL_SCANCODE_F12,       0x58);
	MAP(SDL_SCANCODE_NUMLOCKCLEAR,   0x45);
	MAP(SDL_SCANCODE_CAPSLOCK,  0x3a);
	MAP(SDL_SCANCODE_SCROLLLOCK, 0x46);
	MAP(SDL_SCANCODE_RSHIFT,    0x36);
	MAP(SDL_SCANCODE_LSHIFT,    0x2a);
	MAP(SDL_SCANCODE_RCTRL,     0x9d);
	MAP(SDL_SCANCODE_LCTRL,     0x1d);
	MAP(SDL_SCANCODE_RALT,      0xb8);
	MAP(SDL_SCANCODE_LALT,      0x38);
	MAP(SDL_SCANCODE_LGUI,    0xdb);  // win l
	MAP(SDL_SCANCODE_RGUI,    0xdc);  // win r
	MAP(SDL_SCANCODE_PRINTSCREEN,     -2);    // 0xaa + 0xb7
	MAP(SDL_SCANCODE_SYSREQ,    0x54);  // alt+printscr
	MAP(SDL_SCANCODE_APPLICATION,      0xdd);  // win menu?

	return 0;
}

#if USE_OPENGL
static int set_glswapinterval(const osdfuncparm_t *parm)
{
	int interval;

	if (nogl) {
		buildputs("glswapinterval is not adjustable\n");
		return OSDCMD_OK;
	}
	if (parm->numparms == 0) {
		buildprintf("glswapinterval = %d\n", glswapinterval);
		return OSDCMD_OK;
	}
	if (parm->numparms != 1) return OSDCMD_SHOWHELP;

	interval = Batol(parm->parms[0]);
	if (interval < 0 || interval > 2) return OSDCMD_SHOWHELP;

	if (SDL_GL_SetSwapInterval(interval) < 0) {
		buildputs("note: OpenGL swap interval could not be changed\n");
	} else {
		glswapinterval = interval;
	}
	return OSDCMD_OK;
}
#endif
