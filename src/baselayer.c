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
unsigned maxrefreshfreq = 0;
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
// resetvideomode() -- forces a reset of the video mode at next setgamemode() call
//
void resetvideomode(void)
{
	videomodereset = 1;
	validmodecnt = 0; // Re-enumerate valid modes.
	validmodesetcnt = 0;
}


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

void addvalidmode(int w, int h, int bpp, int fs, int display, unsigned refresh, int extra)
{
	int m;

	if (validmodecnt>=MAXVALIDMODES) return;
	if ((w > MAXXDIM) || (h > MAXYDIM)) return;
	if (!(fs&255)) display=0, refresh=0; // Windowed modes don't declare a display or refresh rate.
	if (maxrefreshfreq > 0 && refresh > maxrefreshfreq) return;

	// Check for a duplicate.
	for (m=0; m<validmodecnt; m++)
		if (validmode[m].xdim==w && validmode[m].ydim==h &&
				validmode[m].bpp==bpp && validmode[m].fs==fs &&
				validmode[m].display==display)
		{
			if (maxrefreshfreq > 0 && refresh > maxrefreshfreq) return;
			if (refresh > validmode[m].refresh) {
				validmode[m].refresh = refresh;
				validmode[m].extra = extra;
			}
			return;
		}

	validmode[validmodecnt].xdim=w;
	validmode[validmodecnt].ydim=h;
	validmode[validmodecnt].bpp=bpp;
	validmode[validmodecnt].fs=fs;
	validmode[validmodecnt].display=display;
	validmode[validmodecnt].refresh=refresh;
	validmode[validmodecnt].extra=extra;
	validmode[validmodecnt].validmodeset=-1;
	validmodecnt++;
}

void addstandardvalidmodes(int maxx, int maxy, int bpp, int fs, int display, unsigned refresh, int extra)
{
	static const int defaultres[][2] = {
		{1920,1200}, {1920,1080}, {1600,1200}, {1680,1050}, {1600,900},
		{1400,1050}, {1440,900},  {1366,768},  {1280,1024}, {1280,960},
		{1280,800},  {1280,720},  {1152,864},  {1024,768},  {800,600},
		{640,480},   {640,400},   {320,240},   {320,200},   {0,0}
	};
	for (int i=0; defaultres[i][0]; i++) {
		if (defaultres[i][0] <= maxx && defaultres[i][1] <= maxy)
			addvalidmode(defaultres[i][0], defaultres[i][1], bpp, fs, display, refresh, extra);
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
			debugprintf("  - %dx%d %d-bit fullscreen, display %d, %u Hz\n",
				validmode[i].xdim, validmode[i].ydim, validmode[i].bpp,
				validmode[i].display, validmode[i].refresh);
		} else {
			debugprintf("  - %dx%d %d-bit windowed\n",
				validmode[i].xdim, validmode[i].ydim, validmode[i].bpp);
		}

	}
