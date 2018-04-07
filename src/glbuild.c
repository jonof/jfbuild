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

// Fog
void (APIENTRY * bglFogf)( GLenum pname, GLfloat param );
void (APIENTRY * bglFogi)( GLenum pname, GLint param );
void (APIENTRY * bglFogfv)( GLenum pname, const GLfloat *params );

// Shaders
void (APIENTRY * bglActiveTexture)( GLenum texture );
void (APIENTRY * bglAttachShader)(GLuint program, GLuint shader);
void (APIENTRY * bglCompileShader)(GLuint shader);
GLuint (APIENTRY * bglCreateProgram)(void);
GLuint (APIENTRY * bglCreateShader)(GLenum type);
void (APIENTRY * bglDeleteProgram)(GLuint program);
void (APIENTRY * bglDeleteShader)(GLuint shader);
void (APIENTRY * bglDetachShader)(GLuint program, GLuint shader);
void (APIENTRY * bglGetProgramiv)(GLuint program, GLenum pname, GLint *params);
void (APIENTRY * bglGetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
void (APIENTRY * bglGetShaderiv)(GLuint shader, GLenum pname, GLint *params);
void (APIENTRY * bglGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
GLint (APIENTRY * bglGetUniformLocation)(GLuint program, const GLchar *name);
void (APIENTRY * bglLinkProgram)(GLuint program);
void (APIENTRY * bglShaderSource)(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
void (APIENTRY * bglUniform1i)(GLint location, GLint v0);
void (APIENTRY * bglUseProgram)(GLuint program);

#ifdef RENDERTYPEWIN
// Windows
HGLRC (WINAPI * bwglCreateContext)(HDC);
BOOL (WINAPI * bwglDeleteContext)(HGLRC);
PROC (WINAPI * bwglGetProcAddress)(LPCSTR);
BOOL (WINAPI * bwglMakeCurrent)(HDC,HGLRC);

BOOL (WINAPI * bwglSwapBuffers)(HDC);
int (WINAPI * bwglChoosePixelFormat)(HDC,CONST PIXELFORMATDESCRIPTOR*);
int (WINAPI * bwglDescribePixelFormat)(HDC,int,UINT,LPPIXELFORMATDESCRIPTOR);
int (WINAPI * bwglGetPixelFormat)(HDC);
BOOL (WINAPI * bwglSetPixelFormat)(HDC,int,const PIXELFORMATDESCRIPTOR*);

#endif


static void * getproc_(const char *s, int *err, int fatal, int extension)
{
	void *t = NULL;
	t = getglprocaddress(s, extension);
	if (!t && fatal) {
		buildprintf("Failed to find %s\n", s);
		*err = 1;
	}
	return t;
}
#define GETPROC(s)        getproc_(s,&err,1,0)
#define GETPROCSOFT(s)    getproc_(s,&err,0,0)
#define GETPROCEXT(s)     getproc_(s,&err,1,1)
#define GETPROCEXTSOFT(s) getproc_(s,&err,0,1)

int loadglfunctions(void)
{
	int err=0;

#ifdef RENDERTYPEWIN
	bwglCreateContext	= GETPROC("wglCreateContext");
	bwglDeleteContext	= GETPROC("wglDeleteContext");
	bwglGetProcAddress	= GETPROC("wglGetProcAddress");
	bwglMakeCurrent		= GETPROC("wglMakeCurrent");

	bwglSwapBuffers		= GETPROC("wglSwapBuffers");
	bwglChoosePixelFormat	= GETPROC("wglChoosePixelFormat");
	bwglDescribePixelFormat	= GETPROC("wglDescribePixelFormat");
	bwglGetPixelFormat	= GETPROC("wglGetPixelFormat");
	bwglSetPixelFormat	= GETPROC("wglSetPixelFormat");
#endif

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

	// Lighting
	bglShadeModel		= GETPROC("glShadeModel");

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

	// Fog
	bglFogf			= GETPROC("glFogf");
	bglFogi			= GETPROC("glFogi");
	bglFogfv		= GETPROC("glFogfv");

	// Shaders
	bglActiveTexture = GETPROC("glActiveTexture");
	bglAttachShader  = GETPROC("glAttachShader");
	bglCompileShader = GETPROC("glCompileShader");
	bglCreateProgram = GETPROC("glCreateProgram");
	bglCreateShader  = GETPROC("glCreateShader");
	bglDeleteProgram = GETPROC("glDeleteProgram");
	bglDeleteShader  = GETPROC("glDeleteShader");
	bglDetachShader  = GETPROC("glDetachShader");
	bglGetProgramiv  = GETPROC("glGetProgramiv");
	bglGetProgramInfoLog = GETPROC("glGetProgramInfoLog");
	bglGetShaderiv   = GETPROC("glGetShaderiv");
	bglGetShaderInfoLog = GETPROC("glGetShaderInfoLog");
	bglGetUniformLocation = GETPROC("glGetUniformLocation");
	bglLinkProgram   = GETPROC("glLinkProgram");
	bglShaderSource  = GETPROC("glShaderSource");
	bglUseProgram    = GETPROC("glUseProgram");
	bglUniform1i     = GETPROC("glUniform1i");

	loadglextensions();

	if (err) unloadgldriver();
	return err;
}

int loadglextensions(void)
{
	int err = 0;

	bglCompressedTexImage2DARB  = GETPROCEXTSOFT("glCompressedTexImage2DARB");
	bglGetCompressedTexImageARB = GETPROCEXTSOFT("glGetCompressedTexImageARB");

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

	// Fog
	bglFogf			= NULL;
	bglFogi			= NULL;
	bglFogfv		= NULL;

	// Shaders
	bglActiveTexture = NULL;
	bglAttachShader  = NULL;
	bglCompileShader = NULL;
	bglCreateProgram = NULL;
	bglCreateShader  = NULL;
	bglDeleteProgram = NULL;
	bglDeleteShader  = NULL;
	bglDetachShader  = NULL;
	bglGetProgramiv  = NULL;
	bglGetProgramInfoLog = NULL;
	bglGetShaderiv   = NULL;
	bglGetShaderInfoLog = NULL;
	bglGetUniformLocation = NULL;
	bglLinkProgram   = NULL;
	bglShaderSource  = NULL;
	bglUseProgram    = NULL;
	bglUniform1i     = NULL;

#ifdef RENDERTYPEWIN
	bwglCreateContext	= NULL;
	bwglDeleteContext	= NULL;
	bwglGetProcAddress	= NULL;
	bwglMakeCurrent		= NULL;

	bwglSwapBuffers		= NULL;
	bwglChoosePixelFormat	= NULL;
	bwglDescribePixelFormat	= NULL;
	bwglGetPixelFormat	= NULL;
	bwglSetPixelFormat	= NULL;
#endif
}

#else

int loadglfunctions(void) { return 0; }
int loadglextensions(void) { return 0; }
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
		buildprintf("GL program link error: %s\n", logtext);
		free(logtext);

		bglDeleteProgram(program);
		return 0;
	}

	for (i = 0; i < shadercount; i++) {
		bglDetachShader(program, shaders[i]);
	}

	return program;
}

#endif  //USE_OPENGL
