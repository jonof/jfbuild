// Windows interface layer
// for the Build Engine
// by Jonathon Fowler (jf@jonof.id.au)
//
// This is all very ugly.

#ifndef _WIN32
#error winlayer.c is for Windows only.
#endif

#include "build.h"

#define WINVER 0x0600
#define _WIN32_WINNT 0x0600

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xinput.h>
#include <math.h>

#define COMPILE_MULTIMON_STUBS
#include <multimon.h>

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <stdarg.h>
#include <commdlg.h>

#if USE_OPENGL
#include "glbuild_priv.h"
#include "wglext.h"
#endif

#include "baselayer_priv.h"
#include "winlayer.h"
#include "pragmas.h"
#include "a.h"
#include "osd.h"


static char *argvbuf = NULL;

// Windows crud
static HINSTANCE hInstance = 0;
static HWND hWindow = 0;
static HDC hDCWindow = NULL;
#define WINDOW_CLASS "buildapp"
static BOOL window_class_registered = FALSE;

static int backgroundidle = 0;
static char apptitle[256] = "Build Engine";
static char wintitle[256] = "";

static WORD defgamma[3][256], defgammaread = FALSE;
static float curshadergamma = 1.f, cursysgamma = -1.f;

static struct displayinfo {
	RECT bounds;
	RECT usablebounds;
	CHAR device[CCHDEVICENAME];
	char name[128];
} *displays;
int displaycnt;

#if USE_OPENGL
// OpenGL stuff
static HGLRC hGLRC = 0;
static HANDLE hGLDLL;
static glbuild8bit gl8bit;
static unsigned char *frame = NULL;

static HWND hGLWindow = NULL;
static HWND dummyhGLwindow = NULL;
static HDC hDCGLWindow = NULL;

static struct winlayer_glfuncs {
	HGLRC (WINAPI * wglCreateContext)(HDC);
	BOOL (WINAPI * wglDeleteContext)(HGLRC);
	PROC (WINAPI * wglGetProcAddress)(LPCSTR);
	BOOL (WINAPI * wglMakeCurrent)(HDC,HGLRC);
	BOOL (WINAPI * wglSwapBuffers)(HDC);

	const char * (WINAPI * wglGetExtensionsStringARB)(HDC hdc);
	BOOL (WINAPI * wglChoosePixelFormatARB)(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
	HGLRC (WINAPI * wglCreateContextAttribsARB)(HDC hDC, HGLRC hShareContext, const int *attribList);
	BOOL (WINAPI * wglSwapIntervalEXT)(int interval);
	int (WINAPI * wglGetSwapIntervalEXT)(void);

	int have_ARB_create_context_profile;
	int have_EXT_multisample;
	int have_EXT_swap_control;
	int have_EXT_swap_control_tear;
} wglfunc;
#endif

static LPTSTR GetWindowsErrorMsg(DWORD code);
static const char * getwindowserrorstr(DWORD code);
static void ShowErrorBox(const char *m);
static BOOL CheckWinVersion(void);
static LRESULT CALLBACK WndProcCallback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void fetchkeynames(void);
static void updatemouse(void);
static void updatejoystick(void);
static void UninitDIB(void);
static int SetupDIB(int width, int height);
static void UninitOpenGL(void);
static int SetupOpenGL(int width, int height, int bitspp);
static BOOL RegisterWindowClass(void);
static BOOL CreateAppWindow(int width, int height, int bitspp, int fs, unsigned refresh);
static void DestroyAppWindow(void);
static void UpdateAppWindowTitle(void);

static void enumdisplays(void);
static void shutdownvideo(void);

// video
static int desktopxdim=0,desktopydim=0,desktopbpp=0,desktopmodeset=-1;
static int windowposx, windowposy;

// input and events
static unsigned int mousewheel[2] = { 0,0 };
#define MouseWheelFakePressTime (100)	// getticks() is a 1000Hz timer, and the button press is faked for 100ms

static char taskswitching=1;

static char keynames[256][24];
static const int wscantable[256], wxscantable[256];




//-------------------------------------------------------------------------------------------------
//  MAIN CRAP
//=================================================================================================


//
// win_gethwnd() -- gets the window handle
//
intptr_t win_gethwnd(void)
{
	return (intptr_t)hWindow;
}


//
// win_gethinstance() -- gets the application instance
//
intptr_t win_gethinstance(void)
{
	return (intptr_t)hInstance;
}


//
// wm_msgbox/wm_ynbox() -- window-manager-provided message boxes
//
int wm_msgbox(const char *name, const char *fmt, ...)
{
	char buf[1000];
	va_list va;

	va_start(va,fmt);
	vsprintf(buf,fmt,va);
	va_end(va);

	MessageBox(hWindow,buf,name,MB_OK|MB_TASKMODAL);
	return 0;
}

int wm_ynbox(const char *name, const char *fmt, ...)
{
	char buf[1000];
	va_list va;
	int r;

	va_start(va,fmt);
	vsprintf(buf,fmt,va);
	va_end(va);

	r = MessageBox((HWND)win_gethwnd(),buf,name,MB_YESNO|MB_TASKMODAL);
	if (r==IDYES) return 1;
	return 0;
}

//
// wm_filechooser() -- display a file selector dialogue box
//
int wm_filechooser(const char *initialdir, const char *initialfile, const char *type, int foropen, char **choice)
{
	OPENFILENAME ofn;
	char filter[100], *filterp = filter;
	char filename[BMAX_PATH+1] = "";

	*choice = NULL;

	if (!foropen && initialfile) {
		strcpy(filename, initialfile);
	}

	// ext Files\0*.ext\0\0
	memset(filter, 0, sizeof(filter));
	sprintf(filterp, "%s Files", type);
	filterp += strlen(filterp) + 1;
	sprintf(filterp, "*.%s", type);

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hWindow;
	ofn.lpstrFilter = filter;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = sizeof(filename);
	ofn.lpstrInitialDir = initialdir;
	ofn.Flags = OFN_DONTADDTORECENT | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
	ofn.lpstrDefExt = type;

	if (foropen ? GetOpenFileName(&ofn) : GetSaveFileName(&ofn)) {
		*choice = strdup(filename);
		return 1;
	} else {
		return 0;
	}
}

//
// wm_setapptitle() -- changes the application title
//
void wm_setapptitle(const char *name)
{
	if (name) {
		Bstrncpy(apptitle, name, sizeof(apptitle)-1);
		apptitle[ sizeof(apptitle)-1 ] = 0;
	}

	UpdateAppWindowTitle();
	startwin_settitle(apptitle);
}

//
// wm_setwindowtitle() -- changes the rendering window title
//
void wm_setwindowtitle(const char *name)
{
	if (name) {
		Bstrncpy(wintitle, name, sizeof(wintitle)-1);
		wintitle[ sizeof(wintitle)-1 ] = 0;
	}

	UpdateAppWindowTitle();
}

//
// wm_allowbackgroundidle() -- allow the application to idle in the background
//
void wm_allowbackgroundidle(int onf)
{
	backgroundidle = onf;
}

//
// wm_allowtaskswitching() -- captures/releases alt+tab hotkeys
//
void wm_allowtaskswitching(int onf)
{
	if (onf == taskswitching) return;

	if (onf) {
		UnregisterHotKey(0,0);
		UnregisterHotKey(0,1);
	} else {
		RegisterHotKey(0,0,MOD_ALT,VK_TAB);
		RegisterHotKey(0,1,MOD_ALT|MOD_SHIFT,VK_TAB);
	}

	taskswitching = onf;
}

//
// SignalHandler() -- called when we've sprung a leak
//
static void SignalHandler(int signum)
{
	switch (signum) {
		case SIGSEGV:
			buildputs("Fatal Signal caught: SIGSEGV. Bailing out.\n");
			uninitsystem();
			if (stdout) fclose(stdout);
			break;
		default:
			break;
	}
}


//
// WinMain() -- main Windows entry point
//
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow)
{
	int r;
	char *argp;
	FILE *fpout = NULL, *fperr = NULL;
	HDC hdc;

	(void)lpCmdLine; (void)nCmdShow;

	hInstance = hInst;

	if (CheckWinVersion() || hPrevInst) {
		MessageBox(0, "This application must be run under Windows Vista or newer.",
			apptitle, MB_OK|MB_ICONSTOP);
		return -1;
	}

	hdc = GetDC(NULL);
	r = GetDeviceCaps(hdc, BITSPIXEL);
	ReleaseDC(NULL, hdc);
	if (r <= 8) {
		MessageBox(0, "This application requires a desktop colour depth of 65536-colours or more.",
			apptitle, MB_OK|MB_ICONSTOP);
		return -1;
	}

	// carve up the commandline into more recognizable pieces
	argvbuf = strdup(GetCommandLine());
	_buildargc = 0;
	if (argvbuf) {
		char quoted = 0, instring = 0, swallownext = 0;
		char *p,*wp; int i;
		for (p=wp=argvbuf; *p; p++) {
			if (*p == ' ') {
				if (instring && !quoted) {
					// end of a string
					*(wp++) = 0;
					instring = 0;
				} else if (instring) {
					*(wp++) = *p;
				}
			} else if (*p == '"' && !swallownext) {
				if (instring && quoted) {
					// end of a string
					if (p[1] == ' ') {
						*(wp++) = 0;
						instring = 0;
						quoted = 0;
					} else {
						quoted = 0;
					}
				} else if (instring && !quoted) {
					quoted = 1;
				} else if (!instring) {
					instring = 1;
					quoted = 1;
					_buildargc++;
				}
			} else if (*p == '\\' && p[1] == '"' && !swallownext) {
				swallownext = 1;
			} else {
				if (!instring) _buildargc++;
				instring = 1;
				*(wp++) = *p;
				swallownext = 0;
			}
		}
		*wp = 0;

		_buildargv = malloc(sizeof(char*)*_buildargc);
		wp = argvbuf;
		for (i=0; i<_buildargc; i++,wp++) {
			_buildargv[i] = wp;
			while (*wp) wp++;
		}
	}

	// pipe standard outputs to files
	if ((argp = Bgetenv("BUILD_REDIR_STDIO")) != NULL)
		if (!Bstrcasecmp(argp, "TRUE")) {
			fpout = freopen("stdout.txt", "w", stdout);
			fperr = freopen("stderr.txt", "w", stderr);
			if (!fpout) fpout = fopen("stdout.txt", "w");
			if (!fperr) fperr = fopen("stderr.txt", "w");
			if (fpout) setvbuf(fpout, 0, _IONBF, 0);
			if (fperr) setvbuf(fperr, 0, _IONBF, 0);
		}

	// install signal handlers
	signal(SIGSEGV, SignalHandler);

	if (RegisterWindowClass()) return -1;

	atexit(uninitsystem);

	startwin_open();
	baselayer_init();
	r = app_main(_buildargc, _buildargv);

	if (fpout) fclose(fpout);
	if (fperr) fclose(fperr);

	startwin_close();

	if (argvbuf) free(argvbuf);

	return r;
}


