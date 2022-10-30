#include "build.h"

#if USE_OPENGL

#include "glbuild_priv.h"
#include "osd.h"
#include "baselayer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

struct glbuild_funcs glfunc;

#if defined(DEBUGGINGAIDS)
static int gldebuglogseverity = 2;	// default to 'low'
#endif

static int osdcmd_vars(const osdfuncparm_t *);
static int osdcmd_glinfo(const osdfuncparm_t *);

static void enumerate_configure(const char *ext) {
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
	} else if (!Bstrcmp(ext, "GL_ARB_sample_shading")) {
			// supports sample shading extension
		glinfo.sampleshading = 1;
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

static void glbuild_enumerate_exts(void (*callback)(const char *)) {
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

	(void)id; (void)length; (void)userParam;

	if (gldebuglogseverity < 1) return;

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
			if (gldebuglogseverity > 3) return;
			severitystr = "MEDIUM";
			break;
		case GL_DEBUG_SEVERITY_LOW_KHR:
			if (gldebuglogseverity > 2) return;
			severitystr = "LOW";
			break;
		case GL_DEBUG_SEVERITY_NOTIFICATION_KHR:
			if (gldebuglogseverity > 1) return;
			severitystr = "NOTIFICATION";
			break;
#else
		case GL_DEBUG_SEVERITY_HIGH:
			severitystr = "HIGH";
			break;
		case GL_DEBUG_SEVERITY_MEDIUM:
			if (gldebuglogseverity > 3) return;
			severitystr = "MEDIUM";
			break;
		case GL_DEBUG_SEVERITY_LOW:
			if (gldebuglogseverity > 2) return;
			severitystr = "LOW";
			break;
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			if (gldebuglogseverity > 1) return;
			severitystr = "NOTIFICATION";
			break;
#endif
	}
	debugprintf("gl_debug_proc: %s %s %s %s\n", sourcestr, typestr, severitystr, message);
}
#endif

int glbuild_init(void)
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

	glbuild_enumerate_exts(enumerate_configure);
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

#if defined(DEBUGGINGAIDS)
	OSD_RegisterFunction("gldebuglogseverity","gldebuglogseverity: set OpenGL debug log verbosity (0-4 = none/notification/low/med/high)",osdcmd_vars);
#endif
	OSD_RegisterFunction("glinfo","glinfo [exts]: shows OpenGL information about the current OpenGL mode",osdcmd_glinfo);

	return 0;
}

static inline void * getproc_(const char *func, int *err, int fatal)
{
	void *proc = getglprocaddress(func, 1);
	if (!proc && fatal) {
		buildprintf("glbuild_loadfunctions: failed to find %s\n", func);
		*err = 1;
	}
	return proc;
}
#define INIT_PROC(s)        glfunc.s = getproc_(#s, &err, 1)
#define INIT_PROC_SOFT(s)   glfunc.s = getproc_(#s, &err, 0)

