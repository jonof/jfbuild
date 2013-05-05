#include "build.h"
#include "osd.h"
#include "baselayer.h"

#ifdef RENDERTYPEWIN
#include "winlayer.h"
#endif

#ifdef USE_OPENGL
#include "glbuild.h"

struct glinfo glinfo = {
	"Unknown",	// vendor
	"Unknown",	// renderer
	"0.0.0",	// version
	"",		// extensions
	
	1.0,		// max anisotropy
	0,		// brga texture format
	0,		// clamp-to-edge support
	0,		// texture compression
	0,		// non-power-of-two textures
	0,		// multisampling
	0,		// nvidia multisampling hint
	0,		// multitexturing
	0,		// envcombine
	
	0,		// no fog hack flag
};
#endif

static void onvideomodechange(int UNUSED(newmode)) { }
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
	buildprintf("Rendering method changed to %s\n", modestrs[ getrendermode() ] );

	return OSDCMD_OK;
}

#if defined(POLYMOST) && defined(USE_OPENGL)
#ifdef DEBUGGINGAIDS
static int osdcmd_hicsetpalettetint(const osdfuncparm_t *parm)
{
	int pal, cols[3], eff;
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

static int osdcmd_glinfo(const osdfuncparm_t *UNUSED(parm))
{
	char *s,*t,*u,i;
	
	if (bpp == 8) {
		buildputs("glinfo: Not in OpenGL mode.\n");
		return OSDCMD_OK;
	}
	
	buildprintf("OpenGL Information:\n"
	           " Version:  %s\n"
		   " Vendor:   %s\n"
		   " Renderer: %s\n"
		   " Maximum anisotropy:      %.1f%s\n"
		   " BGRA textures:           %s\n"
		   " Non-x^2 textures:        %s\n"
		   " Texure compression:      %s\n"
		   " Clamp-to-edge:           %s\n"
		   " Multisampling:           %s\n"
		   " Nvidia multisample hint: %s\n"
		   " Multitexturing:          %s\n"
		   " Env combine extension:   %s\n"
		   " Extensions:\n",
		   	glinfo.version,
			glinfo.vendor,
			glinfo.renderer,
			glinfo.maxanisotropy, glinfo.maxanisotropy>1.0?"":" (no anisotropic filtering)",
			glinfo.bgra ? "supported": "not supported",
			glinfo.texnpot ? "supported": "not supported",
			glinfo.texcompr ? "supported": "not supported",
			glinfo.clamptoedge ? "supported": "not supported",
			glinfo.multisample ? "supported": "not supported",
			glinfo.nvmultisamplehint ? "supported": "not supported",
			glinfo.multitex ? "supported": "not supported",
			glinfo.envcombine ? "supported": "not supported"
		);

	s = Bstrdup(glinfo.extensions);
	if (!s) buildputs(glinfo.extensions);
	else {
		i = 0; t = u = s;
		while (*t) {
			if (*t == ' ') {
				if (i&1) {
					*t = 0;
					buildprintf("   %s\n",u);
					u = t+1;
				}
				i++;
			}
			t++;
		}
		if (i&1) buildprintf("   %s\n",u);
		Bfree(s);
	}
	
	return OSDCMD_OK;
}
#endif

static int osdcmd_vars(const osdfuncparm_t *parm)
{
	int showval = (parm->numparms < 1);
	
	if (!Bstrcasecmp(parm->name, "screencaptureformat")) {
		const char *fmts[2][2] = { {"TGA", "PCX"}, {"0", "1"} };
		if (showval) { buildprintf("captureformat is %s\n", fmts[captureformat][0]); }
		else {
			int i,j;
			for (j=0; j<2; j++)
				for (i=0; i<2; i++)
					if (!Bstrcasecmp(parm->parms[0], fmts[j][i])) break;
			if (j == 2) return OSDCMD_SHOWHELP;
			captureformat = i;
		}
		return OSDCMD_OK;
	}
#ifdef SUPERBUILD
	else if (!Bstrcasecmp(parm->name, "novoxmips")) {
		if (showval) { buildprintf("novoxmips is %d\n", novoxmips); }
		else { novoxmips = (atoi(parm->parms[0]) != 0); }
	}
	else if (!Bstrcasecmp(parm->name, "usevoxels")) {
		if (showval) { buildprintf("usevoxels is %d\n", usevoxels); }
		else { usevoxels = (atoi(parm->parms[0]) != 0); }
	}
#endif
	return OSDCMD_SHOWHELP;
}

int baselayer_init(void)
{
    OSD_Init();
    
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
	OSD_RegisterFunction("screencaptureformat","screencaptureformat: sets the output format for screenshots (TGA or PCX)",osdcmd_vars);
#ifdef SUPERBUILD
	OSD_RegisterFunction("novoxmips","novoxmips: turn off/on the use of mipmaps when rendering 8-bit voxels",osdcmd_vars);
	OSD_RegisterFunction("usevoxels","usevoxels: enable/disable automatic sprite->voxel rendering",osdcmd_vars);
#endif
#if defined(POLYMOST) && defined(USE_OPENGL)
#ifdef DEBUGGINGAIDS
	OSD_RegisterFunction("hicsetpalettetint","hicsetpalettetint: sets palette tinting values",osdcmd_hicsetpalettetint);
#endif
	OSD_RegisterFunction("glinfo","glinfo: shows OpenGL information about the current OpenGL mode",osdcmd_glinfo);
#endif
	
	return 0;
}

#ifdef USE_OPENGL
void baselayer_setupopengl()
{
	char *p,*p2,*p3;
	static int warnonce = 0;
	int i;

	bglEnable(GL_TEXTURE_2D);
	bglShadeModel(GL_SMOOTH); //GL_FLAT
	bglClearColor(0,0,0,0.5); //Black Background
	bglHint(GL_PERSPECTIVE_CORRECTION_HINT,GL_NICEST); //Use FASTEST for ortho!
	bglHint(GL_LINE_SMOOTH_HINT,GL_NICEST);
	bglDisable(GL_DITHER);

	glinfo.vendor     = (const char *) bglGetString(GL_VENDOR);
	glinfo.renderer   = (const char *) bglGetString(GL_RENDERER);
	glinfo.version    = (const char *) bglGetString(GL_VERSION);
	glinfo.extensions = (const char *) bglGetString(GL_EXTENSIONS);
	
	glinfo.maxanisotropy = 1.0;
	glinfo.bgra = 0;
	glinfo.texcompr = 0;
	
	// process the extensions string and flag stuff we recognize
	p = strdup(glinfo.extensions);
	p3 = p;
	while ((p2 = Bstrtoken(p3 == p ? p : NULL, " ", (char **) &p3, 1)) != NULL) {
		if (!Bstrcmp(p2, "GL_EXT_texture_filter_anisotropic")) {
				// supports anisotropy. get the maximum anisotropy level
			bglGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &glinfo.maxanisotropy);
		} else if (!Bstrcmp(p2, "GL_EXT_texture_edge_clamp") ||
			   !Bstrcmp(p2, "GL_SGIS_texture_edge_clamp")) {
				// supports GL_CLAMP_TO_EDGE or GL_CLAMP_TO_EDGE_SGIS
			glinfo.clamptoedge = 1;
		} else if (!Bstrcmp(p2, "GL_EXT_bgra")) {
				// support bgra textures
			glinfo.bgra = 1;
		} else if (!Bstrcmp(p2, "GL_ARB_texture_compression")) {
				// support texture compression
			glinfo.texcompr = 1;
		} else if (!Bstrcmp(p2, "GL_ARB_texture_non_power_of_two")) {
				// support non-power-of-two texture sizes
			glinfo.texnpot = 1;
		} else if (!Bstrcmp(p2, "WGL_3DFX_gamma_control")) {
				// 3dfx cards have issues with fog
			glinfo.hack_nofog = 1;
			if (!(warnonce&1)) buildputs("3dfx card detected: OpenGL fog disabled\n");
			warnonce |= 1;
		} else if (!Bstrcmp(p2, "GL_ARB_multisample")) {
				// supports multisampling
			glinfo.multisample = 1;
		} else if (!Bstrcmp(p2, "GL_NV_multisample_filter_hint")) {
				// supports nvidia's multisample hint extension
			glinfo.nvmultisamplehint = 1;
		} else if (!strcmp(p2, "GL_ARB_multitexture")) {
			glinfo.multitex = 1;
		} else if (!strcmp(p2, "GL_ARB_texture_env_combine")) {
			glinfo.envcombine = 1;
		}
	}
	free(p);
}
#endif
