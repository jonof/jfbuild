#ifdef USE_OPENGL

#include "glbuild.h"

#ifdef DYNAMIC_OPENGL

#include "build.h"
#include "baselayer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void (APIENTRY * bglClearColor)( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha );
void (APIENTRY * bglClear)( GLbitfield mask );
void (APIENTRY * bglColorMask)( GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha );
void (APIENTRY * bglAlphaFunc)( GLenum func, GLclampf ref );
void (APIENTRY * bglBlendFunc)( GLenum sfactor, GLenum dfactor );
void (APIENTRY * bglCullFace)( GLenum mode );
void (APIENTRY * bglFrontFace)( GLenum mode );
void (APIENTRY * bglPolygonOffset)( GLfloat factor, GLfloat units );
void (APIENTRY * bglPolygonMode)( GLenum face, GLenum mode );
void (APIENTRY * bglEnable)( GLenum cap );
void (APIENTRY * bglDisable)( GLenum cap );
void (APIENTRY * bglGetFloatv)( GLenum pname, GLfloat *params );
void (APIENTRY * bglGetIntegerv)( GLenum pname, GLint *params );
void (APIENTRY * bglPushAttrib)( GLbitfield mask );
void (APIENTRY * bglPopAttrib)( void );
GLenum (APIENTRY * bglGetError)( void );
const GLubyte* (APIENTRY * bglGetString)( GLenum name );
void (APIENTRY * bglHint)( GLenum target, GLenum mode );
void (APIENTRY * bglPixelStorei)( GLenum pname, GLint param );

// Depth
void (APIENTRY * bglDepthFunc)( GLenum func );
void (APIENTRY * bglDepthMask)( GLboolean flag );
void (APIENTRY * bglDepthRange)( GLclampd near_val, GLclampd far_val );

// Matrix
void (APIENTRY * bglMatrixMode)( GLenum mode );
void (APIENTRY * bglOrtho)( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val );
void (APIENTRY * bglFrustum)( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val );
void (APIENTRY * bglViewport)( GLint x, GLint y, GLsizei width, GLsizei height );
void (APIENTRY * bglPushMatrix)( void );
void (APIENTRY * bglPopMatrix)( void );
void (APIENTRY * bglLoadIdentity)( void );
void (APIENTRY * bglLoadMatrixf)( const GLfloat *m );

// Drawing
void (APIENTRY * bglBegin)( GLenum mode );
void (APIENTRY * bglEnd)( void );
void (APIENTRY * bglVertex2f)( GLfloat x, GLfloat y );
void (APIENTRY * bglVertex2i)( GLint x, GLint y );
void (APIENTRY * bglVertex3d)( GLdouble x, GLdouble y, GLdouble z );
void (APIENTRY * bglVertex3fv)( const GLfloat *v );
void (APIENTRY * bglColor4f)( GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha );
void (APIENTRY * bglColor4ub)( GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha );
void (APIENTRY * bglTexCoord2d)( GLdouble s, GLdouble t );
void (APIENTRY * bglTexCoord2f)( GLfloat s, GLfloat t );

// Lighting
void (APIENTRY * bglShadeModel)( GLenum mode );

// Raster funcs
void (APIENTRY * bglReadPixels)( GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels );

// Texture mapping
void (APIENTRY * bglTexEnvf)( GLenum target, GLenum pname, GLfloat param );
void (APIENTRY * bglGenTextures)( GLsizei n, GLuint *textures );	// 1.1
void (APIENTRY * bglDeleteTextures)( GLsizei n, const GLuint *textures);	// 1.1
void (APIENTRY * bglBindTexture)( GLenum target, GLuint texture );	// 1.1
void (APIENTRY * bglTexImage1D)( GLenum target, GLint level,
								   GLint internalFormat,
								   GLsizei width, GLint border,
								   GLenum format, GLenum type,
								   const GLvoid *pixels );
void (APIENTRY * bglTexImage2D)( GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels );
void (APIENTRY * bglTexSubImage2D)( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels );	// 1.1
void (APIENTRY * bglTexParameterf)( GLenum target, GLenum pname, GLfloat param );
void (APIENTRY * bglTexParameteri)( GLenum target, GLenum pname, GLint param );
void (APIENTRY * bglGetTexLevelParameteriv)( GLenum target, GLint level, GLenum pname, GLint *params );
void (APIENTRY * bglCompressedTexImage2DARB)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const GLvoid *);
void (APIENTRY * bglGetCompressedTexImageARB)(GLenum, GLint, GLvoid *);