#endif

	int i, vs, bppvs, nextfsvs, bestnextfsvs, prevfsvs, bestprevfsvs;

	vs = -1;
	for (i = 0; i < validmodecnt && vs < MAXVALIDMODESETS-1; i++) {
		if (vs < 0 ||
				validmode[i].fs != validmodeset[vs].fs ||
				validmode[i].display != validmodeset[vs].display ||
				validmode[i].bpp != validmodeset[vs].bpp) {
			++vs;
			validmodeset[vs].fs = validmode[i].fs;
			validmodeset[vs].display = validmode[i].display;
			validmodeset[vs].bpp = validmode[i].bpp;

			validmodeset[vs].firstmode = validmodeset[vs].lastmode = i;
			validmodeset[vs].nextbppmodeset = validmodeset[vs].prevbppmodeset = -1;
			validmodeset[vs].nextfsmodeset = validmodeset[vs].prevfsmodeset = -1;
		} else {
			validmodeset[vs].lastmode = i;
		}
		validmode[i].validmodeset = (short)vs;
	}
	validmodesetcnt = vs + 1;

	bppvs = 0;
	for (i = 0; i < validmodesetcnt; i++) {
		// Find the next/prev change of bpp with a perfect match on display/fs.
		if (validmodeset[bppvs].fs == validmodeset[i].fs &&
				validmodeset[bppvs].display == validmodeset[i].display) {
			if (validmodeset[bppvs].bpp != validmodeset[i].bpp) {
				validmodeset[bppvs].nextbppmodeset = i;
				validmodeset[i].prevbppmodeset = bppvs;
				bppvs = i;
			}
		} else {
			bppvs = i;
		}

		// Find the next/prev change of display/fs with a perfect match on bpp whilst keeping track of the closest bpp match.
		bestnextfsvs = nextfsvs = bestprevfsvs = prevfsvs = -1;
		for (vs=1; nextfsvs<0 && prevfsvs<0 && vs<validmodesetcnt; vs++) {
			if (i+vs<validmodesetcnt && (validmodeset[i+vs].fs != validmodeset[i].fs ||
					validmodeset[i+vs].display != validmodeset[i].display)) {
				if (validmodeset[i+vs].bpp == validmodeset[i].bpp)
					nextfsvs = i+vs;
				else if (bestnextfsvs < 0 || (validmodeset[i+vs].bpp - validmodeset[i].bpp <
						validmodeset[bestnextfsvs].bpp - validmodeset[i].bpp))
					bestnextfsvs = i+vs;
			}
			if (i-vs>=0 && (validmodeset[i-vs].fs != validmodeset[i].fs ||
					validmodeset[i-vs].display != validmodeset[i].display)) {
				if (validmodeset[i-vs].bpp == validmodeset[i].bpp)
					prevfsvs = i-vs;
				else if (bestprevfsvs < 0 || (validmodeset[i-vs].bpp - validmodeset[i].bpp <
						validmodeset[bestprevfsvs].bpp - validmodeset[i].bpp))
					bestprevfsvs = i-vs;
			}
		}
		if (nextfsvs >= 0) validmodeset[i].nextfsmodeset = nextfsvs;
		else if (bestnextfsvs >= 0) validmodeset[i].nextfsmodeset = bestnextfsvs;
		if (prevfsvs >= 0) validmodeset[i].prevfsmodeset = prevfsvs;
		else if (bestprevfsvs >= 0) validmodeset[i].prevfsmodeset = bestprevfsvs;
	}

#ifdef DEBUGGINGAIDS
	debugprintf("Video mode sets:\n");
	for (int i=0; i<validmodesetcnt; i++) {
		if (validmodeset[i].fs) {
			debugprintf("%d) %d-bit fullscreen, display %d\n", i, validmodeset[i].bpp, validmodeset[i].display);
		} else {
			debugprintf("%d) %d-bit windowed\n", i, validmodeset[i].bpp);
		}
		debugprintf("   mode: first %d, last %d\n", validmodeset[i].firstmode, validmodeset[i].lastmode);
		debugprintf("   bpp set: next %d, prev %d\n", validmodeset[i].nextbppmodeset, validmodeset[i].prevbppmodeset);
		debugprintf("   fs-display set: next %d, prev %d\n", validmodeset[i].nextfsmodeset, validmodeset[i].prevfsmodeset);
	}
#endif
}

//
// findvideomode() -- identify the next/previous mode in the valid modes list
//
int findvideomode(int search, int refmode, int dir)
{
	int vs, mode, last, foundmode, txdim, tydim;
	int steps = abs(dir), incr = dir < 0 ? -1 : 1;

	if (refmode < 0) {
		txdim = xdim; tydim = ydim;
		refmode = checkvideomode(&txdim, &tydim, bpp, fullscreen, 1);
	}
	if ((unsigned)refmode >= (unsigned)validmodecnt) return -1;

	foundmode = refmode;
	vs = validmode[refmode].validmodeset;

	switch (search) {
		case FINDVIDEOMODE_SEARCH_ANY:
			mode = refmode + dir;
			if (mode < 0) foundmode = 0;
			else if (mode >= validmodecnt) foundmode = validmodecnt-1;
			else foundmode = mode;
			break;
		case FINDVIDEOMODE_SEARCH_XYDIM:
			for (mode = refmode + incr;
				steps && mode >= validmodeset[vs].firstmode && mode <= validmodeset[vs].lastmode;
				steps--, mode += incr) {
				foundmode = mode;
			}
			break;
		case FINDVIDEOMODE_SEARCH_BITSPP:
		case FINDVIDEOMODE_SEARCH_FULLSC:
			for (last = vs; steps && vs >= 0; steps--) {
				last = vs;
				if (search == FINDVIDEOMODE_SEARCH_BITSPP) {
					if (incr < 0) vs = validmodeset[vs].prevbppmodeset;
					else vs = validmodeset[vs].nextbppmodeset;
				} else {
					if (incr < 0) vs = validmodeset[vs].prevfsmodeset;
					else vs = validmodeset[vs].nextfsmodeset;
				}
			}
			if (vs < 0) vs = last;

			txdim = validmode[refmode].xdim; tydim = validmode[refmode].ydim;
			mode = checkvideomode(&txdim, &tydim, validmodeset[vs].bpp,
				SETGAMEMODE_FULLSCREEN(validmodeset[vs].display, validmodeset[vs].fs), 1);
			if (mode >= 0) foundmode = mode;
			break;
	}

	return foundmode;
}

