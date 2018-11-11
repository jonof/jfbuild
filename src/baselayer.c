// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#include "build.h"
#include "osd.h"
#include "baselayer.h"

#ifdef RENDERTYPEWIN
#include "winlayer.h"
#endif

#if USE_OPENGL
#include "glbuild.h"
baselayer_glinfo glinfo;
#endif //USE_OPENGL

static void onvideomodechange(int UNUSED(newmode)) { }
void (*baselayer_onvideomodechange)(int) = onvideomodechange;

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
	m = Bstrtol(parm->parms[0], &p, 10);

	if (m < 0 || m > 3) return OSDCMD_SHOWHELP;

	setrendermode(m);
	buildprintf("Rendering method changed to %s\n", modestrs[ getrendermode() ] );

	return OSDCMD_OK;
}
#endif //USE_POLYMOST

#if defined(DEBUGGINGAIDS) && USE_OPENGL
static int debuggllogseverity = 2;	// default to 'low'
#endif

#if defined(DEBUGGINGAIDS) && USE_POLYMOST && USE_OPENGL
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
#endif //DEBUGGINGAIDS && USE_POLYMOST && USE_OPENGL

#if USE_OPENGL
static void dumpglinfo(void);
static void dumpglexts(void);
static int osdcmd_glinfo(const osdfuncparm_t *parm)
{
	if (parm->numparms == 0) {
		dumpglinfo();
		buildputs("Use \"glinfo exts\" to list extensions.\n");
	} else if (strcmp(parm->parms[0], "exts") == 0) {
		dumpglexts();
	}
	return OSDCMD_OK;
}
#endif //USE_OPENGL

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
#if defined(DEBUGGINGAIDS) && USE_OPENGL
	else if (!Bstrcasecmp(parm->name, "debuggllogseverity")) {
		const char *levels[] = {"none", "notification", "low", "medium", "high"};
		if (showval) { buildprintf("debuggllogseverity is %s (%d)\n", levels[debuggllogseverity], debuggllogseverity); }
		else {
			debuggllogseverity = atoi(parm->parms[0]);
			debuggllogseverity = max(0, min(4, debuggllogseverity));
			buildprintf("debuggllogseverity is now %s (%d)\n", levels[debuggllogseverity], debuggllogseverity);
		}
		return OSDCMD_OK;
	}
#endif
	return OSDCMD_SHOWHELP;
}

int baselayer_init(void)
{
    OSD_Init();

	OSD_RegisterFunction("screencaptureformat","screencaptureformat: sets the output format for screenshots (TGA or PCX)",osdcmd_vars);

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
#if defined(DEBUGGINGAIDS) && USE_OPENGL
	OSD_RegisterFunction("debuggllogseverity","debuggllogseverity: set OpenGL debug log verbosity (0-4 = none/notification/low/med/high)",osdcmd_vars);
#endif

#if USE_OPENGL
	OSD_RegisterFunction("glinfo","glinfo [exts]: shows OpenGL information about the current OpenGL mode",osdcmd_glinfo);
#endif //USE_OPENGL

	return 0;
}

#if USE_OPENGL

static void glext_enumerate_dump(const char *ext) {
	buildputs(" ");
	buildputs(ext);
}