#if USE_OPENGL
static int set_glswapinterval(const osdfuncparm_t *parm)
{
	int interval;

	if (!wglfunc.wglSwapIntervalEXT || glunavailable) {
		buildputs("glswapinterval is not adjustable\n");
		return OSDCMD_OK;
	}
	if (parm->numparms == 0) {
		if (glswapinterval == -1) buildprintf("glswapinterval is %d (adaptive vsync)\n", glswapinterval);
		else buildprintf("glswapinterval is %d\n", glswapinterval);
		return OSDCMD_OK;
	}
	if (parm->numparms != 1) return OSDCMD_SHOWHELP;

	interval = atoi(parm->parms[0]);
	if (interval < -1 || interval > 8) return OSDCMD_SHOWHELP;

	if (interval == -1 && !wglfunc.have_EXT_swap_control_tear) {
		buildputs("adaptive glswapinterval is not available\n");
		return OSDCMD_OK;
	}

	glswapinterval = interval;
	wglfunc.wglSwapIntervalEXT(interval);

	return OSDCMD_OK;
}
#endif

//
// initsystem() -- init systems
//
int initsystem(void)
{
	buildputs("Initialising Windows system interface\n");

	// Get connected displays and primary desktop colour depth before any changes happen.
	enumdisplays();

	memset(curpalette, 0, sizeof(palette_t) * 256);

	atexit(uninitsystem);

	frameplace=0;

#if USE_OPENGL
	memset(&wglfunc, 0, sizeof(wglfunc));
	glunavailable = loadgldriver(getenv("BUILD_GLDRV"));
	if (!glunavailable) {
		// Load the core WGL functions.
		wglfunc.wglGetProcAddress = getglprocaddress("wglGetProcAddress", 0);
		wglfunc.wglCreateContext  = getglprocaddress("wglCreateContext", 0);
		wglfunc.wglDeleteContext  = getglprocaddress("wglDeleteContext", 0);
		wglfunc.wglMakeCurrent    = getglprocaddress("wglMakeCurrent", 0);
		wglfunc.wglSwapBuffers    = getglprocaddress("wglSwapBuffers", 0);
		glunavailable = !wglfunc.wglGetProcAddress ||
		 	   !wglfunc.wglCreateContext ||
		 	   !wglfunc.wglDeleteContext ||
		 	   !wglfunc.wglMakeCurrent ||
		 	   !wglfunc.wglSwapBuffers;
	}
	if (glunavailable) {
		buildputs("Failed loading OpenGL driver. GL modes will be unavailable.\n");
		memset(&wglfunc, 0, sizeof(wglfunc));
	} else {
		OSD_RegisterFunction("glswapinterval", "glswapinterval: frame swap interval for OpenGL modes. 0 = no vsync, -1 = adaptive, max 8", set_glswapinterval);
	}
#endif

	return 0;
}


//
// uninitsystem() -- uninit systems
//
void uninitsystem(void)
{
	DestroyAppWindow();

	startwin_close();

	uninitinput();
	uninittimer();

	wm_allowtaskswitching(1);

	shutdownvideo();
#if USE_OPENGL
	glbuild_unloadfunctions();
	memset(&wglfunc, 0, sizeof(wglfunc));
	unloadgldriver();
#endif

	if (displays) {
		free(displays);
		displays = NULL;
		displaycnt = 0;
	}
}


//
// initputs() -- prints a string to the intitialization window
//
void initputs(const char *buf)
{
	startwin_puts(buf);
}


//
// debugprintf() -- sends a debug string to the debugger
//
void debugprintf(const char *f, ...)
{
#ifdef DEBUGGINGAIDS
	va_list va;

	va_start(va,f);

	if (IsDebuggerPresent()) {
		va_list va2;
		char buf[1024];

		va_copy(va2, va);
		vsnprintf(buf, sizeof(buf), f, va2);
		OutputDebugString(buf);
		va_end(va2);
	}

	vfprintf(stderr, f, va);
	va_end(va);
#endif
}


//
// handleevents() -- process the Windows message queue
//   returns !0 if there was an important event worth checking (like quitting)
//
static int eatosdinput = 0;
int handleevents(void)
{
	int rv=0;
	MSG msg;

	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT)
			quitevent = 1;

		if (startwin_idle((void*)&msg) > 0) continue;

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	eatosdinput = 0;
	updatemouse();
	updatejoystick();

	if (!appactive || quitevent) rv = -1;

	sampletimer();

	return rv;
}




//-------------------------------------------------------------------------------------------------
//  INPUT (MOUSE/KEYBOARD)
//=================================================================================================

static char moustat = 0, mousegrab = 0;
static int joyblast=0;

static int xinputusernum = -1;