// Buffer objects
void (APIENTRY * bglBindBuffer)(GLenum target, GLuint buffer);
void (APIENTRY * bglBufferData)(GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage);
void (APIENTRY * bglBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data);
void (APIENTRY * bglDeleteBuffers)(GLsizei n, const GLuint * buffers);
void (APIENTRY * bglGenBuffers)(GLsizei n, GLuint * buffers);
void (APIENTRY * bglDrawElements)( GLenum mode, GLsizei count, GLenum type, const GLvoid *indices );
void (APIENTRY * bglEnableVertexAttribArray)(GLuint index);
void (APIENTRY * bglDisableVertexAttribArray)(GLuint index);
void (APIENTRY * bglVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer);

// Shaders
void (APIENTRY * bglActiveTexture)( GLenum texture );
void (APIENTRY * bglAttachShader)(GLuint program, GLuint shader);
void (APIENTRY * bglCompileShader)(GLuint shader);
GLuint (APIENTRY * bglCreateProgram)(void);
GLuint (APIENTRY * bglCreateShader)(GLenum type);
void (APIENTRY * bglDeleteProgram)(GLuint program);
void (APIENTRY * bglDeleteShader)(GLuint shader);
void (APIENTRY * bglDetachShader)(GLuint program, GLuint shader);
GLint (APIENTRY * bglGetAttribLocation)(GLuint program, const GLchar *name);
void (APIENTRY * bglGetProgramiv)(GLuint program, GLenum pname, GLint *params);
void (APIENTRY * bglGetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
void (APIENTRY * bglGetShaderiv)(GLuint shader, GLenum pname, GLint *params);
void (APIENTRY * bglGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
GLint (APIENTRY * bglGetUniformLocation)(GLuint program, const GLchar *name);
void (APIENTRY * bglLinkProgram)(GLuint program);
void (APIENTRY * bglShaderSource)(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
void (APIENTRY * bglUniform1i)(GLint location, GLint v0);
void (APIENTRY * bglUniform1f)(GLint location, GLfloat v0);
void (APIENTRY * bglUniform2f)(GLint location, GLfloat v0, GLfloat v1);
void (APIENTRY * bglUniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
void (APIENTRY * bglUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
void (APIENTRY * bglUseProgram)(GLuint program);

#ifdef RENDERTYPEWIN
// Windows
PROC (WINAPI * bwglGetProcAddress)(LPCSTR);
HGLRC (WINAPI * bwglCreateContext)(HDC);
BOOL (WINAPI * bwglDeleteContext)(HGLRC);
BOOL (WINAPI * bwglMakeCurrent)(HDC,HGLRC);

const char * (WINAPI * bwglGetExtensionsStringARB)(HDC hdc);
BOOL (WINAPI * bwglChoosePixelFormatARB)(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
HGLRC (WINAPI * bwglCreateContextAttribsARB)(HDC hDC, HGLRC hShareContext, const int *attribList);
BOOL (WINAPI * bwglSwapBuffers)(HDC);

#endif


static void * getproc_(const char *s, int *err, int fatal, int extension)
{
	void *t = NULL;
	t = getglprocaddress(s, extension);
	if (!t && fatal) {
		buildprintf("loadglfunctions: failed to find %s\n", s);
		*err = 1;
	}
	return t;
}
#define GETPROC(s)        getproc_(s,&err,1,0)
#define GETPROCSOFT(s)    getproc_(s,&err,0,0)
#define GETPROCEXT(s)     getproc_(s,&err,1,1)
#define GETPROCEXTSOFT(s) getproc_(s,&err,0,1)

int loadglfunctions(int all)
{
	int err=0;

#ifdef RENDERTYPEWIN
	bwglGetProcAddress	= GETPROC("wglGetProcAddress");
	bwglCreateContext	= GETPROC("wglCreateContext");
	bwglDeleteContext	= GETPROC("wglDeleteContext");
	bwglMakeCurrent		= GETPROC("wglMakeCurrent");
	bwglSwapBuffers		= GETPROC("wglSwapBuffers");

	bwglGetExtensionsStringARB = GETPROCEXTSOFT("wglGetExtensionsStringARB");
	bwglChoosePixelFormatARB = GETPROCEXTSOFT("wglChoosePixelFormatARB");
	bwglCreateContextAttribsARB = GETPROCEXTSOFT("wglCreateContextAttribsARB");
#endif

	if (!all) {
		if (err) unloadglfunctions();
		return err;
	}

	bglClearColor		= GETPROC("glClearColor");
	bglClear		= GETPROC("glClear");
	bglColorMask		= GETPROC("glColorMask");
	bglAlphaFunc		= GETPROC("glAlphaFunc");
	bglBlendFunc		= GETPROC("glBlendFunc");
	bglCullFace		= GETPROC("glCullFace");
	bglFrontFace		= GETPROC("glFrontFace");
	bglPolygonOffset	= GETPROC("glPolygonOffset");
	bglPolygonMode		= GETPROC("glPolygonMode");
	bglEnable		= GETPROC("glEnable");
	bglDisable		= GETPROC("glDisable");
	bglGetFloatv		= GETPROC("glGetFloatv");
	bglGetIntegerv		= GETPROC("glGetIntegerv");
	bglPushAttrib		= GETPROC("glPushAttrib");
	bglPopAttrib		= GETPROC("glPopAttrib");
	bglGetError		= GETPROC("glGetError");
	bglGetString		= GETPROC("glGetString");
	bglHint			= GETPROC("glHint");
	bglPixelStorei	= GETPROC("glPixelStorei");

	// Depth
	bglDepthFunc		= GETPROC("glDepthFunc");
	bglDepthMask		= GETPROC("glDepthMask");
	bglDepthRange		= GETPROC("glDepthRange");

	// Matrix
	bglMatrixMode		= GETPROC("glMatrixMode");
	bglOrtho		= GETPROC("glOrtho");
	bglFrustum		= GETPROC("glFrustum");
	bglViewport		= GETPROC("glViewport");
	bglPushMatrix		= GETPROC("glPushMatrix");
	bglPopMatrix		= GETPROC("glPopMatrix");
	bglLoadIdentity		= GETPROC("glLoadIdentity");
	bglLoadMatrixf		= GETPROC("glLoadMatrixf");

	// Drawing
	bglBegin		= GETPROC("glBegin");
	bglEnd			= GETPROC("glEnd");
	bglVertex2f		= GETPROC("glVertex2f");
	bglVertex2i		= GETPROC("glVertex2i");
	bglVertex3d		= GETPROC("glVertex3d");
	bglVertex3fv		= GETPROC("glVertex3fv");
	bglColor4f		= GETPROC("glColor4f");
	bglColor4ub		= GETPROC("glColor4ub");
	bglTexCoord2d		= GETPROC("glTexCoord2d");
	bglTexCoord2f		= GETPROC("glTexCoord2f");

	// Raster funcs
	bglReadPixels		= GETPROC("glReadPixels");

	// Texture mapping
	bglTexEnvf		= GETPROC("glTexEnvf");
	bglGenTextures		= GETPROC("glGenTextures");
	bglDeleteTextures	= GETPROC("glDeleteTextures");
	bglBindTexture		= GETPROC("glBindTexture");
	bglTexImage1D		= GETPROC("glTexImage1D");
	bglTexImage2D		= GETPROC("glTexImage2D");
	bglTexSubImage2D	= GETPROC("glTexSubImage2D");
	bglTexParameterf	= GETPROC("glTexParameterf");
	bglTexParameteri	= GETPROC("glTexParameteri");
	bglGetTexLevelParameteriv = GETPROC("glGetTexLevelParameteriv");
	bglCompressedTexImage2DARB  = GETPROCEXTSOFT("glCompressedTexImage2DARB");
	bglGetCompressedTexImageARB = GETPROCEXTSOFT("glGetCompressedTexImageARB");

	// Buffer objects
	bglBindBuffer		= GETPROCEXT("glBindBuffer");
	bglBufferData		= GETPROCEXT("glBufferData");
	bglBufferSubData	= GETPROCEXT("glBufferSubData");
	bglDeleteBuffers	= GETPROCEXT("glDeleteBuffers");
	bglGenBuffers		= GETPROCEXT("glGenBuffers");
	bglDrawElements		= GETPROCEXT("glDrawElements");
	bglEnableVertexAttribArray = GETPROCEXT("glEnableVertexAttribArray");
	bglDisableVertexAttribArray = GETPROCEXT("glDisableVertexAttribArray");
	bglVertexAttribPointer = GETPROCEXT("glVertexAttribPointer");

	// Shaders
	bglActiveTexture = GETPROCEXT("glActiveTexture");
	bglAttachShader  = GETPROCEXT("glAttachShader");
	bglCompileShader = GETPROCEXT("glCompileShader");
	bglCreateProgram = GETPROCEXT("glCreateProgram");
	bglCreateShader  = GETPROCEXT("glCreateShader");
	bglDeleteProgram = GETPROCEXT("glDeleteProgram");
	bglDeleteShader  = GETPROCEXT("glDeleteShader");
	bglDetachShader  = GETPROCEXT("glDetachShader");
	bglGetAttribLocation = GETPROCEXT("glGetAttribLocation");
	bglGetProgramiv  = GETPROCEXT("glGetProgramiv");
	bglGetProgramInfoLog = GETPROCEXT("glGetProgramInfoLog");
	bglGetShaderiv   = GETPROCEXT("glGetShaderiv");
	bglGetShaderInfoLog = GETPROCEXT("glGetShaderInfoLog");
	bglGetUniformLocation = GETPROCEXT("glGetUniformLocation");
	bglLinkProgram   = GETPROCEXT("glLinkProgram");
	bglShaderSource  = GETPROCEXT("glShaderSource");
	bglUseProgram    = GETPROCEXT("glUseProgram");
	bglUniform1i     = GETPROCEXT("glUniform1i");
	bglUniform1f     = GETPROC("glUniform1f");
	bglUniform2f     = GETPROC("glUniform2f");
	bglUniform3f     = GETPROC("glUniform3f");
	bglUniform4f     = GETPROC("glUniform4f");

	bglCompressedTexImage2DARB  = GETPROCEXTSOFT("glCompressedTexImage2DARB");
	bglGetCompressedTexImageARB = GETPROCEXTSOFT("glGetCompressedTexImageARB");

	if (err) unloadgldriver();
	return err;
}

void unloadglfunctions(void)
{
	bglClearColor		= NULL;
	bglClear		= NULL;
	bglColorMask		= NULL;
	bglAlphaFunc		= NULL;
	bglBlendFunc		= NULL;
	bglCullFace		= NULL;
	bglFrontFace		= NULL;
	bglPolygonOffset	= NULL;
	bglPolygonMode   = NULL;
	bglEnable		= NULL;
	bglDisable		= NULL;
	bglGetFloatv		= NULL;
	bglGetIntegerv		= NULL;
	bglPushAttrib		= NULL;
	bglPopAttrib		= NULL;
	bglGetError		= NULL;
	bglGetString		= NULL;
	bglHint			= NULL;
	bglPixelStorei	= NULL;

	// Depth
	bglDepthFunc		= NULL;
	bglDepthMask		= NULL;
	bglDepthRange		= NULL;

	// Matrix
	bglMatrixMode		= NULL;
	bglOrtho		= NULL;
	bglFrustum		= NULL;
	bglViewport		= NULL;
	bglPushMatrix		= NULL;
	bglPopMatrix		= NULL;
	bglLoadIdentity		= NULL;
	bglLoadMatrixf		= NULL;

	// Drawing
	bglBegin		= NULL;
	bglEnd			= NULL;
	bglVertex2f		= NULL;
	bglVertex2i		= NULL;
	bglVertex3d		= NULL;
	bglVertex3fv		= NULL;
	bglColor4f		= NULL;
	bglColor4ub		= NULL;
	bglTexCoord2d		= NULL;
	bglTexCoord2f		= NULL;

	// Lighting
	bglShadeModel		= NULL;

	// Raster funcs
	bglReadPixels		= NULL;

	// Texture mapping
	bglTexEnvf		= NULL;
	bglGenTextures		= NULL;
	bglDeleteTextures	= NULL;
	bglBindTexture		= NULL;
	bglTexImage1D		= NULL;
	bglTexImage2D		= NULL;
	bglTexSubImage2D	= NULL;
	bglTexParameterf	= NULL;
	bglTexParameteri	= NULL;
	bglGetTexLevelParameteriv   = NULL;
	bglCompressedTexImage2DARB  = NULL;
	bglGetCompressedTexImageARB = NULL;

	// Buffer objects
	bglBindBuffer		= NULL;
	bglBufferData		= NULL;
	bglBufferSubData	= NULL;
	bglDeleteBuffers	= NULL;
	bglGenBuffers		= NULL;
	bglDrawElements		= NULL;
	bglEnableVertexAttribArray = NULL;
	bglDisableVertexAttribArray = NULL;
	bglVertexAttribPointer = NULL;

	// Shaders
	bglActiveTexture = NULL;
	bglAttachShader  = NULL;
	bglCompileShader = NULL;
	bglCreateProgram = NULL;
	bglCreateShader  = NULL;
	bglDeleteProgram = NULL;
	bglDeleteShader  = NULL;
	bglDetachShader  = NULL;
	bglGetAttribLocation = NULL;
	bglGetProgramiv  = NULL;
	bglGetProgramInfoLog = NULL;
	bglGetShaderiv   = NULL;
	bglGetShaderInfoLog = NULL;
	bglGetUniformLocation = NULL;
	bglLinkProgram   = NULL;
	bglShaderSource  = NULL;
	bglUseProgram    = NULL;
	bglUniform1i     = NULL;
	bglUniform1f     = NULL;
	bglUniform2f     = NULL;
	bglUniform3f     = NULL;
	bglUniform4f     = NULL;

#ifdef RENDERTYPEWIN
	bwglCreateContext	= NULL;
	bwglDeleteContext	= NULL;
	bwglGetProcAddress	= NULL;
	bwglMakeCurrent		= NULL;

	bwglGetExtensionsStringARB = NULL;
	bwglChoosePixelFormatARB = NULL;
	bwglCreateContextAttribsARB = NULL;
	bwglSwapBuffers		= NULL;
#endif
}

#else

int loadglfunctions(void) { return 0; }
void unloadglfunctions(void) { }

#endif  //DYNAMIC_OPENGL

GLuint glbuild_compile_shader(GLuint type, const GLchar *source)
{
	GLuint shader;
	GLint status;

	shader = bglCreateShader(type);
	if (!shader) {
		return 0;
	}

	bglShaderSource(shader, 1, &source, NULL);
	bglCompileShader(shader);

	bglGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		GLint loglen = 0;
		GLchar *logtext = NULL;

		bglGetShaderiv(shader, GL_INFO_LOG_LENGTH, &loglen);

		logtext = (GLchar *)malloc(loglen);
		bglGetShaderInfoLog(shader, loglen, &loglen, logtext);
		buildprintf("GL shader compile error: %s\n", logtext);
		free(logtext);

		bglDeleteShader(shader);
		return 0;
	}

	return shader;
}

GLuint glbuild_link_program(int shadercount, GLuint *shaders)
{
	GLuint program;
	GLint status;
	int i;

	program = bglCreateProgram();
	if (!program) {
		return 0;
	}

	for (i = 0; i < shadercount; i++) {
		bglAttachShader(program, shaders[i]);
	}

	bglLinkProgram(program);

	bglGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		GLint loglen = 0;
		GLchar *logtext = NULL;

		bglGetProgramiv(program, GL_INFO_LOG_LENGTH, &loglen);

		logtext = (GLchar *)malloc(loglen);
		bglGetProgramInfoLog(program, loglen, &loglen, logtext);
		buildprintf("glbuild_link_program: link error: %s\n", logtext);
		free(logtext);

		bglDeleteProgram(program);
		return 0;
	}

	for (i = 0; i < shadercount; i++) {
		bglDetachShader(program, shaders[i]);
	}

	return program;
}

int glbuild_prepare_8bit_shader(GLuint *paltex, GLuint *frametex, GLuint *program, int resx, int resy)
{
	GLuint shader = 0, prog = 0;
	GLint status = 0;

	static const GLchar *shadersrc = \
	"#version 110\n"
	"uniform sampler1D palette;\n"
	"uniform sampler2D frame;\n"
	"void main(void)\n"
	"{\n"
	"  float pixelvalue;\n"
	"  vec3 palettevalue;\n"
	"  pixelvalue = texture2D(frame, gl_TexCoord[0].xy).r;\n"
	"  palettevalue = texture1D(palette, pixelvalue).rgb;\n"
	"  gl_FragColor = vec4(palettevalue, 1.0);\n"
	"}\n";

	// Initialise texture objects for the palette and framebuffer.
	// The textures will be uploaded on the first rendered frame.
	bglGenTextures(1, paltex);
	bglActiveTexture(GL_TEXTURE1);
	bglBindTexture(GL_TEXTURE_1D, *paltex);
	bglTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);	// Allocates memory.
	bglTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	bglTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	bglGenTextures(1, frametex);
	bglActiveTexture(GL_TEXTURE0);
	bglBindTexture(GL_TEXTURE_2D, *frametex);
	bglTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, resx, resy, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);	// Allocates memory.
	bglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	bglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	// Prepare the fragment shader.
	shader = glbuild_compile_shader(GL_FRAGMENT_SHADER, shadersrc);
	if (!shader) {
		return -1;
	}

	// Prepare the shader program.
	prog = glbuild_link_program(1, &shader);
	if (!prog) {
		bglDeleteShader(shader);
		return -1;
	}

	// Shaders were detached in glbuild_link_program.
	bglDeleteShader(shader);

	// Connect the textures to the program.
	bglUseProgram(prog);
	bglUniform1i(bglGetUniformLocation(prog, "palette"), 1);
	bglUniform1i(bglGetUniformLocation(prog, "frame"), 0);

	*program = prog;
	return 0;
}


#endif  //USE_OPENGL
