// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#include "build.h"
#include "osd.h"
#include "baselayer.h"
#include "baselayer_priv.h"

#ifdef RENDERTYPEWIN
#include "winlayer.h"
#endif

#if USE_OPENGL
#include "glbuild.h"
struct glbuild_info glinfo;
int glswapinterval = 1;
int glunavailable;
#endif //USE_OPENGL

int   _buildargc = 0;
const char **_buildargv = NULL;

char quitevent=0, appactive=1;

int xres=-1, yres=-1, bpp=0, fullscreen=0, bytesperline, imageSize;
intptr_t frameplace=0;
char offscreenrendering=0;
char videomodereset = 0;
void (*baselayer_videomodewillchange)(void) = NULL;
void (*baselayer_videomodedidchange)(void) = NULL;

int inputdevices=0;

// keys
char keystatus[256];
int keyfifo[KEYFIFOSIZ];
unsigned char keyasciififo[KEYFIFOSIZ];
int keyfifoplc, keyfifoend;
int keyasciififoplc, keyasciififoend;

// mouse
int mousex=0, mousey=0, mouseb=0;

// joystick
int joyaxis[8], joyb=0;
char joynumaxes=0, joynumbuttons=0;



//
// checkvideomode() -- makes sure the video mode passed is legal
//
int checkvideomode(int *xdim, int *ydim, int bitspp, int fullsc, int strict)
{
	int i, display, nearest=-1, dx, dy, darea, odx=INT_MAX, ody=INT_MAX, odarea=INT_MAX;

	getvalidmodes();

#if USE_OPENGL
	if (bitspp > 8 && glunavailable) return -1;
#else
	if (bitspp > 8) return -1;
#endif

	// fix up the passed resolution values to be multiples of 8
	// and at least 320x200 or at most MAXXDIMxMAXYDIM
	if (*xdim < 320) *xdim = 320;
	if (*ydim < 200) *ydim = 200;
	if (*xdim > MAXXDIM) *xdim = MAXXDIM;
	if (*ydim > MAXYDIM) *ydim = MAXYDIM;
	*xdim &= 0xfffffff8l;
	display = (fullsc&255) ? (fullsc>>8) : 0;
	fullsc &= 255;

	for (i=0; i<validmodecnt; i++) {
		if (validmode[i].bpp != bitspp) continue;
		if (validmode[i].fs != fullsc) continue;
		if (validmode[i].display != display) continue;
		dx = abs(validmode[i].xdim - *xdim);
		dy = abs(validmode[i].ydim - *ydim);
		darea = dx*dy;
		if (!(dx | dy)) { 	// perfect match
			nearest = i;
			break;
		}
		if (((dx <= odx) && (dy <= ody)) || (darea < odarea)) {
			nearest = i;
			odx = dx; ody = dy; odarea = darea;
		}
	}

	if (!strict && !fullsc && (nearest < 0 || validmode[nearest].xdim!=*xdim ||
			validmode[nearest].ydim!=*ydim)) {
		// check the colour depth is recognised at the very least
		for (i=0;i<validmodecnt;i++)
			if (validmode[i].bpp == bitspp && validmode[i].display == display)
				return VIDEOMODE_RELAXED;
		return -1;	// strange colour depth
	}

	if (nearest < 0) {
		// no mode that will match (eg. if no fullscreen modes)
		return -1;
	}

	*xdim = validmode[nearest].xdim;
	*ydim = validmode[nearest].ydim;

	return nearest;		// JBF 20031206: Returns the mode number
}

void addvalidmode(int w, int h, int bpp, int fs, int display, int extra)
{
	int m;

	if (validmodecnt>=MAXVALIDMODES) return;
	if ((w > MAXXDIM) || (h > MAXYDIM)) return;
	if (!(fs&255)) display=0; // Windowed modes don't declare a display.

	// Check for a duplicate.
	for (m=0; m<validmodecnt; m++)
		if (validmode[m].xdim==w && validmode[m].ydim==h &&
				validmode[m].bpp==bpp && validmode[m].fs==fs &&
				validmode[m].display==display)
			return;

	validmode[validmodecnt].xdim=w;
	validmode[validmodecnt].ydim=h;
	validmode[validmodecnt].bpp=bpp;
	validmode[validmodecnt].fs=fs;
	validmode[validmodecnt].display=display;
	validmode[validmodecnt].extra=extra;
	validmodecnt++;
}