int glbuild_loadfunctions(void)
{
	int err=0;

	INIT_PROC(glClearColor);
	INIT_PROC(glClear);
	INIT_PROC(glColorMask);
	INIT_PROC(glBlendFunc);
	INIT_PROC(glCullFace);
	INIT_PROC(glFrontFace);
	INIT_PROC(glPolygonOffset);
#if (USE_OPENGL != USE_GLES2)
	INIT_PROC(glPolygonMode);
#endif
	INIT_PROC(glEnable);
	INIT_PROC(glDisable);
	INIT_PROC(glGetFloatv);
	INIT_PROC(glGetIntegerv);
	INIT_PROC(glGetString);
#if (USE_OPENGL == USE_GL3)
	INIT_PROC(glGetStringi);
#endif
	INIT_PROC(glGetError);
	INIT_PROC(glHint);
	INIT_PROC(glPixelStorei);
	INIT_PROC(glViewport);
	INIT_PROC(glScissor);
	INIT_PROC_SOFT(glMinSampleShadingARB);

	// Depth
	INIT_PROC(glDepthFunc);
	INIT_PROC(glDepthMask);
#if (USE_OPENGL == USE_GLES2)
	INIT_PROC(glDepthRangef);
#else
	INIT_PROC(glDepthRange);
#endif

	// Raster funcs
	INIT_PROC(glReadPixels);

	// Texture mapping
	INIT_PROC(glTexEnvf);
	INIT_PROC(glGenTextures);
	INIT_PROC(glDeleteTextures);
	INIT_PROC(glBindTexture);
	INIT_PROC(glTexImage2D);
	INIT_PROC(glTexSubImage2D);
	INIT_PROC(glTexParameterf);
	INIT_PROC(glTexParameteri);
	INIT_PROC_SOFT(glCompressedTexImage2D);

	// Buffer objects
	INIT_PROC(glBindBuffer);
	INIT_PROC(glBufferData);
	INIT_PROC(glBufferSubData);
	INIT_PROC(glDeleteBuffers);
	INIT_PROC(glGenBuffers);
	INIT_PROC(glDrawElements);
	INIT_PROC(glEnableVertexAttribArray);
	INIT_PROC(glDisableVertexAttribArray);
	INIT_PROC(glVertexAttribPointer);
#if (USE_OPENGL == USE_GL3)
    INIT_PROC(glBindVertexArray);
    INIT_PROC(glDeleteVertexArrays);
    INIT_PROC(glGenVertexArrays);
#endif

	// Shaders
	INIT_PROC(glActiveTexture);
	INIT_PROC(glAttachShader);
	INIT_PROC(glCompileShader);
	INIT_PROC(glCreateProgram);
	INIT_PROC(glCreateShader);
	INIT_PROC(glDeleteProgram);
	INIT_PROC(glDeleteShader);
	INIT_PROC(glDetachShader);
	INIT_PROC(glGetAttribLocation);
	INIT_PROC(glGetProgramiv);
	INIT_PROC(glGetProgramInfoLog);
	INIT_PROC(glGetShaderiv);
	INIT_PROC(glGetShaderInfoLog);
	INIT_PROC(glGetUniformLocation);
	INIT_PROC(glLinkProgram);
	INIT_PROC(glShaderSource);
	INIT_PROC(glUseProgram);
	INIT_PROC(glUniform1i);
	INIT_PROC(glUniform1f);
	INIT_PROC(glUniform2f);
	INIT_PROC(glUniform3f);
	INIT_PROC(glUniform4f);
	INIT_PROC(glUniformMatrix4fv);

#if (USE_OPENGL == USE_GLES2)
    INIT_PROC_SOFT(glDebugMessageCallbackKHR);
#else
    INIT_PROC_SOFT(glDebugMessageCallback);
#endif

	if (err) glbuild_unloadfunctions();
	return err;
}

void glbuild_unloadfunctions(void)
{
	memset(&glfunc, 0, sizeof(glfunc));
}

void glbuild_check_errors(const char *file, int line)
{
	GLenum err;
	const char *str;

	while ((err = glfunc.glGetError()) != GL_NO_ERROR) {
		switch (err) {
			case GL_INVALID_ENUM: str = "GL_INVALID_ENUM"; break;
			case GL_INVALID_VALUE: str = "GL_INVALID_VALUE"; break;
			case GL_INVALID_OPERATION: str = "GL_INVALID_OPERATION"; break;
			case GL_INVALID_FRAMEBUFFER_OPERATION: str = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
			case GL_OUT_OF_MEMORY: str = "GL_OUT_OF_MEMORY"; break;
#ifdef GL_STACK_UNDERFLOW
			case GL_STACK_UNDERFLOW: str = "GL_STACK_UNDERFLOW"; break;
#endif
#ifdef GL_STACK_OVERFLOW
			case GL_STACK_OVERFLOW: str = "GL_STACK_OVERFLOW"; break;
#endif
			default: str = "(unknown)"; break;
		}
		debugprintf("GL error [%s:%d]: (%d) %s\n", file, line, err, str);
	}
}


//
// OpenGL shader compilation and linking
//
static GLchar *glbuild_cook_source(const GLchar *source, const char *spec)
{
	GLchar *cooked, *pos, *match, *end;
	const char *marker = "#glbuild(";
	const int markerlen = 9;
	int keep;

	cooked = strdup(source);
	if (!cooked) {
		debugprintf("glbuild_cook_source: couldn't duplicate source\n");
		return NULL;
	}
	pos = cooked;
	do {
		match = strstr(pos, marker);
		if (!match) break;

		end = strchr(match, ')');
		if (!end) break;

		if (!strncmp(match + markerlen, spec, end-match-markerlen)) {
			// Marker matches the spec. Overwrite it with spaces to uncomment the line.
			for (; match <= end; match++) *match = ' ';
		} else {
			// Overwrite the line with spaces.
			for (; *match && *match != '\n' && *match != '\r'; match++) *match = ' ';
		}
		pos = end;
	} while(pos);

	return cooked;
}

