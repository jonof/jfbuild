#include "compat.h"
#include "osd.h"
#include "build.h"
#include "engineinfo.h"
#include "baselayer.h"

#ifdef RENDERTYPEWIN
#include "winlayer.h"
#endif

static int osdfunc_dumpbuildinfo(const osdfuncparm_t *parm)
{
	OSD_Printf(
		"Build engine compilation:\n"
		"  CFLAGS: %s\n"
		"  LIBS: %s\n"
		"  Host: %s\n"
		"  Compiler: %s\n"
		"  Built: %s\n",
		_engine_cflags,
		_engine_libs,
		_engine_uname,
		_engine_compiler,
		_engine_date);

	return OSDCMD_OK;
}

static void onvideomodechange(int newmode) { }
void (*baselayer_onvideomodechange)(int) = onvideomodechange;

static int osdfunc_setrendermode(const osdfuncparm_t *parm)
{
	int m;
	char *p;

	char *modestrs[] = {
		"classic software", "polygonal flat-shaded software",
		"polygonal textured software", "polygonal OpenGL"
	};

	if (parm->numparms != 1) return OSDCMD_SHOWHELP;
	m = Bstrtol(parm->parms[0], &p, 10);

	if (m < 0 || m > 3) return OSDCMD_SHOWHELP;

	setrendermode(m);
	OSD_Printf("Rendering method changed to %s\n", modestrs[ getrendermode() ] );

	return OSDCMD_OK;
}

#ifdef DEBUGGINGAIDS
#if defined(POLYMOST) && defined(USE_OPENGL)
static int osdcmd_hicsetpalettetint(const osdfuncparm_t *parm)
{
	long pal, cols[3], eff;
	char *p;

	if (parm->numparms != 5) return OSDCMD_SHOWHELP;

	pal = Batol(parm->parms[0]);
	cols[0] = Batol(parm->parms[1]);
	cols[1] = Batol(parm->parms[2]);
	cols[2] = Batol(parm->parms[3]);
	eff = Batol(parm->parms[4]);

	hicsetpalettetint(pal,cols[0],cols[1],cols[2],eff);

	return OSDCMD_OK;
}
#endif
#endif

#if defined(POLYMOST) && defined(USE_OPENGL)
static int osdcmd_glinfo(const osdfuncparm_t *parm)
{
	char *s,*t,*u,i;
	
	if (bpp == 8) {
		OSD_Printf("glinfo: Not in OpenGL mode.\n");
		return OSDCMD_OK;
	}
	
	OSD_Printf("OpenGL Information:\n"
	           " Version:  %s\n"
		   " Vendor:   %s\n"
		   " Renderer: %s\n"
		   " Maximum anisotropy:  %.1f%s\n"
		   " BGRA textures:       %s\n"
		   " Texure compression:  %s\n"
		   " Clamp-to-edge:       %s\n"
		   " Extensions:\n",
		   	glinfo.version,
			glinfo.vendor,
			glinfo.renderer,
			glinfo.maxanisotropy, glinfo.maxanisotropy>1.0?"":" (no anisotropic filtering)",
			glinfo.bgra ? "supported": "not supported",
			glinfo.texcompr ? "supported": "not supported",
			glinfo.clamptoedge ? "supported": "not supported"
		);

	s = Bstrdup(glinfo.extensions);
	if (!s) OSD_Printf(glinfo.extensions);
	else {
		i = 0; t = u = s;
		while (*t) {
			if (*t == ' ') {
				if (i&1) {
					*t = 0;
					OSD_Printf("   %s\n",u);
					u = t+1;
				}
				i++;
			}
			t++;
		}
		if (i&1) OSD_Printf("   %s\n",u);
		Bfree(s);
	}
	
	if (s) Bfree(s);
	return OSDCMD_OK;
}

#endif

int baselayer_init(void)
{
#ifdef POLYMOST
	OSD_RegisterFunction("setrendermode","setrendermode <number>: sets the engine's rendering mode.\n"
			"Mode numbers are:\n"
			"   0 - Classic Build software\n"
			"   1 - Polygonal flat-shaded software\n"
			"   2 - Polygonal textured software\n"
#ifdef USE_OPENGL
			"   3 - Polygonal OpenGL\n"
#endif
			,
			osdfunc_setrendermode);
#endif
	OSD_RegisterFunction("dumpbuildinfo","dumpbuildinfo: outputs engine compilation information",osdfunc_dumpbuildinfo);
	OSD_RegisterVariable("screencaptureformat", OSDVAR_INTEGER, &captureformat, 0, osd_internal_validate_integer);
#ifdef SUPERBUILD
	OSD_RegisterVariable("novoxmips", OSDVAR_INTEGER, &novoxmips, 0, osd_internal_validate_boolean);
	OSD_RegisterVariable("usevoxels", OSDVAR_INTEGER, &usevoxels, 0, osd_internal_validate_boolean);
#endif
#ifdef DEBUGGINGAIDS
#if defined(POLYMOST) && defined(USE_OPENGL)
	OSD_RegisterFunction("hicsetpalettetint","hicsetpalettetint: sets palette tinting values",osdcmd_hicsetpalettetint);
#endif
#endif
#if defined(POLYMOST) && defined(USE_OPENGL)
	OSD_RegisterFunction("glinfo","glinfo: shows OpenGL information about the current OpenGL mode",osdcmd_glinfo);
#endif
#if defined(RENDERTYPEWIN)
	OSD_RegisterVariable("glusecds", OSDVAR_INTEGER, &glusecds, 0, osd_internal_validate_boolean);
#endif
	
	return 0;
}

