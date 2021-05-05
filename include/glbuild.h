#ifndef __build_h__
#error Include build.h first.
#endif

#ifndef __glbuild_h__
#define __glbuild_h__

#if USE_OPENGL

#if (USE_OPENGL == USE_GLES2)
#  include <GLES2/gl2.h>
#  include <GLES2/gl2ext.h>
#else
#  if defined(_MSC_VER)
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#    include <GL/gl.h>
#  elif defined(__APPLE__)
#    if (USE_OPENGL == USE_GL3)
#      include <stddef.h>
#      include <OpenGL/gl3.h>
#    else
#      include <OpenGL/gl.h>
#    endif
#    define APIENTRY
#  else
#    if (USE_OPENGL == USE_GL3)
#      include <GL/glcorearb.h>
#    else
#      include <GL/gl.h>
#    endif
#  endif
#  ifndef GL_GLEXT_VERSION
#    include "glext.h"
#  endif
#endif

#ifndef APIENTRY
#  define APIENTRY GL_APIENTRY
#endif
#ifndef GL_CLAMP  //ES2
#  define GL_CLAMP GL_CLAMP_TO_EDGE
#endif
#ifndef GL_BGRA
#  define GL_BGRA GL_BGRA_EXT
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY
#  define GL_MAX_TEXTURE_MAX_ANISOTROPY GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#endif

typedef void (APIENTRY *GLBUILD_DEBUGPROC)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
    const GLchar* message, const GLvoid* userParam);

struct glbuild_funcs {
    void (APIENTRY * glClearColor)( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha );
    void (APIENTRY * glClear)( GLbitfield mask );
    void (APIENTRY * glColorMask)( GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha );
    void (APIENTRY * glBlendFunc)( GLenum sfactor, GLenum dfactor );
    void (APIENTRY * glCullFace)( GLenum mode );
    void (APIENTRY * glFrontFace)( GLenum mode );
    void (APIENTRY * glPolygonOffset)( GLfloat factor, GLfloat units );
#if (USE_OPENGL != USE_GLES2)
    void (APIENTRY * glPolygonMode)( GLenum face, GLenum mode );
#endif
    void (APIENTRY * glEnable)( GLenum cap );
    void (APIENTRY * glDisable)( GLenum cap );
    void (APIENTRY * glGetFloatv)( GLenum pname, GLfloat *params );
    void (APIENTRY * glGetIntegerv)( GLenum pname, GLint *params );
    const GLubyte* (APIENTRY * glGetString)( GLenum name );
#if (USE_OPENGL == USE_GL3)
    const GLubyte* (APIENTRY * glGetStringi)(GLenum name, GLuint index);
#endif
    GLenum (APIENTRY * glGetError)( GLvoid );
    void (APIENTRY * glHint)( GLenum target, GLenum mode );
    void (APIENTRY * glPixelStorei)( GLenum pname, GLint param );
    void (APIENTRY * glViewport)( GLint x, GLint y, GLsizei width, GLsizei height );
    void (APIENTRY * glScissor)( GLint x, GLint y, GLsizei width, GLsizei height );

    // Depth
    void (APIENTRY * glDepthFunc)( GLenum func );
    void (APIENTRY * glDepthMask)( GLboolean flag );
#if (USE_OPENGL == USE_GLES2)
    void (APIENTRY * glDepthRangef)( GLclampf near_val, GLclampf far_val );
#else
    void (APIENTRY * glDepthRange)( GLclampd near_val, GLclampd far_val );
#endif

    // Raster funcs
    void (APIENTRY * glReadPixels)( GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels );

    // Texture mapping
    void (APIENTRY * glTexEnvf)( GLenum target, GLenum pname, GLfloat param );
    void (APIENTRY * glGenTextures)( GLsizei n, GLuint *textures );
    void (APIENTRY * glDeleteTextures)( GLsizei n, const GLuint *textures);
    void (APIENTRY * glBindTexture)( GLenum target, GLuint texture );
    void (APIENTRY * glTexImage2D)( GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels );
    void (APIENTRY * glTexSubImage2D)( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels );	// 1.1
    void (APIENTRY * glTexParameterf)( GLenum target, GLenum pname, GLfloat param );
    void (APIENTRY * glTexParameteri)( GLenum target, GLenum pname, GLint param );
    void (APIENTRY * glCompressedTexImage2D)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const GLvoid *);

    // Buffer objects
    void (APIENTRY * glBindBuffer)(GLenum target, GLuint buffer);
    void (APIENTRY * glBufferData)(GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage);
    void (APIENTRY * glBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data);
    void (APIENTRY * glDeleteBuffers)(GLsizei n, const GLuint * buffers);
    void (APIENTRY * glGenBuffers)(GLsizei n, GLuint * buffers);
    void (APIENTRY * glDrawElements)( GLenum mode, GLsizei count, GLenum type, const GLvoid *indices );
    void (APIENTRY * glEnableVertexAttribArray)(GLuint index);
    void (APIENTRY * glDisableVertexAttribArray)(GLuint index);
    void (APIENTRY * glVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer);