GLuint glbuild_compile_shader(GLuint type, const GLchar *source)
{
	GLuint shader;
	GLint status;
	GLchar *cookedsource;

	shader = glfunc.glCreateShader(type);
	if (!shader) {
		return 0;
	}

#if (USE_OPENGL == USE_GLES2)
	cookedsource = glbuild_cook_source(source, "ES2");
#elif (USE_OPENGL == USE_GL3)
	cookedsource = glbuild_cook_source(source, "3");
#else
	cookedsource = glbuild_cook_source(source, "2");
#endif
	if (!cookedsource) {
		glfunc.glDeleteShader(shader);
		return 0;
	}

	glfunc.glShaderSource(shader, 1, (const GLchar**)&cookedsource, NULL);
	glfunc.glCompileShader(shader);
	free(cookedsource);

	glfunc.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		GLint loglen = 0;
		GLchar *logtext = NULL;

		glfunc.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &loglen);

		logtext = (GLchar *)malloc(loglen);
		glfunc.glGetShaderInfoLog(shader, loglen, &loglen, logtext);
		buildprintf("GL shader compile error: %s\n", logtext);
		free(logtext);

		glfunc.glDeleteShader(shader);
		return 0;
	}

	return shader;
}

GLuint glbuild_link_program(int shadercount, GLuint *shaders)
{
	GLuint program;
	GLint status;
	int i;

	program = glfunc.glCreateProgram();
	if (!program) {
		return 0;
	}

	for (i = 0; i < shadercount; i++) {
		glfunc.glAttachShader(program, shaders[i]);
	}

	glfunc.glLinkProgram(program);

	glfunc.glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		GLint loglen = 0;
		GLchar *logtext = NULL;

		glfunc.glGetProgramiv(program, GL_INFO_LOG_LENGTH, &loglen);

		logtext = (GLchar *)malloc(loglen);
		glfunc.glGetProgramInfoLog(program, loglen, &loglen, logtext);
		buildprintf("glbuild_link_program: link error: %s\n", logtext);
		free(logtext);

		glfunc.glDeleteProgram(program);
		return 0;
	}

	for (i = 0; i < shadercount; i++) {
		glfunc.glDetachShader(program, shaders[i]);
	}

	return program;
}


//
// OpenGL 8-bit rendering
//
int glbuild_prepare_8bit_shader(glbuild8bit *state, int resx, int resy, int stride, int winx, int winy)
{
	GLuint shaders[2] = {0,0}, prog = 0;
	GLint status = 0;

	extern const char default_glbuild_fs_glsl[];
	extern const char default_glbuild_vs_glsl[];

	float tx = (float)resx / (float)stride, ty = 1.0;
	int tsizx = stride, tsizy = resy;

	float winaspect = (float)winx / (float)winy;
	float frameaspect = (float)resx / (float)resy;
	float aspectx, aspecty;

	if (!glinfo.texnpot) {
		for (tsizx = 1; tsizx < stride; tsizx <<= 1) { }
		for (tsizy = 1; tsizy < resy; tsizy <<= 1) { }
		tx = (float)resx / (float)tsizx;
		ty = (float)resy / (float)tsizy;
	}

	// Correct for aspect ratio difference between the window size
	// and the logical 8-bit surface by skewing the quad we draw.
	if (fabs(winaspect - frameaspect) < 0.01) {
		aspectx = aspecty = 1.0;
	} else if (winaspect > frameaspect) {
		// Window is wider than the frame.
		aspectx = frameaspect / winaspect;
		aspecty = 1.0;
	} else {
		// Window is narrower than the frame.
		aspectx = 1.0;
		aspecty = winaspect / frameaspect;
	}

	// Buffer contents: indexes and texcoord/vertex elements.
	GLushort indexes[6] = { 0, 1, 2, 0, 2, 3 };
	GLfloat elements[4][4] = {
		// tx, ty,  vx, vy
		{ 0.0, ty,  -aspectx, -aspecty },
		{ tx,  ty,   aspectx, -aspecty },
		{ tx,  0.0,  aspectx,  aspecty },
		{ 0.0, 0.0, -aspectx,  aspecty },
	};

	GLint clamp = glinfo.clamptoedge ? GL_CLAMP_TO_EDGE : GL_CLAMP;
	GLenum intfmt, extfmt;

	// Initialise texture objects for the palette and framebuffer.
	// The textures will be uploaded on the first rendered frame.
	glfunc.glGenTextures(1, &state->paltex);
	glfunc.glActiveTexture(GL_TEXTURE1);
	glfunc.glBindTexture(GL_TEXTURE_2D, state->paltex);
	glfunc.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);	// Allocates memory.
	glfunc.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glfunc.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glfunc.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp);
	glfunc.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp);