static void glext_enumerate_configure(const char *ext) {
	if (!Bstrcmp(ext, "GL_EXT_texture_filter_anisotropic")) {
			// supports anisotropy. get the maximum anisotropy level
		glfunc.glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &glinfo.maxanisotropy);
	} else if (!Bstrcmp(ext, "GL_EXT_texture_edge_clamp") ||
			!Bstrcmp(ext, "GL_SGIS_texture_edge_clamp")) {
			// supports GL_CLAMP_TO_EDGE
		glinfo.clamptoedge = 1;
	} else if (!Bstrcmp(ext, "GL_EXT_bgra") ||
			!Bstrcmp(ext, "GL_EXT_texture_format_BGRA8888")) {
			// support bgra textures
		glinfo.bgra = 1;
	} else if (!Bstrcmp(ext, "GL_EXT_texture_compression_s3tc")) {
			// support DXT1 and DXT5 texture compression
		glinfo.texcomprdxt1 = 1;
		glinfo.texcomprdxt5 = 1;
	} else if (!Bstrcmp(ext, "GL_EXT_texture_compression_dxt1")) {
			// support DXT1 texture compression
		glinfo.texcomprdxt1 = 1;
	} else if (!Bstrcmp(ext, "GL_OES_compressed_ETC1_RGB8_texture")) {
			// support ETC1 texture compression
		glinfo.texcompretc1 = 1;
	} else if (!Bstrcmp(ext, "GL_ARB_texture_non_power_of_two") ||
			!Bstrcmp(ext, "GL_OES_texture_npot")) {
			// support non-power-of-two texture sizes
		glinfo.texnpot = 1;
	} else if (!Bstrcmp(ext, "GL_ARB_multisample")) {
			// supports multisampling
		glinfo.multisample = 1;
	} else if (!Bstrcmp(ext, "GL_NV_multisample_filter_hint")) {
			// supports nvidia's multisample hint extension
		glinfo.nvmultisamplehint = 1;
	} else if (!strcmp(ext, "GL_ARB_shading_language_100")) {
		const char *ver;

		// Clear the error queue, then query the version string.
		while (glfunc.glGetError() != GL_NO_ERROR) { }
		ver = (const char *) glfunc.glGetString(GL_SHADING_LANGUAGE_VERSION);

		if (!ver && glfunc.glGetError() == GL_INVALID_ENUM) {
			// GLSL 1.00 (1.051).
			glinfo.glslmajver = 1;
			glinfo.glslminver = 0;
		} else if (ver) {
			// GLSL 1.10 or newer.
			sscanf(ver, "%d.%d", &glinfo.glslmajver, &glinfo.glslminver);
		}
	} else if (!strcmp(ext, "GL_KHR_debug")) {
		glinfo.debugext = 1;
	}
}

static void glext_enumerate(void (*callback)(const char *)) {
	char *workstr = NULL, *workptr = NULL, *nextptr = NULL;
	const char *ext = NULL;
	GLint extn = 0, numexts = 0;

#if (USE_OPENGL == USE_GL3)
	if (glinfo.majver >= 3) {
		glfunc.glGetIntegerv(GL_NUM_EXTENSIONS, &numexts);
	} else
#endif
	{
		const char *extstr = (const char *) glfunc.glGetString(GL_EXTENSIONS);
		workstr = workptr = strdup(extstr);
	}

	while (1) {
#if (USE_OPENGL == USE_GL3)
		if (glinfo.majver >= 3) {
			if (extn == numexts) break;
			ext = (const char *) glfunc.glGetStringi(GL_EXTENSIONS, extn);
		} else
#endif
		{
			ext = Bstrtoken(workptr, " ", &nextptr, 1);
			if (!ext) break;
		}

		callback(ext);

#if (USE_OPENGL == USE_GL3)
		if (glinfo.majver >= 3) {
			extn++;
		} else
#endif
		{
			workptr = NULL;
		}
	}

	if (workstr) free(workstr);
}

