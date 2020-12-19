#include "build.h"

#if USE_OPENGL

#include "glbuild.h"
#include "baselayer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct glbuild_funcs glfunc;

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

int glbuild_prepare_8bit_shader(glbuild8bit *state, int resx, int resy, int stride)
{
	GLuint shaders[2] = {0,0}, prog = 0;
	GLint status = 0;

	extern const char default_glbuild_fs_glsl[];
	extern const char default_glbuild_vs_glsl[];

	float tx = (float)resx / (float)stride, ty = 1.0;
	int tsizx = stride, tsizy = resy;

	if (!glinfo.texnpot) {
		for (tsizx = 1; tsizx < stride; tsizx <<= 1) { }
		for (tsizy = 1; tsizy < resy; tsizy <<= 1) { }
		tx = (float)resx / (float)tsizx;
		ty = (float)resy / (float)tsizy;
	}

	// Buffer contents: indexes and texcoord/vertex elements.
	GLushort indexes[6] = { 0, 1, 2, 0, 2, 3 };
	GLfloat elements[4][4] = {
		// tx, ty,  vx, vy
		{ 0.0, ty,  -1.0, -1.0 },
		{ tx,  ty,   1.0, -1.0 },
		{ tx,  0.0,  1.0,  1.0 },
		{ 0.0, 0.0, -1.0,  1.0 },
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

	glfunc.glActiveTexture(GL_TEXTURE0);
	glfunc.glBindTexture(GL_TEXTURE_2D, state->frametex);
	glfunc.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, stride, resy, extfmt, GL_UNSIGNED_BYTE, frame);
}

void glbuild_draw_8bit_frame(glbuild8bit *state)
{
#if (USE_OPENGL == USE_GLES2)
	glfunc.glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
#endif
#if (USE_OPENGL == USE_GL3)
	glfunc.glBindVertexArray(state->vao);
#endif
	glfunc.glDrawElements(GL_TRIANGLE_FAN, 6, GL_UNSIGNED_SHORT, 0);
}

#endif  //USE_OPENGL