#if (USE_OPENGL == USE_GLES2)
	intfmt = GL_LUMINANCE;
	extfmt = GL_LUMINANCE;
#else
	intfmt = GL_RGB;
	extfmt = GL_RED;
#endif

	glfunc.glGenTextures(1, &state->frametex);
	glfunc.glActiveTexture(GL_TEXTURE0);
	glfunc.glBindTexture(GL_TEXTURE_2D, state->frametex);
	glfunc.glTexImage2D(GL_TEXTURE_2D, 0, intfmt, tsizx, tsizy, 0, extfmt, GL_UNSIGNED_BYTE, NULL);	// Allocates memory.
	glfunc.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glfunc.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glfunc.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp);
	glfunc.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp);

	glfunc.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// Prepare the vertex and fragment shaders.
	shaders[0] = glbuild_compile_shader(GL_VERTEX_SHADER, default_glbuild_vs_glsl);
	shaders[1] = glbuild_compile_shader(GL_FRAGMENT_SHADER, default_glbuild_fs_glsl);
	if (!shaders[0] || !shaders[1]) {
		if (shaders[0]) glfunc.glDeleteShader(shaders[0]);
		if (shaders[1]) glfunc.glDeleteShader(shaders[1]);
		return -1;
	}

	// Prepare the shader program.
	prog = glbuild_link_program(2, shaders);
	if (!prog) {
		glfunc.glDeleteShader(shaders[0]);
		glfunc.glDeleteShader(shaders[1]);
		return -1;
	}

	// Shaders were detached in glbuild_link_program.
	glfunc.glDeleteShader(shaders[0]);
	glfunc.glDeleteShader(shaders[1]);

	// Connect the textures to the program.
	glfunc.glUseProgram(prog);
	glfunc.glUniform1i(glfunc.glGetUniformLocation(prog, "u_palette"), 1);
	glfunc.glUniform1i(glfunc.glGetUniformLocation(prog, "u_frame"), 0);

	state->program = prog;
	state->attrib_vertex = glfunc.glGetAttribLocation(prog, "a_vertex");
	state->attrib_texcoord = glfunc.glGetAttribLocation(prog, "a_texcoord");

#if (USE_OPENGL == USE_GL3)
	glfunc.glGenVertexArrays(1, &state->vao);
	glfunc.glBindVertexArray(state->vao);
#endif

	// Prepare buffer objects for the vertex/texcoords and indexes.
	glfunc.glGenBuffers(1, &state->buffer_indexes);
	glfunc.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->buffer_indexes);
	glfunc.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indexes), indexes, GL_STATIC_DRAW);

	glfunc.glGenBuffers(1, &state->buffer_elements);
	glfunc.glBindBuffer(GL_ARRAY_BUFFER, state->buffer_elements);
	glfunc.glBufferData(GL_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);

	glfunc.glEnableVertexAttribArray(state->attrib_texcoord);
	glfunc.glVertexAttribPointer(state->attrib_texcoord, 2, GL_FLOAT, GL_FALSE,
		sizeof(GLfloat)*4, (const GLvoid *)0);

    glfunc.glEnableVertexAttribArray(state->attrib_vertex);
	glfunc.glVertexAttribPointer(state->attrib_vertex, 2, GL_FLOAT, GL_FALSE,
		sizeof(GLfloat)*4, (const GLvoid *)(sizeof(GLfloat)*2));

#if (USE_OPENGL == USE_GL3)
    glfunc.glBindVertexArray(0);