//
// initinput() -- init input system
//
int initinput(void)
{
	moustat=0;
	memset(keystatus, 0, sizeof(keystatus));
	keyfifoplc = keyfifoend = 0;
	keyasciififoplc = keyasciififoend = 0;

	inputdevices = 1;
	joynumaxes=0;
	joynumbuttons=0;

	fetchkeynames();

	{
		DWORD usernum, result;
		XINPUT_CAPABILITIES caps;

		buildputs("Initialising game controllers\n");

		for (usernum = 0; usernum < XUSER_MAX_COUNT; usernum++) {
			result = XInputGetCapabilities(usernum, XINPUT_FLAG_GAMEPAD, &caps);
			if (result == ERROR_SUCCESS && xinputusernum < 0) {
				xinputusernum = (int)usernum;
				inputdevices |= 4;

				joynumbuttons = 15;
				joynumaxes = 6;
			}
		}
		if (xinputusernum >= 0) {
			buildprintf("  - Using controller in port %d\n", xinputusernum);
		} else {
			buildputs("  - No usable controller found\n");
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

	xinputusernum = -1;
	inputdevices &= ~4;
}


//
// initmouse() -- init mouse input
//
int initmouse(void)
{
	RAWINPUTDEVICE rid;

	if (moustat) return 0;

	buildputs("Initialising mouse\n");

	// Register for mouse raw input.
	rid.usUsagePage = 0x01;
	rid.usUsage = 0x02;
	rid.dwFlags = 0;	// We want legacy events when the mouse is not grabbed, so no RIDEV_NOLEGACY.
	rid.hwndTarget = NULL;
	if (RegisterRawInputDevices(&rid, 1, sizeof(rid)) == FALSE) {
		buildprintf("initinput: could not register for raw mouse input (%s)\n",
			getwindowserrorstr(GetLastError()));
		return -1;
	}

	// grab input
	moustat=1;
	inputdevices |= 2;
	grabmouse(1);

	return 0;
}


//
// uninitmouse() -- uninit mouse input
//
void uninitmouse(void)
{
	RAWINPUTDEVICE rid;

	if (!moustat) return;

	grabmouse(0);
	moustat=mousegrab=0;

	// Unregister for mouse raw input.
	rid.usUsagePage = 0x01;
	rid.usUsage = 0x02;
	rid.dwFlags = RIDEV_REMOVE;
	rid.hwndTarget = NULL;
	if (RegisterRawInputDevices(&rid, 1, sizeof(rid)) == FALSE) {
		buildprintf("initinput: could not unregister for raw mouse input (%s)\n",
			getwindowserrorstr(GetLastError()));
	}
}


static void constrainmouse(int a)
{
	RECT rect;
	LONG x, y;

	if (!hWindow) return;
	if (a) {
		GetWindowRect(hWindow, &rect);

		x = rect.left + (rect.right - rect.left) / 2;
		y = rect.top + (rect.bottom - rect.top) / 2;
		rect.left = x - 1;
		rect.right = x + 1;
		rect.top = y - 1;
		rect.bottom = y + 1;

		ClipCursor(&rect);
		SetCursorPos(rect.left+1, rect.top+1);
		ShowCursor(FALSE);
	} else {
		ClipCursor(NULL);
		ShowCursor(TRUE);
	}
}

static void updatemouse(void)
{
	unsigned t = getticks();

	if (!mousegrab) return;

	// we only want the wheel to signal once, but hold the state for a moment
	if (mousewheel[0] > 0 && t - mousewheel[0] > MouseWheelFakePressTime) {
		mousewheel[0] = 0; mouseb &= ~16;
	}
	if (mousewheel[1] > 0 && t - mousewheel[1] > MouseWheelFakePressTime) {
		mousewheel[1] = 0; mouseb &= ~32;
	}
}

//
// grabmouse() -- show/hide mouse cursor
//
void grabmouse(int a)
{
	if (!moustat) return;

	mousegrab = a;

	constrainmouse(a);
	mousex = 0;
	mousey = 0;
	mouseb = 0;
}


//
// readmousexy() -- return mouse motion information
//
void readmousexy(int *x, int *y)
{
	if (!moustat || !mousegrab) { *x = *y = 0; return; }
	*x = mousex;
	*y = mousey;
	mousex = 0;
	mousey = 0;
}


//
// readmousebstatus() -- return mouse button information
//
void readmousebstatus(int *b)
{
	if (!moustat || !mousegrab) *b = 0;
	else *b = mouseb;
}


static void updatejoystick(void)
{
	XINPUT_STATE state;

	if (xinputusernum < 0) return;

	ZeroMemory(&state, sizeof(state));
	if (XInputGetState(xinputusernum, &state) != ERROR_SUCCESS) {
		buildputs("Joystick error, disabling.\n");
		joyb = 0;
		memset(joyaxis, 0, sizeof(joyaxis));
		xinputusernum = -1;
		return;
	}

	// We use SDL's game controller button order for BUILD:
	//   A, B, X, Y, Back, (Guide), Start, LThumb, RThumb,
	//   LShoulder, RShoulder, DPUp, DPDown, DPLeft, DPRight
	// So we must shuffle XInput around.
	joyb = ((state.Gamepad.wButtons & 0xF000) >> 12) |		// A,B,X,Y
	       ((state.Gamepad.wButtons & 0x0020) >> 1) |		// Back
	       ((state.Gamepad.wButtons & 0x0010) << 2) |       // Start
	       ((state.Gamepad.wButtons & 0x03C0) << 1) |       // LThumb,RThumb,LShoulder,RShoulder
	       ((state.Gamepad.wButtons & 0x000F) << 8);		// DPadUp,Down,Left,Right

	joyaxis[0] = state.Gamepad.sThumbLX;
	joyaxis[1] = -state.Gamepad.sThumbLY;
	joyaxis[2] = state.Gamepad.sThumbRX;
	joyaxis[3] = -state.Gamepad.sThumbRY;
	joyaxis[4] = (state.Gamepad.bLeftTrigger >> 1) | ((int)state.Gamepad.bLeftTrigger << 7);	// Extend to 0-32767
	joyaxis[5] = (state.Gamepad.bRightTrigger >> 1) | ((int)state.Gamepad.bRightTrigger << 7);
}


void releaseallbuttons(void)
{
	int i;

	mousewheel[0]=mousewheel[1]=0;
	mouseb = 0;

	joyb = joyblast = 0;

	for (i=0;i<256;i++) {
		//if (!keystatus[i]) continue;
		//if (OSD_HandleKey(i, 0) != 0) {
			OSD_HandleKey(i, 0);
			keystatus[i] = 0;
		//}
	}
}


//
// fetchkeynames() -- retrieves the names for all the keys on the keyboard
//
static void putkeyname(int vsc, int ex, int scan) {
	TCHAR tbuf[24];

	vsc <<= 16;
	vsc |= ex << 24;
	if (GetKeyNameText(vsc, tbuf, 24) == 0) return;
	CharToOemBuff(tbuf, keynames[scan], 24-1);

	//buildprintf("VSC %8x scan %-2x = %s\n", vsc, scan, keynames[scan]);
}

static void fetchkeynames(void)
{
	int scan;
	unsigned i;

	memset(keynames,0,sizeof(keynames));
	for (i=0; i < 256; i++) {
		scan = wscantable[i];
		if (scan != 0) {
			putkeyname(i, 0, scan);
		}
		scan = wxscantable[i];
		if (scan != 0) {
			putkeyname(i, 1, scan);
		}
	}
}

const char *getkeyname(int num)
{
	if ((unsigned)num >= 256) return NULL;
	return keynames[num];
}

const char *getjoyname(int what, int num)
{
	static const char * axisnames[6] = {
		"Left Stick X",
		"Left Stick Y",
		"Right Stick X",
		"Right Stick Y",
		"Left Trigger",
		"Right Trigger",
	};
	static const char * buttonnames[15] = {
		"A",
		"B",
		"X",
		"Y",
		"Start",
		"Guide",
		"Back",
		"Left Thumb",
		"Right Thumb",
		"Left Shoulder",
		"Right Shoulder",
		"DPad Up",
		"DPad Down",
		"DPad Left",
		"DPad Right",
	};
	switch (what) {
		case 0:	// axis
			if ((unsigned)num > (unsigned)6) return NULL;
			return axisnames[num];

		case 1: // button
			if ((unsigned)num > (unsigned)15) return NULL;
			return buttonnames[num];

		default:
			return NULL;
	}
}





//-------------------------------------------------------------------------------------------------
//  TIMER
//=================================================================================================

static int64_t timerfreq=0;
static int timerlastsample=0;
static int timerticspersec=0;
static void (*usertimercallback)(void) = NULL;

//  This timer stuff is all Ken's idea.

//
// inittimer() -- initialise timer
//
int inittimer(int tickspersecond, void(*callback)(void))
{
	int64_t t;

	if (timerfreq) return 0;	// already installed

	buildputs("Initialising timer\n");

	// OpenWatcom seems to want us to query the value into a local variable
	// instead of the global 'timerfreq' or else it gets pissed with an
	// access violation
	if (!QueryPerformanceFrequency((LARGE_INTEGER*)&t)) {
		ShowErrorBox("Failed fetching timer frequency");
		return -1;
	}
	timerfreq = t;
	timerticspersec = tickspersecond;
	QueryPerformanceCounter((LARGE_INTEGER*)&t);
	timerlastsample = (int)(t*timerticspersec / timerfreq);

	usertimercallback = callback;

	return 0;
}

//
// uninittimer() -- shut down timer
//
void uninittimer(void)
{
	if (!timerfreq) return;

	timerfreq=0;
	timerticspersec = 0;
}

//
// sampletimer() -- update totalclock
//
void sampletimer(void)
{
	int64_t i;
	int n;

	if (!timerfreq) return;

	QueryPerformanceCounter((LARGE_INTEGER*)&i);
	n = (int)(i*timerticspersec / timerfreq) - timerlastsample;
	if (n>0) {
		totalclock += n;
		timerlastsample += n;
	}

	if (usertimercallback) for (; n>0; n--) usertimercallback();
}


//
// getticks() -- returns the millisecond ticks count
//
unsigned int getticks(void)
{
	int64_t i;
	if (timerfreq == 0) return 0;
	QueryPerformanceCounter((LARGE_INTEGER*)&i);
	return (unsigned int)(i*INT64_C(1000)/timerfreq);
}


//
// getusecticks() -- returns the microsecond ticks count
//
unsigned int getusecticks(void)
{
	int64_t i;
	if (timerfreq == 0) return 0;
	QueryPerformanceCounter((LARGE_INTEGER*)&i);
	return (unsigned int)(i*INT64_C(1000000)/timerfreq);
}


//
// gettimerfreq() -- returns the number of ticks per second the timer is configured to generate
//
int gettimerfreq(void)
{
	return timerticspersec;
}




//-------------------------------------------------------------------------------------------------
//  VIDEO
//=================================================================================================

// DIB stuff
static HDC      hDCSection  = NULL;
static HBITMAP  hDIBSection = NULL;
static HPALETTE hPalette    = NULL;
static VOID    *lpPixels    = NULL;

static int setgammaramp(WORD gt[3][256]);
static int getgammaramp(WORD gt[3][256]);

static void shutdownvideo(void)
{
#if USE_OPENGL
	if (frame) {
		free(frame);
		frame = NULL;
	}
	glbuild_delete_8bit_shader(&gl8bit);
	UninitOpenGL();
#endif
	UninitDIB();

	if (desktopmodeset>=0) {
		ChangeDisplaySettingsEx(displays[desktopmodeset].device, NULL, NULL, 0, NULL);
		desktopmodeset = -1;
	}
}

//
// setvideomode() -- set the video mode
//
int setvideomode(int xdim, int ydim, int bitspp, int fullsc)
{
	int display, modenum;
	unsigned refresh=0;
	const char *str;

	if ((fullsc == fullscreen) && (xdim == xres) && (ydim == yres) && (bitspp == bpp) && !videomodereset) {
		OSD_ResizeDisplay(xres,yres);
		return 0;
	}

	display = fullsc>>8;
	if (display >= displaycnt) display = 0, fullsc &= 255; // Display number out of range, use primary instead.
	modenum = checkvideomode(&xdim,&ydim,bitspp,fullsc,0);
	if (modenum < 0) return -1;
	else if (modenum != VIDEOMODE_RELAXED) refresh = validmode[modenum].refresh;
	else if (fullsc&255) return -1; // Must be a perfect match for fullscreen.

	if (defgammaread) setgammaramp(defgamma);

	if (baselayer_videomodewillchange) baselayer_videomodewillchange();
	shutdownvideo();

	if ((fullsc&255) && refresh) str = "Setting video mode %dx%d (%d-bit fullscreen, display %d, %u Hz)\n";
	else if (fullsc&255) str = "Setting video mode %dx%d (%d-bit fullscreen, display %d)\n";
	else str = "Setting video mode %dx%d (%d-bit windowed)\n";
	buildprintf(str,xdim,ydim,bitspp,display,refresh);

	if (CreateAppWindow(xdim, ydim, bitspp, fullsc, refresh)) return -1;

	if (!defgammaread && getgammaramp(defgamma) == 0) defgammaread = 1;
	if (!defgammaread && usegammabrightness > 0) usegammabrightness = 1;

	videomodereset = 0;
	if (baselayer_videomodedidchange) baselayer_videomodedidchange();
	OSD_ResizeDisplay(xres,yres);

	return 0;
}

#if USE_POLYMOST && USE_OPENGL
static void enumdisplaymodes(CHAR *device, int display)
{
	DEVMODE dm;
	int m;

	dm.dmSize = sizeof(DEVMODE);
	for (m=0; EnumDisplaySettings(device, m, &dm); m++) {
		if (dm.dmBitsPerPel <= 8) continue;
		addvalidmode(dm.dmPelsWidth, dm.dmPelsHeight, dm.dmBitsPerPel,
			1, display, dm.dmDisplayFrequency, -1);
	}
}
#endif

static BOOL edmcallback(HMONITOR hMonitor, HDC hDC, LPRECT lprcBounds, LPARAM lParam)
{
	MONITORINFOEX info;
	int destidx;

	(void)hDC; (void)lprcBounds; (void)lParam;

	ZeroMemory(&info, sizeof(info));
	info.cbSize = sizeof(MONITORINFOEX);
	if (!GetMonitorInfo(hMonitor, (LPMONITORINFO)&info)) {
		debugprintf("edmcallback(): error getting monitor info for %p\n", hMonitor);
		return TRUE;
	}

	if (displaycnt == lParam) {
		debugprintf("edmcallback(): enumerating more than the anticipated number of monitors\n");
		return FALSE;
	}

	if ((info.dwFlags & MONITORINFOF_PRIMARY) && displaycnt > 0) {
		// Put the primary monitor in position 0 by moving everything so far one along.
		memmove(&displays[1], &displays[0], displaycnt*sizeof(struct displayinfo));
		memset(&displays[0], 0, sizeof(struct displayinfo));
		destidx = 0;
	} else {
		destidx = displaycnt;
	}

	memcpy(&displays[destidx].bounds, &info.rcMonitor, sizeof(RECT));
	memcpy(&displays[destidx].usablebounds, &info.rcWork, sizeof(RECT));
	strncpy(displays[destidx].device, info.szDevice, sizeof(displays[0].device)-1);
	strncpy(displays[destidx].name, info.szDevice, sizeof(displays[0].name)-1);

	// Extended desktops have 1:1 device:monitor correlation,
	// cloned desktops have 1:n device:monitor correlation.
	DISPLAY_DEVICE ddev;
	ZeroMemory(&ddev, sizeof(ddev));
	ddev.cb = sizeof(DISPLAY_DEVICE);
	if (EnumDisplayDevices(info.szDevice, 0, &ddev, 0)) {
		strncpy(displays[destidx].name, ddev.DeviceString, sizeof(displays[0].name)-1);
	}

	displaycnt++;
	return TRUE;
}

static void enumdisplays(void)
{
	DEVMODE desktopmode;
	int i, nalloc;

	ZeroMemory(&desktopmode, sizeof(DEVMODE));
	desktopmode.dmSize = sizeof(DEVMODE);
	if (EnumDisplaySettings(NULL,ENUM_CURRENT_SETTINGS,&desktopmode)) {
		desktopbpp = desktopmode.dmBitsPerPel;
	}

	displaycnt = 0;
	nalloc = GetSystemMetrics(SM_CMONITORS);
	displays = (struct displayinfo *)calloc(nalloc, sizeof(struct displayinfo));
	if (!displays) {
		buildputs("Could not allocate display information structures!\n");
		return;
	}
	EnumDisplayMonitors(NULL, NULL, edmcallback, nalloc);
	debugprintf("Displays available:\n");
	for (i=0; i<displaycnt; i++) {
		debugprintf("  %d) %s (%ldx%ld)\n", i, displays[i].name,
			displays[i].bounds.right-displays[i].bounds.left,
			displays[i].bounds.bottom-displays[i].bounds.top);
	}
}

//
// getvalidmodes() -- figure out what video modes are available
//
void getvalidmodes(void)
{
	int i, maxx, maxy;

	if (validmodecnt) return;

	// Fullscreen modes
	for (i=0; i<displaycnt; i++) {
		// 8-bit modes upsample to the desktop.
		addstandardvalidmodes(displays[i].bounds.right-displays[i].bounds.left,
			displays[i].bounds.bottom-displays[i].bounds.top, 8, 1, i, 0, -1);
#if USE_POLYMOST && USE_OPENGL
		if (!glunavailable) enumdisplaymodes(displays[i].device, i);
#endif
	}

	// Windowed modes
	maxx = maxy = INT_MIN;
	for (i=0; i<displaycnt; i++) {
		maxx = max(maxx, displays[i].usablebounds.right-displays[i].usablebounds.left);
		maxy = max(maxy, displays[i].usablebounds.bottom-displays[i].usablebounds.top);
	}
	addstandardvalidmodes(maxx, maxy, 8, 0, 0, 0, -1);
#if USE_POLYMOST && USE_OPENGL
	if (!glunavailable) {
		addstandardvalidmodes(maxx, maxy, desktopbpp, 0, 0, 0, -1);
	}
#endif

	sortvalidmodes();
}


//
// getdisplaynamee() -- returns a human friendly name for a particular display
//
const char *getdisplayname(int display)
{
	if (!displays || (unsigned)display >= (unsigned)displaycnt) return NULL;
	return displays[display].name;
}


//
// showframe() -- update the display
//
void showframe(void)
{
#if USE_OPENGL
	if (!glunavailable) {
		if (bpp == 8) {
			glbuild_update_8bit_frame(&gl8bit, frame, bytesperline, yres);
			glbuild_draw_8bit_frame(&gl8bit);
		}

		wglfunc.wglSwapBuffers(hDCGLWindow);
		return;
	}
#endif

	{
		if ((xres == desktopxdim && yres == desktopydim) || !fullscreen) {
			BitBlt(hDCWindow, 0, 0, xres, yres, hDCSection, 0, 0, SRCCOPY);
		} else {
			int xpos, ypos, xscl, yscl;
			int desktopaspect = divscale16(desktopxdim, desktopydim);
			int frameaspect = divscale16(xres, yres);

			if (desktopaspect >= frameaspect) {
				// Desktop is at least as wide as the frame. We maximise frame height and centre on width.
				ypos = 0;
				yscl = desktopydim;
				xscl = mulscale16(desktopydim, frameaspect);
				xpos = (desktopxdim - xscl) >> 1;
			} else {
				// Desktop is narrower than the frame. We maximise frame width and centre on height.
				xpos = 0;
				xscl = desktopxdim;
				yscl = divscale16(desktopxdim, frameaspect);
				ypos = (desktopydim - yscl) >> 1;
			}

			StretchBlt(hDCWindow, xpos, ypos, xscl, yscl, hDCSection, 0, 0, xres, yres, SRCCOPY);
		}
	}
}


//
// setpalette() -- set palette values
//
int setpalette(int start, int num, unsigned char * dapal)
{
	(void)start; (void)num; (void)dapal;

#if USE_OPENGL
	if (!glunavailable && bpp == 8) {
		glbuild_update_8bit_palette(&gl8bit, curpalettefaded);
		return 0;
	}
#endif
	if (hDCSection) {
		RGBQUAD rgb[256];
		int i;

		for (i = 0; i < 256; i++) {
			rgb[i].rgbBlue = curpalettefaded[i].b;
			rgb[i].rgbGreen = curpalettefaded[i].g;
			rgb[i].rgbRed = curpalettefaded[i].r;
			rgb[i].rgbReserved = 0;
		}

		SetDIBColorTable(hDCSection, 0, 256, rgb);
	}

	return 0;
}


//
// setgamma
//
static int setgammaramp(WORD gt[3][256])
{
	if (!hDCWindow || !hWindow) return -1;
	return SetDeviceGammaRamp(hDCWindow, gt) ? 0 : -1;
}

static int getgammaramp(WORD gt[3][256])
{
	if (!hDCWindow || !hWindow) return -1;
	return GetDeviceGammaRamp(hDCWindow, gt) ? 0 : -1;
}

static void makegammaramp(WORD gt[3][256], double gamma)
{
	int i;
	for (i=0;i<256;i++) {
		gt[0][i] =
		gt[1][i] =
		gt[2][i] = (WORD)min(65535, max(0, (int)(pow((double)i / 256.0, gamma) * 65535.0 + 0.5)));
	}
}

int setsysgamma(float shadergamma, float sysgamma)
{
	int r = 0;
#if USE_OPENGL
	if (!glunavailable && bpp == 8) glbuild_set_8bit_gamma(&gl8bit, shadergamma);
#endif
	if (sysgamma < 0.0) {
		if (defgammaread) r = setgammaramp(defgamma);
	} else {
		WORD gt[3][256];
		makegammaramp(gt, 1.0/sysgamma);
		if (hWindow) r = setgammaramp(gt);
	}
	if (r == 0) { curshadergamma = shadergamma; cursysgamma = sysgamma; }
	return r;
}


//
// UninitDIB() -- clean up the DIB renderer
//
static void UninitDIB(void)
{
	if (hPalette) {
		DeleteObject(hPalette);
		hPalette = NULL;
	}

	if (hDCSection) {
		DeleteDC(hDCSection);
		hDCSection = NULL;
	}

	if (hDIBSection) {
		DeleteObject(hDIBSection);
		hDIBSection = NULL;
	}
}


//
// SetupDIB() -- sets up DIB rendering
//
static int SetupDIB(int width, int height)
{
	struct binfo {
		BITMAPINFOHEADER header;
		RGBQUAD colours[256];
	} dibsect;
	int i;

	// create the new DIB section
	memset(&dibsect, 0, sizeof(dibsect));
	numpages = 1;	// KJS 20031225
	dibsect.header.biSize = sizeof(dibsect.header);
	dibsect.header.biWidth = width|1;	// Ken did this
	dibsect.header.biHeight = -height;
	dibsect.header.biPlanes = 1;
	dibsect.header.biBitCount = 8;
	dibsect.header.biCompression = BI_RGB;
	dibsect.header.biClrUsed = 256;
	dibsect.header.biClrImportant = 256;
	for (i=0; i<256; i++) {
		dibsect.colours[i].rgbBlue = curpalettefaded[i].b;
		dibsect.colours[i].rgbGreen = curpalettefaded[i].g;
		dibsect.colours[i].rgbRed = curpalettefaded[i].r;
	}

	hDIBSection = CreateDIBSection(hDCWindow, (BITMAPINFO *)&dibsect, DIB_RGB_COLORS, &lpPixels, NULL, 0);
	if (!hDIBSection || lpPixels == NULL) {
		UninitDIB();
		ShowErrorBox("Error creating DIB section");
		return TRUE;
	}

	memset(lpPixels, 0, (((width|1) + 4) & ~3)*height);

	// create a compatible memory DC
	hDCSection = CreateCompatibleDC(hDCWindow);
	if (!hDCSection) {
		UninitDIB();
		ShowErrorBox("Error creating compatible DC");
		return TRUE;
	}

	// select the DIB section into the memory DC
	if (!SelectObject(hDCSection, hDIBSection)) {
		UninitDIB();
		ShowErrorBox("Error creating compatible DC");
		return TRUE;
	}

	return FALSE;
}

#if USE_OPENGL

//
// loadgldriver -- loads an OpenGL DLL
//
int loadgldriver(const char *dll)
{
	if (hGLDLL) return 0;	// Already loaded

	if (!dll) {
		dll = "OPENGL32.DLL";
	}

	buildprintf("Loading %s\n", dll);

	hGLDLL = LoadLibrary(dll);
	if (!hGLDLL) return -1;

	return 0;
}

int unloadgldriver(void)
{
	if (!hGLDLL) return 0;
	FreeLibrary(hGLDLL);
	hGLDLL = NULL;
	return 0;
}

//
// getglprocaddress
//
void *getglprocaddress(const char *name, int ext)
{
	void *func = NULL;
	if (!hGLDLL) return NULL;
	if (ext && wglfunc.wglGetProcAddress) {
		func = wglfunc.wglGetProcAddress(name);
	}
	if (!func) {
		func = GetProcAddress(hGLDLL, name);
	}
	return func;
}


//
// UninitOpenGL() -- cleans up OpenGL rendering
//

static void UninitOpenGL(void)
{
	if (hGLRC) {
#if USE_POLYMOST
		polymost_glreset();
#endif
		if (!wglfunc.wglMakeCurrent(0,0)) { }
		if (!wglfunc.wglDeleteContext(hGLRC)) { }
		hGLRC = NULL;
	}
	if (hGLWindow) {
		if (hDCGLWindow) {
			ReleaseDC(hGLWindow, hDCGLWindow);
			hDCGLWindow = NULL;
		}

		DestroyWindow(hGLWindow);
		hGLWindow = NULL;
	}
}

// Enumerate the WGL interface extensions.
static void EnumWGLExts(HDC hdc)
{
	const GLchar *extstr;
	char *workstr, *workptr, *nextptr = NULL, *ext = NULL;
	int ack;

	wglfunc.wglGetExtensionsStringARB = getglprocaddress("wglGetExtensionsStringARB", 1);
	if (!wglfunc.wglGetExtensionsStringARB) {
		debugprintf("Note: OpenGL does not provide WGL_ARB_extensions_string extension.\n");
		return;
	}

	extstr = wglfunc.wglGetExtensionsStringARB(hdc);

	debugprintf("WGL extensions supported:\n");
	workstr = workptr = strdup(extstr);
	while ((ext = Bstrtoken(workptr, " ", &nextptr, 1)) != NULL) {
		if (!strcmp(ext, "WGL_ARB_pixel_format")) {
			wglfunc.wglChoosePixelFormatARB = getglprocaddress("wglChoosePixelFormatARB", 1);
			ack = !wglfunc.wglChoosePixelFormatARB ? '!' : '+';
		} else if (!strcmp(ext, "WGL_ARB_create_context")) {
			wglfunc.wglCreateContextAttribsARB = getglprocaddress("wglCreateContextAttribsARB", 1);
			ack = !wglfunc.wglCreateContextAttribsARB ? '!' : '+';
		} else if (!strcmp(ext, "WGL_ARB_create_context_profile")) {
			wglfunc.have_ARB_create_context_profile = 1;
			ack = '+';
		} else if (!strcmp(ext, "WGL_EXT_multisample") || !strcmp(ext, "WGL_ARB_multisample")) {
			wglfunc.have_EXT_multisample = 1;
			ack = '+';
		} else if (!strcmp(ext, "WGL_EXT_swap_control")) {
			wglfunc.have_EXT_swap_control = 1;
			wglfunc.wglSwapIntervalEXT = getglprocaddress("wglSwapIntervalEXT", 1);
			wglfunc.wglGetSwapIntervalEXT = getglprocaddress("wglGetSwapIntervalEXT", 1);
			ack = (!wglfunc.wglSwapIntervalEXT || !wglfunc.wglGetSwapIntervalEXT) ? '!' : '+';
		} else if (!strcmp(ext, "WGL_EXT_swap_control_tear")) {
			wglfunc.have_EXT_swap_control_tear = 1;
			ack = '+';
		} else {
			ack = ' ';
		}
		debugprintf("  %s %c\n", ext, ack);
		workptr = NULL;
	}
	free(workstr);
}

//
// SetupOpenGL() -- sets up opengl rendering
//
static int SetupOpenGL(int width, int height, int bitspp)
{
	int err, pixelformat;

	// Step 1. Create a fake context with a safe pixel format descriptor.
	GLuint dummyPixelFormat;
	PIXELFORMATDESCRIPTOR dummyPfd = {
		sizeof(PIXELFORMATDESCRIPTOR),
		1,                             //Version Number
		PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER, //Must Support these
		PFD_TYPE_RGBA,                 //Request An RGBA Format
		32,                            //Color Depth
		0,0,0,0,0,0,                   //Color Bits Ignored
		0,                             //No Alpha Buffer
		0,                             //Shift Bit Ignored
		0,                             //No Accumulation Buffer
		0,0,0,0,                       //Accumulation Bits Ignored
		24,                            //16/24/32 Z-Buffer depth
		8,                             //Stencil Buffer
		0,                             //No Auxiliary Buffer
		PFD_MAIN_PLANE,                //Main Drawing Layer
		0,                             //Reserved
		0,0,0                          //Layer Masks Ignored
	};
	HDC dummyhDC = 0;
	HGLRC dummyhGLRC = 0;
	const char *errmsg = NULL;

	dummyhGLwindow = CreateWindow(
			WINDOW_CLASS,
			"OpenGL Window",
			WS_CHILD,
			0,0,
			1,1,
			hWindow,
			(HMENU)0,
			hInstance,
			NULL);
	if (!dummyhGLwindow) {
		errmsg = "Error creating dummy OpenGL child window.";
		goto fail;
	}

	dummyhDC = GetDC(dummyhGLwindow);
	if (!dummyhDC) {
		errmsg = "Error getting dummy device context";
		goto fail;
	}

	dummyPixelFormat = ChoosePixelFormat(dummyhDC, &dummyPfd);
	if (!dummyPixelFormat) {
		errmsg = "Can't choose dummy pixel format";
		goto fail;
	}

	err = SetPixelFormat(dummyhDC, dummyPixelFormat, &dummyPfd);
	if (!err) {
		errmsg = "Can't set dummy pixel format";
		goto fail;
	}

	dummyhGLRC = wglfunc.wglCreateContext(dummyhDC);
	if (!dummyhGLRC) {
		errmsg = "Can't create dummy GL context";
		goto fail;
	}

	if (!wglfunc.wglMakeCurrent(dummyhDC, dummyhGLRC)) {
		errmsg = "Can't activate dummy GL context";
		goto fail;
	}

	// Step 2. Check the WGL extensions.
	EnumWGLExts(dummyhDC);

	// Step 3. Create the actual window we will use going forward.
	hGLWindow = CreateWindow(
			WINDOW_CLASS,
			"OpenGL Window",
			WS_CHILD|WS_VISIBLE,
			0, 0,
			width, height,
			hWindow,
			(HMENU)0,
			hInstance,
			NULL);
	if (!hGLWindow) {
		errmsg = "Error creating OpenGL child window.";
		goto fail;
	}

	hDCGLWindow = GetDC(hGLWindow);
	if (!hDCGLWindow) {
		errmsg = "Error getting device context.";
		goto fail;
	}

#if USE_POLYMOST && USE_OPENGL
	if (!wglfunc.have_EXT_multisample) {
		glmultisample = 0;
	}
#endif

	// Step 3. Find and set a suitable pixel format.
	if (wglfunc.wglChoosePixelFormatARB) {
		UINT numformats;
		int pformatattribs[] = {
			WGL_DRAW_TO_WINDOW_ARB,	GL_TRUE,
			WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
			WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
			WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
			WGL_COLOR_BITS_ARB,     bitspp,
			WGL_DEPTH_BITS_ARB,     24,
			WGL_STENCIL_BITS_ARB,   0,
			WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_ARB,
#if USE_POLYMOST && USE_OPENGL
			WGL_SAMPLE_BUFFERS_EXT, glmultisample > 0,
			WGL_SAMPLES_EXT,        glmultisample > 0 ? (1 << glmultisample) : 0,
#endif
			0,
		};
		if (!wglfunc.wglChoosePixelFormatARB(hDCGLWindow, pformatattribs, NULL, 1, &pixelformat, &numformats)) {
			errmsg = "Can't choose pixel format.";
			goto fail;
		} else if (numformats < 1) {
			errmsg = "No suitable pixel format available.";
			goto fail;
		}
	} else {
		PIXELFORMATDESCRIPTOR pfd = {
			sizeof(PIXELFORMATDESCRIPTOR),
			1,                             //Version Number
			PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER, //Must Support these
			PFD_TYPE_RGBA,                 //Request An RGBA Format
			bitspp,                        //Color Depth
			0,0,0,0,0,0,                   //Color Bits Ignored
			0,                             //No Alpha Buffer
			0,                             //Shift Bit Ignored
			0,                             //No Accumulation Buffer
			0,0,0,0,                       //Accumulation Bits Ignored
			24,                            //16/24/32 Z-Buffer depth
			0,                             //Stencil Buffer
			0,                             //No Auxiliary Buffer
			PFD_MAIN_PLANE,                //Main Drawing Layer
			0,                             //Reserved
			0,0,0                          //Layer Masks Ignored
		};
		pixelformat = ChoosePixelFormat(hDCGLWindow, &pfd);
		if (!pixelformat) {
			errmsg = "Can't choose pixel format";
			goto fail;
		}
	}

	DescribePixelFormat(hDCGLWindow, pixelformat, sizeof(PIXELFORMATDESCRIPTOR), &dummyPfd);
	err = SetPixelFormat(hDCGLWindow, pixelformat, &dummyPfd);
	if (!err) {
		errmsg = "Can't set pixel format.";
		goto fail;
	}

	// Step 4. Create a context with the needed profile.
	if (wglfunc.wglCreateContextAttribsARB) {
		int contextattribs[] = {
#if (USE_OPENGL == USE_GL3)
			WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
			WGL_CONTEXT_MINOR_VERSION_ARB, 2,
			WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
#else
			WGL_CONTEXT_MAJOR_VERSION_ARB, 2,
			WGL_CONTEXT_MINOR_VERSION_ARB, 1,
			WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
#endif
			0,
		};
		if (!wglfunc.have_ARB_create_context_profile) {
			contextattribs[4] = 0;	//WGL_CONTEXT_PROFILE_MASK_ARB
		}
		hGLRC = wglfunc.wglCreateContextAttribsARB(hDCGLWindow, 0, contextattribs);
		if (!hGLRC) {
			errmsg = "Can't create GL context.";
			goto fail;
		}
	} else {
		hGLRC = wglfunc.wglCreateContext(hDCGLWindow);
		if (!hGLRC) {
			errmsg = "Can't create GL context.";
			goto fail;
		}
	}

	// Scrap the dummy stuff.
	if (!wglfunc.wglMakeCurrent(NULL, NULL)) { }
	if (!wglfunc.wglDeleteContext(dummyhGLRC)) { }
	ReleaseDC(dummyhGLwindow, dummyhDC);
	DestroyWindow(dummyhGLwindow);
	dummyhGLwindow = NULL;
	dummyhGLRC = NULL;
	dummyhDC = NULL;

	if (!wglfunc.wglMakeCurrent(hDCGLWindow, hGLRC)) {
		errmsg = "Can't activate GL context";
		goto fail;
	}

	// Step 5. Done.
	switch (glbuild_init()) {
		case 0:
			break;
		case -1:
			errmsg = "Can't load required OpenGL function pointers.";
			goto fail;
		case -2:
			errmsg = "Minimum OpenGL requirements are not met.";
			goto fail;
		default:
			errmsg = "Other OpenGL initialisation error.";
			goto fail;
	}

	if (wglfunc.have_EXT_swap_control && wglfunc.wglSwapIntervalEXT) {
		if (glswapinterval == -1 && !wglfunc.have_EXT_swap_control_tear) glswapinterval = 1;
		if (!wglfunc.wglSwapIntervalEXT(glswapinterval)) {
			buildputs("note: OpenGL swap interval could not be changed\n");
		}
	}
	numpages = 127;

	return FALSE;

fail:
	if (bpp > 8) {
		ShowErrorBox(errmsg);
	}
	shutdownvideo();

	if (!wglfunc.wglMakeCurrent(NULL, NULL)) { }

	if (hGLRC) {
		if (!wglfunc.wglDeleteContext(hGLRC)) { }
	}
	if (hGLWindow) {
		if (hDCGLWindow) {
			ReleaseDC(hGLWindow, hDCGLWindow);
		}
	}
	hDCGLWindow = NULL;
	hGLRC = NULL;
	hGLWindow = NULL;

	if (dummyhGLRC) {
		if (!wglfunc.wglDeleteContext(dummyhGLRC)) { }
	}
	if (dummyhGLwindow) {
		if (dummyhDC) {
			ReleaseDC(dummyhGLwindow, dummyhDC);
		}
		DestroyWindow(dummyhGLwindow);
	}

	return TRUE;
}

#endif	//USE_OPENGL

//
// CreateAppWindow() -- create the application window
//
static BOOL CreateAppWindow(int width, int height, int bitspp, int fs, unsigned refresh)
{
	RECT rect;
	int winw, winh, winx, winy, vieww, viewh, stylebits = 0, stylebitsex = 0, display;

	if (width == xres && height == yres && fs == fullscreen && bitspp == bpp && !videomodereset) return FALSE;

	display = fs>>8;
	fs &= 255;

	if (hWindow) {
		ShowWindow(hWindow, SW_HIDE);	// so Windows redraws what's behind if the window shrinks
	}

	if (fs) {
		stylebitsex = WS_EX_TOPMOST;
		stylebits = WS_POPUP;
	} else {
		stylebitsex = 0;
		stylebits = (WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX);
	}

	if (!hWindow) {
		hWindow = CreateWindowEx(
			stylebitsex,
			"buildapp",
			apptitle,
			stylebits,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			320,
			200,
			NULL,
			NULL,
			hInstance,
			0);
		if (!hWindow) {
			ShowErrorBox("Unable to create window");
			return TRUE;
		}

		hDCWindow = GetDC(hWindow);
		if (!hDCWindow) {
			ShowErrorBox("Error getting device context");
			return TRUE;
		}

		startwin_close();
	} else {
		SetWindowLong(hWindow,GWL_EXSTYLE,stylebitsex);
		SetWindowLong(hWindow,GWL_STYLE,stylebits);
	}

	// resize the window
	desktopxdim = displays[display].bounds.right - displays[display].bounds.left;
	desktopydim = displays[display].bounds.bottom - displays[display].bounds.top;
	if (!fs) {
		rect.left = 0;
		rect.top = 0;
		rect.right = width;
		rect.bottom = height;
		AdjustWindowRectEx(&rect, stylebits, FALSE, stylebitsex);

		// Centre on the relevant desktop.
		winw = (rect.right - rect.left);
		winh = (rect.bottom - rect.top);
		winx = displays[display].bounds.left + (desktopxdim - winw) / 2;
		winy = displays[display].bounds.top + (desktopydim - winh) / 2;
		vieww = width;
		viewh = height;
#if USE_OPENGL
	} else if (bitspp > 8) {
		DEVMODE dmScreenSettings;

		ZeroMemory(&dmScreenSettings, sizeof(DEVMODE));
		dmScreenSettings.dmSize = sizeof(DEVMODE);
		dmScreenSettings.dmPelsWidth = width;
		dmScreenSettings.dmPelsHeight = height;
		dmScreenSettings.dmBitsPerPel = bitspp;
		dmScreenSettings.dmFields = DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT;
		if (refresh > 0) {
			dmScreenSettings.dmDisplayFrequency = refresh;
			dmScreenSettings.dmFields |= DM_DISPLAYFREQUENCY;
		}

		if (ChangeDisplaySettingsEx(displays[display].device, &dmScreenSettings, NULL,
				CDS_FULLSCREEN, NULL) != DISP_CHANGE_SUCCESSFUL) {
			ShowErrorBox("Video mode not supported");
			return TRUE;
		}
		desktopmodeset = display;

		// Read back what was just set to get the position and dimensions.
		ZeroMemory(&dmScreenSettings, sizeof(DEVMODE));
		dmScreenSettings.dmSize = sizeof(DEVMODE);
		EnumDisplaySettings(displays[display].device, ENUM_CURRENT_SETTINGS, &dmScreenSettings);
		winx = dmScreenSettings.dmPosition.x;
		winy = dmScreenSettings.dmPosition.y;
		desktopxdim = winw = vieww = dmScreenSettings.dmPelsWidth;
		desktopydim = winh = viewh = dmScreenSettings.dmPelsHeight;
#endif
	} else {
		winx = displays[display].bounds.left;
		winy = displays[display].bounds.top;
		winw = vieww = desktopxdim;
		winh = viewh = desktopydim;
	}
	SetWindowPos(hWindow, HWND_TOP, winx, winy, winw, winh, 0);

	UpdateAppWindowTitle();
	ShowWindow(hWindow, SW_SHOWNORMAL);
	SetForegroundWindow(hWindow);
	SetFocus(hWindow);

	if (bitspp == 8) {
		int i, j;

#if USE_OPENGL
		if (glunavailable) {
#endif
			// 8-bit software with no GL shader uses classic Windows DIB blitting.
			if (SetupDIB(width, height)) {
				return TRUE;
			}

			frameplace = (intptr_t)lpPixels;
			bytesperline = (((width|1) + 4) & ~3);
#if USE_OPENGL
		} else {
			// Prepare the GLSL shader for 8-bit blitting.
			if (SetupOpenGL(vieww, viewh, bitspp)) {
				// No luck. Write off OpenGL and try DIB.
				buildputs("OpenGL initialisation failed. Falling back to DIB mode.\n");
				glunavailable = 1;
				return CreateAppWindow(width, height, bitspp, fs, refresh);
			}

			bytesperline = (((width|1) + 4) & ~3);

			if (glbuild_prepare_8bit_shader(&gl8bit, width, height, bytesperline, vieww, viewh) < 0) {
				shutdownvideo();
				return -1;
			}

			frame = (unsigned char *) malloc(bytesperline * height);
			if (!frame) {
				shutdownvideo();
				buildputs("Unable to allocate framebuffer\n");
				return FALSE;
			}

			frameplace = (intptr_t)frame;
		}
#endif

		imageSize = bytesperline*height;
		setvlinebpl(bytesperline);

		for(i=j=0; i<=height; i++) ylookup[i] = j, j += bytesperline;

		numpages = 1;
	} else {
#if USE_OPENGL
		ShowWindow(hWindow, SW_SHOWNORMAL);
		SetForegroundWindow(hWindow);
		SetFocus(hWindow);

		if (SetupOpenGL(width, height, bitspp)) {
			return TRUE;
		}

		frameplace = 0;
		bytesperline = 0;
		imageSize = 0;
#else
		return FALSE;
#endif
	}

	xres = width;
	yres = height;
	bpp = bitspp;
	fullscreen = (display<<8)|fs;

	UpdateWindow(hWindow);

	return FALSE;
}


//
// DestroyAppWindow() -- destroys the application window
//
static void DestroyAppWindow(void)
{
	if (defgammaread) {
		setgammaramp(defgamma);
	}

	shutdownvideo();

	if (hDCWindow) {
		ReleaseDC(hWindow, hDCWindow);
		hDCWindow = NULL;
	}

	if (hWindow) {
		DestroyWindow(hWindow);
		hWindow = NULL;
	}
}

//
// UpdateAppWindowTitle() -- sets the title of the application window
//
static void UpdateAppWindowTitle(void)
{
	char tmp[256+3+256+1];		//sizeof(wintitle) + " - " + sizeof(apptitle) + '\0'

	if (!hWindow) return;

	if (wintitle[0]) {
		snprintf(tmp, sizeof(tmp), "%s - %s", wintitle, apptitle);
		SetWindowText(hWindow, tmp);
	} else {
		SetWindowText(hWindow, apptitle);
	}
}






//-------------------------------------------------------------------------------------------------
//  MOSTLY STATIC INTERNAL WINDOWS THINGS
//=================================================================================================

//
// ShowErrorBox() -- shows an error message box
//
static void ShowErrorBox(const char *m)
{
	TCHAR msg[1024];

	wsprintf(msg, "%s: %s", m, GetWindowsErrorMsg(GetLastError()));
	MessageBox(0, msg, apptitle, MB_OK|MB_ICONSTOP);
}


//
// CheckWinVersion() -- check to see what version of Windows we happen to be running under
//
static BOOL CheckWinVersion(void)
{
	OSVERSIONINFO osv;

	ZeroMemory(&osv, sizeof(osv));
	osv.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if (!GetVersionEx(&osv)) return TRUE;

	// At least Windows Vista
	if (osv.dwPlatformId != VER_PLATFORM_WIN32_NT) return TRUE;
	if (osv.dwMajorVersion < 6) return TRUE;

	return FALSE;
}


static const int wscantable[256] = {
/*         x0    x1    x2    x3    x4    x5    x6    x7    x8    x9    xA    xB    xC    xD    xE    xF */
/* 0y */ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
/* 1y */ 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
/* 2y */ 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
/* 3y */ 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
/* 4y */ 0x40, 0x41, 0x42, 0x43, 0x44, 0x59, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
/* 5y */ 0x50, 0x51, 0x52, 0x53, 0,    0,    0,    0x57, 0x58, 0,    0,    0,    0,    0,    0,    0,
/* 6y */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 7y */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 8y */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 9y */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* Ay */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* By */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* Cy */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* Dy */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* Ey */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* Fy */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
};

static const int wxscantable[256] = {
/*         x0    x1    x2    x3    x4    x5    x6    x7    x8    x9    xA    xB    xC    xD    xE    xF */
/* 0y */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 1y */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0x9c, 0x9d, 0,    0,
/* 2y */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 3y */ 0,    0,    0,    0,    0,    0xb5, 0,    0,    0xb8, 0,    0,    0,    0,    0xb8, 0,    0,
/* 4y */ 0,    0,    0,    0,    0,    0x45, 0,    0xc7, 0xc8, 0xc9, 0,    0xcb, 0,    0xcd, 0,    0xcf,
/* 5y */ 0xd0, 0xd1, 0xd2, 0xd3, 0,    0,    0,    0,    0,    0,    0,    0x5b, 0x5c, 0x5d, 0,    0,
/* 6y */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 7y */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 8y */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 9y */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0x9d, 0,    0,
/* Ay */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* By */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* Cy */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* Dy */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* Ey */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* Fy */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
};


//
// WndProcCallback() -- the Windows window callback
//
static LRESULT CALLBACK WndProcCallback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#if USE_OPENGL
	if (hGLWindow && hWnd == hGLWindow) return DefWindowProc(hWnd,uMsg,wParam,lParam);
	if (dummyhGLwindow && hWnd == dummyhGLwindow) return DefWindowProc(hWnd,uMsg,wParam,lParam);
#endif

	switch (uMsg) {
		case WM_SYSCOMMAND:
			// don't let the monitor fall asleep or let the screensaver activate
			if (wParam == SC_SCREENSAVE || wParam == SC_MONITORPOWER) return 0;

			// Since DirectInput won't let me set an exclusive-foreground
			// keyboard for some unknown reason I just have to tell Windows to
			// rack off with its keyboard accelerators.
			if (wParam == SC_KEYMENU || wParam == SC_HOTKEY) return 0;
			break;

		case WM_ACTIVATEAPP:
			appactive = wParam;
			if (backgroundidle)
				SetPriorityClass( GetCurrentProcess(),
					appactive ? NORMAL_PRIORITY_CLASS : IDLE_PRIORITY_CLASS );
			break;

		case WM_ACTIVATE:
//			AcquireInputDevices(appactive);
			constrainmouse(LOWORD(wParam) != WA_INACTIVE && HIWORD(wParam) == 0);
			break;

		case WM_SIZE:
			if (wParam == SIZE_MAXHIDE || wParam == SIZE_MINIMIZED) appactive = 0;
			else appactive = 1;
//			AcquireInputDevices(appactive);
			break;

		case WM_PAINT:
			break;

			// don't draw the frame if fullscreen
		//case WM_NCPAINT:
			//if (!fullscreen) break;
			//return 0;

		case WM_ERASEBKGND:
			break;//return TRUE;

		case WM_MOVE:
			windowposx = LOWORD(lParam);
			windowposy = HIWORD(lParam);
			return 0;

		case WM_CLOSE:
			quitevent = 1;
			return 0;

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
			{
				int press = (lParam & 0x80000000l) == 0, held = (lParam & 0x40000000) != 0;
				int wscan = (lParam >> 16) & 0xff;
				int scan = 0;

				if (lParam & (1<<24)) {
					scan = wxscantable[wscan];
				} else {
					scan = wscantable[wscan];
				}

				//buildprintf("VK %-2x VSC %8x scan %-2x = %s\n", wParam, (UINT)lParam, scan, keynames[scan]);

				if (scan == 0) {
					// Not a key we want, so give it to the OS to handle.
					break;
				} else if (scan == OSD_CaptureKey(-1)) {
					if (press) {
						OSD_ShowDisplay(-1);
						eatosdinput = 1;
					}
				} else if (OSD_HandleKey(scan, press) != 0) {
					if (press) {
						if (!keystatus[scan] && !held) keystatus[scan] = 1;
						keyfifo[keyfifoend] = scan;
						keyfifo[(keyfifoend+1)&(KEYFIFOSIZ-1)] = 1;
						keyfifoend = ((keyfifoend+2)&(KEYFIFOSIZ-1));
					} else {
						keystatus[scan] = 0;
						keyfifo[keyfifoend] = scan;
						keyfifo[(keyfifoend+1)&(KEYFIFOSIZ-1)] = 0;
						keyfifoend = ((keyfifoend+2)&(KEYFIFOSIZ-1));
					}
				}
			}
			return 0;

		case WM_CHAR:
			if (eatosdinput) {
				eatosdinput = 0;
			} else if (OSD_HandleChar((unsigned char)wParam)) {
				if (((keyasciififoend+1)&(KEYFIFOSIZ-1)) != keyasciififoplc) {
					keyasciififo[keyasciififoend] = (unsigned char)wParam;
					keyasciififoend = ((keyasciififoend+1)&(KEYFIFOSIZ-1));
					//buildprintf("Char %d, %d-%d\n",wParam,keyasciififoplc,keyasciififoend);
				}
			}
			return 0;

		case WM_HOTKEY:
			return 0;

		case WM_INPUT:
			{
				RAWINPUT raw;
				UINT dwSize = sizeof(RAWINPUT);

				GetRawInputData((HRAWINPUT)lParam, RID_INPUT, (LPVOID)&raw, &dwSize, sizeof(RAWINPUTHEADER));

				if (raw.header.dwType == RIM_TYPEMOUSE) {
					int but;

					if (!mousegrab) {
						return 0;
					}

					for (but = 0; but < 4; but++) {  // Sorry XBUTTON2, I didn't plan for you.
						switch ((raw.data.mouse.usButtonFlags >> (but << 1)) & 3) {
							case 1:		// press
								mouseb |= (1 << but);
								break;
							case 2:		// release
								mouseb &= ~(1 << but);
								break;
							default: break;	// no change
						}
					}

					if (raw.data.mouse.usButtonFlags & RI_MOUSE_WHEEL) {
						int direction = (short)raw.data.mouse.usButtonData < 0;	// 1 = down (-ve values), 0 = up

						// Repeated events before the fake button release timer
						// expires need to trigger a release and a new press.
						mousewheel[direction] = getticks();
						mouseb |= (16 << direction);
					}

					if (raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) {
						static LONG absx = 0, absy = 0;
						static char first = 1;

						if (!first) {
							mousex += raw.data.mouse.lLastX - absx;
							mousey += raw.data.mouse.lLastY - absy;
						} else {
							first = 0;
						}
						absx = raw.data.mouse.lLastX;
						absy = raw.data.mouse.lLastY;
					} else {
						mousex += raw.data.mouse.lLastX;
						mousey += raw.data.mouse.lLastY;
					}
				}
			}
			return 0;

		case WM_ENTERMENULOOP:
		case WM_ENTERSIZEMOVE:
//			AcquireInputDevices(0);
			return 0;
		case WM_EXITMENULOOP:
		case WM_EXITSIZEMOVE:
//			AcquireInputDevices(1);
			return 0;

		case WM_DESTROY:
			hWindow = 0;
			//PostQuitMessage(0);	// JBF 20040115: not anymore
			return 0;

		default:
			break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


//
// RegisterWindowClass() -- register the window class
//
static BOOL RegisterWindowClass(void)
{
	WNDCLASSEX wcx;

	if (window_class_registered) return FALSE;

	//buildputs("Registering window class\n");

	wcx.cbSize	= sizeof(wcx);
	wcx.style	= CS_OWNDC;
	wcx.lpfnWndProc	= WndProcCallback;
	wcx.cbClsExtra	= 0;
	wcx.cbWndExtra	= 0;
	wcx.hInstance	= hInstance;
	wcx.hIcon	= LoadImage(hInstance, MAKEINTRESOURCE(100), IMAGE_ICON,
				GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
	wcx.hCursor	= LoadCursor(NULL, IDC_ARROW);
	wcx.hbrBackground = CreateSolidBrush(RGB(0,0,0));
	wcx.lpszMenuName = NULL;
	wcx.lpszClassName = WINDOW_CLASS;
	wcx.hIconSm	= LoadImage(hInstance, MAKEINTRESOURCE(100), IMAGE_ICON,
				GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	if (!RegisterClassEx(&wcx)) {
		ShowErrorBox("Failed to register window class");
		return TRUE;
	}

	window_class_registered = TRUE;

	return FALSE;
}


//
// GetWindowsErrorMsg() -- gives a pointer to a static buffer containing the Windows error message
//
static LPTSTR GetWindowsErrorMsg(DWORD code)
{
	static TCHAR lpMsgBuf[1024];

	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, code,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)lpMsgBuf, 1024, NULL);

	return lpMsgBuf;
}

static const char *getwindowserrorstr(DWORD code)
{
	static char msg[1024];
	memset(msg, 0, sizeof(msg));
	OemToCharBuff(GetWindowsErrorMsg(code), msg, sizeof(msg)-1);
	return msg;
}