#if defined(DEBUGGINGAIDS) && defined(GL_KHR_debug)
static void APIENTRY gl_debug_proc(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
	const GLchar* message, const GLvoid* userParam)
{
	const char *sourcestr = "(unknown)";
	const char *typestr = "(unknown)";
	const char *severitystr = "(unknown)";

	if (debuggllogseverity < 1) return;

	switch (source) {
#if (USE_OPENGL == USE_GLES2)
		case GL_DEBUG_SOURCE_API_KHR: sourcestr = "API"; break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER_KHR: sourcestr = "SHADER_COMPILER"; break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM_KHR: sourcestr = "WINDOW_SYSTEM"; break;
		case GL_DEBUG_SOURCE_THIRD_PARTY_KHR: sourcestr = "THIRD_PARTY"; break;
		case GL_DEBUG_SOURCE_APPLICATION_KHR: sourcestr = "APPLICATION"; break;
		case GL_DEBUG_SOURCE_OTHER_KHR: sourcestr = "OTHER"; break;
#else
		case GL_DEBUG_SOURCE_API: sourcestr = "API"; break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER: sourcestr = "SHADER_COMPILER"; break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM: sourcestr = "WINDOW_SYSTEM"; break;
		case GL_DEBUG_SOURCE_THIRD_PARTY: sourcestr = "THIRD_PARTY"; break;
		case GL_DEBUG_SOURCE_APPLICATION: sourcestr = "APPLICATION"; break;
		case GL_DEBUG_SOURCE_OTHER: sourcestr = "OTHER"; break;
#endif
	}
	switch (type) {
#if (USE_OPENGL == USE_GLES2)
		case GL_DEBUG_TYPE_ERROR_KHR: typestr = "ERROR"; break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_KHR: typestr = "DEPRECATED_BEHAVIOR"; break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_KHR: typestr = "UNDEFINED_BEHAVIOR"; break;
		case GL_DEBUG_TYPE_PERFORMANCE_KHR: typestr = "PERFORMANCE"; break;
		case GL_DEBUG_TYPE_PORTABILITY_KHR: typestr = "PORTABILITY"; break;
		case GL_DEBUG_TYPE_OTHER_KHR: typestr = "OTHER"; break;
		case GL_DEBUG_TYPE_MARKER_KHR: typestr = "MARKER"; break;
		case GL_DEBUG_TYPE_PUSH_GROUP_KHR: typestr = "PUSH_GROUP"; break;
		case GL_DEBUG_TYPE_POP_GROUP_KHR: typestr = "POP_GROUP"; break;
#else
		case GL_DEBUG_TYPE_ERROR: typestr = "ERROR"; break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typestr = "DEPRECATED_BEHAVIOR"; break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: typestr = "UNDEFINED_BEHAVIOR"; break;
		case GL_DEBUG_TYPE_PERFORMANCE: typestr = "PERFORMANCE"; break;
		case GL_DEBUG_TYPE_PORTABILITY: typestr = "PORTABILITY"; break;
		case GL_DEBUG_TYPE_OTHER: typestr = "OTHER"; break;
		case GL_DEBUG_TYPE_MARKER: typestr = "MARKER"; break;
		case GL_DEBUG_TYPE_PUSH_GROUP: typestr = "PUSH_GROUP"; break;
		case GL_DEBUG_TYPE_POP_GROUP: typestr = "POP_GROUP"; break;
#endif
	}
	switch (severity) {
#if (USE_OPENGL == USE_GLES2)
		case GL_DEBUG_SEVERITY_HIGH_KHR:
			severitystr = "HIGH";
			break;
		case GL_DEBUG_SEVERITY_MEDIUM_KHR:
			if (debuggllogseverity > 3) return;
			severitystr = "MEDIUM";
			break;
		case GL_DEBUG_SEVERITY_LOW_KHR:
			if (debuggllogseverity > 2) return;
			severitystr = "LOW";
			break;
		case GL_DEBUG_SEVERITY_NOTIFICATION_KHR:
			if (debuggllogseverity > 1) return;
			severitystr = "NOTIFICATION";
			break;
#else
		case GL_DEBUG_SEVERITY_HIGH:
			severitystr = "HIGH";
			break;
		case GL_DEBUG_SEVERITY_MEDIUM:
			if (debuggllogseverity > 3) return;
			severitystr = "MEDIUM";
			break;
		case GL_DEBUG_SEVERITY_LOW:
			if (debuggllogseverity > 2) return;
			severitystr = "LOW";
			break;
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			if (debuggllogseverity > 1) return;
			severitystr = "NOTIFICATION";
			break;
#endif
	}
	debugprintf("gl_debug_proc: %s %s %s %s\n", sourcestr, typestr, severitystr, message);
}
#endif

int baselayer_setupopengl(void)
{
	const char *glver;

	if (glbuild_loadfunctions()) {
		return -1;
	}

	memset(&glinfo, 0, sizeof(glinfo));
	glver = (const char *) glfunc.glGetString(GL_VERSION);

#if (USE_OPENGL == USE_GLES2)
	sscanf(glver, "OpenGL ES %d.%d", &glinfo.majver, &glinfo.minver);
#else
	sscanf(glver, "%d.%d", &glinfo.majver, &glinfo.minver);
#endif

	sscanf((const char *) glfunc.glGetString(GL_VERSION), "%d.%d",
		&glinfo.majver, &glinfo.minver);
	glinfo.maxanisotropy = 1.0;
	glfunc.glGetIntegerv(GL_MAX_TEXTURE_SIZE, &glinfo.maxtexsize);
	glfunc.glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &glinfo.multitex);
	glfunc.glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &glinfo.maxvertexattribs);

#if (USE_OPENGL == USE_GLES2)
	glinfo.clamptoedge = 1;
	glinfo.glslmajver = 1;
	glinfo.glslminver = 0;