void addstandardvalidmodes(int maxx, int maxy, int bpp, int fs, int display, int extra)
{
	static const int defaultres[][2] = {
		{1920,1200}, {1920,1080}, {1600,1200}, {1680,1050}, {1600,900},
		{1400,1050}, {1440,900},  {1366,768},  {1280,1024}, {1280,960},
		{1280,800},  {1280,720},  {1152,864},  {1024,768},  {800,600},
		{640,480},   {640,400},   {320,240},   {320,200},   {0,0}
	};
	for (int i=0; defaultres[i][0]; i++) {
		if (defaultres[i][0] <= maxx && defaultres[i][1] <= maxy)
			addvalidmode(defaultres[i][0], defaultres[i][1], bpp, fs, display, extra);
	}
}

static int sortmodes(const struct validmode_t *a, const struct validmode_t *b)
{
	int x;

	if ((x = a->fs   - b->fs)   != 0) return x;
	if ((x = a->display - b->display) != 0) return x;
	if ((x = a->bpp  - b->bpp)  != 0) return x;
	if ((x = a->xdim - b->xdim) != 0) return x;
	if ((x = a->ydim - b->ydim) != 0) return x;

	return 0;
}

void sortvalidmodes(void)
{
	qsort((void*)validmode, validmodecnt, sizeof(struct validmode_t), (int(*)(const void*,const void*))sortmodes);

#ifdef DEBUGGINGAIDS
	debugprintf("Video modes available:\n");
	for (int i=0; i<validmodecnt; i++) {
		if (validmode[i].fs) {
			debugprintf("  - %dx%d %d-bit fullscreen, display %d\n",
				validmode[i].xdim, validmode[i].ydim,
				validmode[i].bpp, validmode[i].display);
		} else {
			debugprintf("  - %dx%d %d-bit windowed\n",
				validmode[i].xdim, validmode[i].ydim, validmode[i].bpp);
		}

	}
#endif
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

int bgetkey(void)
{
	int c;
	if (keyfifoplc == keyfifoend) return 0;
	c = keyfifo[keyfifoplc];
	if (!keyfifo[(keyfifoplc+1)&(KEYFIFOSIZ-1)]) c = -c;
	keyfifoplc = (keyfifoplc+2)&(KEYFIFOSIZ-1);
	return c;
}

int bkeyhit(void)
{
	return (keyfifoplc != keyfifoend);
}

void bflushkeys(void)
{
	keyfifoplc = keyfifoend = 0;
}


#if USE_POLYMOST
static int osdfunc_setrendermode(const osdfuncparm_t *parm)
{
	int m;
	char *p;

	char *modestrs[] = {
		"classic software", "polygonal flat-shaded software",
		"polygonal textured software", "polygonal OpenGL"
	};

	if (parm->numparms != 1) return OSDCMD_SHOWHELP;
	m = (int)strtol(parm->parms[0], &p, 10);

	if (m < 0 || m > 3 || *p) return OSDCMD_SHOWHELP;

	setrendermode(m);
	buildprintf("Rendering method changed to %s\n", modestrs[ getrendermode() ] );

	return OSDCMD_OK;
}
#endif //USE_POLYMOST

#if defined(DEBUGGINGAIDS) && USE_POLYMOST && USE_OPENGL
static int osdcmd_hicsetpalettetint(const osdfuncparm_t *parm)
{
	int pal, cols[3], eff;

	if (parm->numparms != 5) return OSDCMD_SHOWHELP;

	pal = atoi(parm->parms[0]);
	cols[0] = atoi(parm->parms[1]);
	cols[1] = atoi(parm->parms[2]);
	cols[2] = atoi(parm->parms[3]);
	eff = atoi(parm->parms[4]);

	hicsetpalettetint(pal,cols[0],cols[1],cols[2],eff);

	return OSDCMD_OK;
}
#endif //DEBUGGINGAIDS && USE_POLYMOST && USE_OPENGL

static int osdcmd_vars(const osdfuncparm_t *parm)
{
	int showval = (parm->numparms < 1);

	if (!Bstrcasecmp(parm->name, "screencaptureformat")) {
		const char *fmts[3] = { "TGA", "PCX", "PNG" };
		if (!showval) {
			int i;
			for (i=0; i<3; i++)
				if (!Bstrcasecmp(parm->parms[0], fmts[i]) || atoi(parm->parms[0]) == i) {
					captureformat = i;
					break;
				}
			if (i == 3) return OSDCMD_SHOWHELP;
		}
		buildprintf("screencaptureformat is %s\n", fmts[captureformat]);
		return OSDCMD_OK;
	}
	else if (!Bstrcasecmp(parm->name, "novoxmips")) {
		if (showval) { buildprintf("novoxmips is %d\n", novoxmips); }
		else { novoxmips = (atoi(parm->parms[0]) != 0); }
		return OSDCMD_OK;
	}
	else if (!Bstrcasecmp(parm->name, "usevoxels")) {
		if (showval) { buildprintf("usevoxels is %d\n", usevoxels); }
		else { usevoxels = (atoi(parm->parms[0]) != 0); }
		return OSDCMD_OK;
	}
	else if (!Bstrcasecmp(parm->name, "usegammabrightness")) {
		const char *types[3] = { "palette", "shader", "system" };
		if (!showval) {
			usegammabrightness = atoi(parm->parms[0]);
			usegammabrightness = min(max(0, usegammabrightness), 2);
#if USE_OPENGL
			if (usegammabrightness == 1 && glunavailable) usegammabrightness = 2;
#endif
		}
		buildprintf("usegammabrightness is %d (%s)\n", usegammabrightness, types[usegammabrightness]);
		return OSDCMD_OK;
	}
	return OSDCMD_SHOWHELP;
}

static int osdcmd_listvidmodes(const osdfuncparm_t *parm)
{
	int filterbpp = -1, filterfs = -1, found = 0;

	if (parm->numparms == 1 && strcasecmp(parm->parms[0], "displays") == 0) {
		for (int i = 0; i < displaycnt; i++) {
			buildprintf("Display %d: %s\n", i, getdisplayname(i));
		}
		return OSDCMD_OK;
	}
	for (int i = 0; i < parm->numparms; i++) {
		if (strcasecmp(parm->parms[i], "win") == 0) {
			filterfs = 0;
		} else if (strcasecmp(parm->parms[i], "fs") == 0) {
			filterfs = 1;
		} else {
			char *e = NULL;
			long l = strtol(parm->parms[i], &e, 10);
			if (!e || *e != 0) return OSDCMD_SHOWHELP;
			filterbpp = (int)l;
		}
	}

	for (int i = 0; i < validmodecnt; i++) {
		if (filterbpp >= 0 && validmode[i].bpp != filterbpp) continue;
		if (filterfs >= 0 && validmode[i].fs != filterfs) continue;
		if (validmode[i].fs) {
			buildprintf(" %4dx%-4d %d-bit fullscreen, display %d\n",
				validmode[i].xdim, validmode[i].ydim,
				validmode[i].bpp, validmode[i].display);
		} else {
			buildprintf(" %4dx%-4d %d-bit windowed\n",
				validmode[i].xdim, validmode[i].ydim,
				validmode[i].bpp);
		}
		found++;
	}
	buildprintf("%d modes identified\n", found);
	return OSDCMD_OK;
}

int baselayer_init(void)
{
    OSD_Init();

	OSD_RegisterFunction("screencaptureformat","screencaptureformat: sets the output format for screenshots (TGA, PCX, PNG)",osdcmd_vars);

	OSD_RegisterFunction("novoxmips","novoxmips: turn off/on the use of mipmaps when rendering 8-bit voxels",osdcmd_vars);
	OSD_RegisterFunction("usevoxels","usevoxels: enable/disable automatic sprite->voxel rendering",osdcmd_vars);
	OSD_RegisterFunction("usegammabrightness","usegammabrightness: set brightness using system gamma (2), shader (1), or palette (0)",osdcmd_vars);
	OSD_RegisterFunction("listvidmodes","listvidmodes [<bpp>|win|fs] / listvidmodes displays: show all available video mode combinations",osdcmd_listvidmodes);

#if USE_POLYMOST
	OSD_RegisterFunction("setrendermode","setrendermode <number>: sets the engine's rendering mode.\n"
			"Mode numbers are:\n"
			"   0 - Classic Build software\n"
			"   1 - Polygonal flat-shaded software\n"
			"   2 - Polygonal textured software\n"
#if USE_OPENGL
			"   3 - Polygonal OpenGL\n"
#endif
			,
			osdfunc_setrendermode);
#endif //USE_POLYMOST

#if defined(DEBUGGINGAIDS) && USE_POLYMOST && USE_OPENGL
	OSD_RegisterFunction("hicsetpalettetint","hicsetpalettetint: sets palette tinting values",osdcmd_hicsetpalettetint);
#endif

	return 0;
}