void setmaxrefreshfreq(unsigned frequency)
{
	maxrefreshfreq = frequency;
}

unsigned getmaxrefreshfreq(void)
{
	return maxrefreshfreq;
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
	else if (!Bstrcasecmp(parm->name, "maxrefreshfreq")) {
		if (showval) {
			buildprintf("maxrefreshfreq is %u%s\n", maxrefreshfreq, maxrefreshfreq ? " Hz" : " (no maximum)");
		} else {
			int freq = atoi(parm->parms[0]);
			if (freq < 0) freq = 0;
			maxrefreshfreq = (unsigned)freq;
			validmodecnt = 0; // Re-enumerate valid modes.
			validmodesetcnt = 0;
		}
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
		const char *str;
		if (filterbpp >= 0 && validmode[i].bpp != filterbpp) continue;
		if (filterfs >= 0 && validmode[i].fs != filterfs) continue;
		if (validmode[i].fs && validmode[i].refresh) str = "%2d) %4dx%-4d %d-bit fullscreen, display %d, %u Hz\n";
		else if (validmode[i].fs) str = "%2d) %4dx%-4d %d-bit fullscreen, display %d\n";
		else str = "%2d) %4dx%-4d %d-bit windowed\n";
		buildprintf(str, i, validmode[i].xdim, validmode[i].ydim, validmode[i].bpp,
			validmode[i].display, validmode[i].refresh);
		found++;
	}
	buildprintf("%d modes identified\n", found);
	return OSDCMD_OK;
}

#if defined(DEBUGGINGAIDS)
static int osdcmd_findvideomode(const osdfuncparm_t *parm)
{
	int mode;

	if (parm->numparms != 3) return OSDCMD_SHOWHELP;
	mode = findvideomode(atoi(parm->parms[0]), atoi(parm->parms[1]), atoi(parm->parms[2]));
	if (mode >= 0) {
		if (validmode[mode].fs) {
			buildprintf("mode %d: %dx%d %d-bit fullscreen, display %d, %u Hz\n", mode,
				validmode[mode].xdim, validmode[mode].ydim, validmode[mode].bpp,
				validmode[mode].display, validmode[mode].refresh);
		} else {
			buildprintf("mode %d: %dx%d %d-bit windowed\n", mode,
				validmode[mode].xdim, validmode[mode].ydim, validmode[mode].bpp);
		}
	} else {
		buildputs("No mode found\n");
	}
	return OSDCMD_OK;
}
#endif

int baselayer_init(void)
{
    OSD_Init();

	OSD_RegisterFunction("screencaptureformat","screencaptureformat: sets the output format for screenshots (TGA, PCX, PNG)",osdcmd_vars);

	OSD_RegisterFunction("novoxmips","novoxmips: turn off/on the use of mipmaps when rendering 8-bit voxels",osdcmd_vars);
	OSD_RegisterFunction("usevoxels","usevoxels: enable/disable automatic sprite->voxel rendering",osdcmd_vars);
	OSD_RegisterFunction("usegammabrightness","usegammabrightness: set brightness using system gamma (2), shader (1), or palette (0)",osdcmd_vars);
	OSD_RegisterFunction("maxrefreshfreq", "maxrefreshfreq: maximum display frequency to set for fullscreen modes (0=no maximum)", osdcmd_vars);
	OSD_RegisterFunction("listvidmodes","listvidmodes [<bpp>|win|fs] / listvidmodes displays: show all available video mode combinations",osdcmd_listvidmodes);
#ifdef DEBUGGINGAIDS
	OSD_RegisterFunction("findvideomode","findvideomode [search] [refmode] [dir]: explore video mode. search: 0 res, 1 bpp, 2 display/fs. refmode: -1 current. dir: 1 forward, 2 back.",osdcmd_findvideomode);
#endif

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