#elif (USE_OPENGL == USE_GL3)
	glinfo.bgra = 1;
	glinfo.clamptoedge = 1;
	glinfo.texnpot = 1;
	glinfo.multisample = 1;

	glver = (const char *) glfunc.glGetString(GL_SHADING_LANGUAGE_VERSION);
	sscanf(glver, "%d.%d", &glinfo.glslmajver, &glinfo.glslminver);
#endif

	glext_enumerate(glext_enumerate_configure);
	glinfo.loaded = 1;

#if (USE_OPENGL == USE_GLES2)
	if (glinfo.majver < 2) {
		buildputs("OpenGL device does not support ES 2.0 features.\n");
		return -2;
	}
#else
	if (glinfo.majver < 2) {
		buildputs("OpenGL device does not support 2.0 features.\n");
		return -2;
	}
	if (glinfo.multitex < 2) {
		buildputs("OpenGL device does not have at least 2 texturing units.\n");
		return -2;
	}
	if (glinfo.glslmajver < 1 || (glinfo.glslmajver == 1 && glinfo.glslminver < 10)) {
		buildputs("OpenGL device does not support GLSL 1.10 shaders.\n");
		return -2;
	}
#endif

#if defined(DEBUGGINGAIDS) && defined(GL_KHR_debug)
	if (glinfo.debugext) {
#if (USE_OPENGL == USE_GLES2)
		glfunc.glDebugMessageCallbackKHR(gl_debug_proc, NULL);
		glfunc.glEnable(GL_DEBUG_OUTPUT_KHR);
#else
		glfunc.glDebugMessageCallback(gl_debug_proc, NULL);
		glfunc.glEnable(GL_DEBUG_OUTPUT);
#endif
	}
#endif

	return 0;
}

static void dumpglinfo(void)
{
	const char *supported = "supported", *unsupported = "not supported";
	const char *glslverstr = "not supported";

	if (!glinfo.loaded) {
		buildputs("OpenGL information not available.\n");
		return;
	}

	if (glinfo.glslmajver == 1 && glinfo.glslminver == 0) {
		glslverstr = "1.00";
	} else if (glinfo.glslmajver >= 1) {
		glslverstr = (const char *) glfunc.glGetString(GL_SHADING_LANGUAGE_VERSION);
	}

	buildprintf(
		"OpenGL Information:\n"
		" Version:      %s\n"
		" Vendor:       %s\n"
		" Renderer:     %s\n"
		" GLSL version: %s\n"
		" Anisotropic filtering: %s (%.1f)\n"
		" BGRA textures:         %s\n"
		" Non-2^x textures:      %s\n"
		" DXT1 texture compr:    %s\n"
		" DXT5 texture compr:    %s\n"
		" ETC1 texture compr:    %s\n"
		" Clamp-to-edge:         %s\n"
		" Multisampling:         %s\n"
		"   Nvidia hint:         %s\n",
		glfunc.glGetString(GL_VERSION),
		glfunc.glGetString(GL_VENDOR),
		glfunc.glGetString(GL_RENDERER),
		glslverstr,
		glinfo.maxanisotropy > 1.0 ? supported : unsupported, glinfo.maxanisotropy,
		glinfo.bgra ? supported : unsupported,
		glinfo.texnpot ? supported : unsupported,
		glinfo.texcomprdxt1 ? supported : unsupported,
		glinfo.texcomprdxt5 ? supported : unsupported,
		glinfo.texcompretc1 ? supported : unsupported,
		glinfo.clamptoedge ? supported : unsupported,
		glinfo.multisample ? supported : unsupported,
		glinfo.nvmultisamplehint ? supported : unsupported
	);

#ifdef DEBUGGINGAIDS
	buildprintf(
		" Multitexturing:        %d units\n"
		" Max 1/2D texture size: %d\n"
		" Max vertex attribs:    %d\n",
		glinfo.multitex,
		glinfo.maxtexsize,
		glinfo.maxvertexattribs
	);
#endif
}

static void dumpglexts(void)
{
	char *workstr, *workptr, *nextptr = NULL, *ext = NULL;

	if (!glinfo.loaded) {
		buildputs("OpenGL information not available.\n");
		return;
	}

	buildputs("OpenGL Extensions:\n");
	glext_enumerate(glext_enumerate_dump);
	buildputs("\n");
}

#endif //USE_OPENGL