#endif

	// Set some persistent GL state.
	glfunc.glClearColor(0.0, 0.0, 0.0, 0.0);
	glfunc.glDisable(GL_DEPTH_TEST);

	return 0;
}

void glbuild_delete_8bit_shader(glbuild8bit *state)
{
	if (state->program) {
#if (USE_OPENGL != USE_GL3)
		glfunc.glDisableVertexAttribArray(state->attrib_vertex);
		glfunc.glDisableVertexAttribArray(state->attrib_texcoord);
#endif
		glfunc.glUseProgram(0);	// Disable shaders, go back to fixed-function.
		glfunc.glDeleteProgram(state->program);
		state->program = 0;
	}
	if (state->paltex) {
		glfunc.glDeleteTextures(1, &state->paltex);
		state->paltex = 0;
	}
	if (state->frametex) {
		glfunc.glDeleteTextures(1, &state->frametex);
		state->frametex = 0;
	}
#if (USE_OPENGL == USE_GL3)
	if (state->vao) {
		glfunc.glBindVertexArray(0);
		glfunc.glDeleteVertexArrays(1, &state->vao);
		state->vao = 0;
	}
#endif
	if (state->buffer_indexes) {
		glfunc.glDeleteBuffers(1, &state->buffer_indexes);
		state->buffer_indexes = 0;
	}
	if (state->buffer_elements) {
		glfunc.glDeleteBuffers(1, &state->buffer_elements);
		state->buffer_elements = 0;
	}
}

void glbuild_update_8bit_palette(glbuild8bit *state, const GLvoid *pal)
{
	glfunc.glActiveTexture(GL_TEXTURE1);
	glfunc.glBindTexture(GL_TEXTURE_2D, state->paltex);
	glfunc.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pal);
}

void glbuild_update_8bit_frame(glbuild8bit *state, const GLvoid *frame, int resx, int resy, int stride)
{
#if (USE_OPENGL == USE_GLES2)
	GLenum extfmt = GL_LUMINANCE;
#else
	GLenum extfmt = GL_RED;
#endif

	(void)resx;

	glfunc.glActiveTexture(GL_TEXTURE0);
	glfunc.glBindTexture(GL_TEXTURE_2D, state->frametex);
	glfunc.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, stride, resy, extfmt, GL_UNSIGNED_BYTE, frame);
}

void glbuild_draw_8bit_frame(glbuild8bit *state)
{
	(void)state;

#if (USE_OPENGL == USE_GLES2)
	glfunc.glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
#endif
#if (USE_OPENGL == USE_GL3)
	glfunc.glBindVertexArray(state->vao);
#endif
	glfunc.glDrawElements(GL_TRIANGLE_FAN, 6, GL_UNSIGNED_SHORT, 0);
}


//
// OSD variables and commands
//
static int osdcmd_vars(const osdfuncparm_t *parm)
{
	int showval = (parm->numparms < 1);

#if defined(DEBUGGINGAIDS)
	if (!Bstrcasecmp(parm->name, "gldebuglogseverity")) {
		const char *levels[] = {"none", "notification", "low", "medium", "high"};
		if (showval) { buildprintf("gldebuglogseverity is %s (%d)\n", levels[gldebuglogseverity], gldebuglogseverity); }
		else {
			gldebuglogseverity = atoi(parm->parms[0]);
			gldebuglogseverity = max(0, min(4, gldebuglogseverity));
			buildprintf("gldebuglogseverity is now %s (%d)\n", levels[gldebuglogseverity], gldebuglogseverity);
		}
		return OSDCMD_OK;
	}
#endif

	return OSDCMD_SHOWHELP;
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
		"   Nvidia quincunx:     %s\n"
		"   Sample shading:      %s\n",
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
		glinfo.nvmultisamplehint ? supported : unsupported,
		glinfo.sampleshading ? supported : unsupported
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

static void indentedputs(const char *ext) {
	buildputs(" ");
	buildputs(ext);
}

static void dumpglexts(void)
{
	char *workstr, *workptr, *nextptr = NULL, *ext = NULL;

	if (!glinfo.loaded) {
		buildputs("OpenGL information not available.\n");
		return;
	}

	buildputs("OpenGL Extensions:\n");
	glbuild_enumerate_exts(indentedputs);
	buildputs("\n");
}

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


#endif  //USE_OPENGL