#if (USE_OPENGL == USE_GL3)
    void (APIENTRY * glBindVertexArray)(GLuint array);
    void (APIENTRY * glDeleteVertexArrays)(GLsizei n, const GLuint *arrays);
    void (APIENTRY * glGenVertexArrays)(GLsizei n, GLuint *arrays);
#endif

    // Shaders
    void (APIENTRY * glActiveTexture)( GLenum texture );
    void (APIENTRY * glAttachShader)(GLuint program, GLuint shader);
    void (APIENTRY * glCompileShader)(GLuint shader);
    GLuint (APIENTRY * glCreateProgram)(GLvoid);
    GLuint (APIENTRY * glCreateShader)(GLenum type);
    void (APIENTRY * glDeleteProgram)(GLuint program);
    void (APIENTRY * glDeleteShader)(GLuint shader);
    void (APIENTRY * glDetachShader)(GLuint program, GLuint shader);
    GLint (APIENTRY * glGetAttribLocation)(GLuint program, const GLchar *name);
    void (APIENTRY * glGetProgramiv)(GLuint program, GLenum pname, GLint *params);
    void (APIENTRY * glGetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
    void (APIENTRY * glGetShaderiv)(GLuint shader, GLenum pname, GLint *params);
    void (APIENTRY * glGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
    GLint (APIENTRY * glGetUniformLocation)(GLuint program, const GLchar *name);
    void (APIENTRY * glLinkProgram)(GLuint program);
    void (APIENTRY * glShaderSource)(GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
    void (APIENTRY * glUniform1i)(GLint location, GLint v0);
    void (APIENTRY * glUniform1f)(GLint location, GLfloat v0);
    void (APIENTRY * glUniform2f)(GLint location, GLfloat v0, GLfloat v1);
    void (APIENTRY * glUniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
    void (APIENTRY * glUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
    void (APIENTRY * glUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void (APIENTRY * glUseProgram)(GLuint program);

    // Debug extension.
#if (USE_OPENGL == USE_GLES2)
    void (APIENTRY * glDebugMessageCallbackKHR)(GLBUILD_DEBUGPROC callback, const GLvoid* userParam);
#else
    void (APIENTRY * glDebugMessageCallback)(GLBUILD_DEBUGPROC callback, const GLvoid* userParam);
#endif
};

extern struct glbuild_funcs glfunc;

typedef struct {
    GLuint program;
    GLuint paltex;
    GLuint frametex;
    GLuint vao;
    GLint attrib_vertex;    // vec2
    GLint attrib_texcoord;  // vec2
    GLuint buffer_indexes;
    GLuint buffer_elements;
} glbuild8bit;

int glbuild_loadfunctions(void);
void glbuild_unloadfunctions(void);
void glbuild_check_errors(const char *file, int line);
#define GLBUILD_CHECK_ERRORS() glbuild_check_errors(__FILE__, __LINE__)

GLuint glbuild_compile_shader(GLuint type, const GLchar *source);
GLuint glbuild_link_program(int shadercount, GLuint *shaders);
int glbuild_prepare_8bit_shader(glbuild8bit *state, int resx, int resy, int stride, int winx, int winy);  // <0 = error
void glbuild_delete_8bit_shader(glbuild8bit *state);
void glbuild_update_8bit_palette(glbuild8bit *state, const GLvoid *pal);
void glbuild_update_8bit_frame(glbuild8bit *state, const GLvoid *frame, int resx, int resy, int stride);
void glbuild_draw_8bit_frame(glbuild8bit *state);

#endif //USE_OPENGL

#endif // __glbuild_h__
