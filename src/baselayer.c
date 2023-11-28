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
int checkvideomode(int *x, int *y, int c, int fs, int forced)
{
	int i, nearest=-1, dx, dy, odx=INT_MAX, ody=INT_MAX;

	getvalidmodes();

#if USE_OPENGL
	if (c > 8 && glunavailable) return -1;
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
		dx = abs(validmode[i].xdim - *x);
		dy = abs(validmode[i].ydim - *y);
		if (!(dx | dy)) { 	// perfect match
			nearest = i;
			break;
		}
		if ((dx <= odx) && (dy <= ody)) {
			nearest = i;
			odx = dx; ody = dy;
		}
	}

#ifdef ANY_WINDOWED_SIZE
	if (!forced && (fs&1) == 0 && (nearest < 0 || validmode[nearest].xdim!=*x || validmode[nearest].ydim!=*y)) {
		// check the colour depth is recognised at the very least
		for (i=0;i<validmodecnt;i++)
			if (validmode[i].bpp == c)
				return 0x7fffffffl;
		return -1;	// strange colour depth
	}
#endif

	if (nearest < 0) {
		// no mode that will match (eg. if no fullscreen modes)
		return -1;
	}

	*x = validmode[nearest].xdim;
	*y = validmode[nearest].ydim;

	return nearest;		// JBF 20031206: Returns the mode number
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
	return OSDCMD_SHOWHELP;
}

int baselayer_init(void)
{
    OSD_Init();

	OSD_RegisterFunction("screencaptureformat","screencaptureformat: sets the output format for screenshots (TGA, PCX, PNG)",osdcmd_vars);

	OSD_RegisterFunction("novoxmips","novoxmips: turn off/on the use of mipmaps when rendering 8-bit voxels",osdcmd_vars);
	OSD_RegisterFunction("usevoxels","usevoxels: enable/disable automatic sprite->voxel rendering",osdcmd_vars);

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

