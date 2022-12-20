#ifndef __build_h__
#error Include build.h first.
#endif

#ifndef __glbuild_priv_h__
#define __glbuild_priv_h__

#if USE_OPENGL

#include "glbuild.h"

#if (USE_OPENGL == USE_GLES2)
#  include <GLES2/gl2.h>
#  include <GLES2/gl2ext.h>
#  define GL_CLAMP GL_CLAMP_TO_EDGE
#  define GL_BGRA GL_BGRA_EXT
#  define APIENTRY GL_APIENTRY
//#elif (USE_OPENGL == USE_GL3)
//#  include <GL/glcorearb.h>
#else
#  if defined(__APPLE__)
#    define GL_GLEXT_LEGACY // Prevent OpenGL/glext.h from being included.
#    include <OpenGL/gl.h>
#    undef GL_VERSION_1_1   // Undefining all these lets glext.h in our tree fill in the gaps.
#    undef GL_VERSION_1_2
#    undef GL_VERSION_1_3
#    undef GL_VERSION_1_4
#    undef GL_VERSION_1_5
#    undef GL_VERSION_2_0
#    undef GL_VERSION_2_1
#  elif defined(_MSC_VER)
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#    include <GL/gl.h>
#  else
#    include <GL/gl.h>
#  endif
#  include "glext.h"
   typedef void (APIENTRYP PFNGLCULLFACEPROC) (GLenum mode);
   typedef void (APIENTRYP PFNGLFRONTFACEPROC) (GLenum mode);
   typedef void (APIENTRYP PFNGLHINTPROC) (GLenum target, GLenum mode);
   typedef void (APIENTRYP PFNGLPOLYGONMODEPROC) (GLenum face, GLenum mode);
   typedef void (APIENTRYP PFNGLSCISSORPROC) (GLint x, GLint y, GLsizei width, GLsizei height);
   typedef void (APIENTRYP PFNGLTEXPARAMETERFPROC) (GLenum target, GLenum pname, GLfloat param);
   typedef void (APIENTRYP PFNGLTEXPARAMETERIPROC) (GLenum target, GLenum pname, GLint param);
   typedef void (APIENTRYP PFNGLTEXIMAGE2DPROC) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
   typedef void (APIENTRYP PFNGLCLEARPROC) (GLbitfield mask);
   typedef void (APIENTRYP PFNGLCLEARCOLORPROC) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
   typedef void (APIENTRYP PFNGLCOLORMASKPROC) (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
   typedef void (APIENTRYP PFNGLDEPTHMASKPROC) (GLboolean flag);
   typedef void (APIENTRYP PFNGLDISABLEPROC) (GLenum cap);
   typedef void (APIENTRYP PFNGLENABLEPROC) (GLenum cap);
   typedef void (APIENTRYP PFNGLBLENDFUNCPROC) (GLenum sfactor, GLenum dfactor);
   typedef void (APIENTRYP PFNGLDEPTHFUNCPROC) (GLenum func);
   typedef void (APIENTRYP PFNGLPIXELSTOREIPROC) (GLenum pname, GLint param);
   typedef void (APIENTRYP PFNGLREADPIXELSPROC) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels);
   typedef GLenum (APIENTRYP PFNGLGETERRORPROC) (void);
   typedef void (APIENTRYP PFNGLGETFLOATVPROC) (GLenum pname, GLfloat *data);
   typedef void (APIENTRYP PFNGLGETINTEGERVPROC) (GLenum pname, GLint *data);
   typedef const GLubyte *(APIENTRYP PFNGLGETSTRINGPROC) (GLenum name);
   typedef void (APIENTRYP PFNGLDEPTHRANGEPROC) (GLdouble n, GLdouble f);
   typedef void (APIENTRYP PFNGLVIEWPORTPROC) (GLint x, GLint y, GLsizei width, GLsizei height);
   typedef void (APIENTRYP PFNGLDRAWELEMENTSPROC) (GLenum mode, GLsizei count, GLenum type, const void *indices);
   typedef void (APIENTRYP PFNGLPOLYGONOFFSETPROC) (GLfloat factor, GLfloat units);
   typedef void (APIENTRYP PFNGLTEXSUBIMAGE2DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);
   typedef void (APIENTRYP PFNGLBINDTEXTUREPROC) (GLenum target, GLuint texture);
   typedef void (APIENTRYP PFNGLDELETETEXTURESPROC) (GLsizei n, const GLuint *textures);
   typedef void (APIENTRYP PFNGLGENTEXTURESPROC) (GLsizei n, GLuint *textures);
#endif

struct glbuild_funcs {
    PFNGLCLEARCOLORPROC glClearColor;
    PFNGLCLEARPROC glClear;
    PFNGLCOLORMASKPROC glColorMask;
    PFNGLBLENDFUNCPROC glBlendFunc;
    PFNGLCULLFACEPROC glCullFace;
    PFNGLFRONTFACEPROC glFrontFace;
    PFNGLPOLYGONOFFSETPROC glPolygonOffset;
#if (USE_OPENGL != USE_GLES2)
    PFNGLPOLYGONMODEPROC glPolygonMode;
#endif
    PFNGLENABLEPROC glEnable;
    PFNGLDISABLEPROC glDisable;
    PFNGLGETFLOATVPROC glGetFloatv;
    PFNGLGETINTEGERVPROC glGetIntegerv;
    PFNGLGETSTRINGPROC glGetString;
#if (USE_OPENGL == USE_GL3)
    PFNGLGETSTRINGIPROC glGetStringi;
#endif
    PFNGLGETERRORPROC glGetError;
    PFNGLHINTPROC glHint;
    PFNGLPIXELSTOREIPROC glPixelStorei;
    PFNGLVIEWPORTPROC glViewport;
    PFNGLSCISSORPROC glScissor;
#if (USE_OPENGL != USE_GLES2)
    PFNGLMINSAMPLESHADINGARBPROC glMinSampleShadingARB;
#endif

    // Depth
    PFNGLDEPTHFUNCPROC glDepthFunc;
    PFNGLDEPTHMASKPROC glDepthMask;
#if (USE_OPENGL == USE_GLES2)
    PFNGLDEPTHRANGEFPROC glDepthRangef;
#else
    PFNGLDEPTHRANGEPROC glDepthRange;
#endif

    // Raster funcs
    PFNGLREADPIXELSPROC glReadPixels;

    // Texture mapping
    PFNGLGENTEXTURESPROC glGenTextures;
    PFNGLDELETETEXTURESPROC glDeleteTextures;
    PFNGLBINDTEXTUREPROC glBindTexture;
    PFNGLTEXIMAGE2DPROC glTexImage2D;
    PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D;
    PFNGLTEXPARAMETERFPROC glTexParameterf;
    PFNGLTEXPARAMETERIPROC glTexParameteri;
    PFNGLCOMPRESSEDTEXIMAGE2DPROC glCompressedTexImage2D;

    // Buffer objects
    PFNGLBINDBUFFERPROC glBindBuffer;
    PFNGLBUFFERDATAPROC glBufferData;
    PFNGLBUFFERSUBDATAPROC glBufferSubData;
    PFNGLDELETEBUFFERSPROC glDeleteBuffers;
    PFNGLGENBUFFERSPROC glGenBuffers;
    PFNGLDRAWELEMENTSPROC glDrawElements;
    PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
    PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
    PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
#if (USE_OPENGL == USE_GL3)
    PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
    PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
    PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
#endif

    // Shaders
    PFNGLACTIVETEXTUREPROC glActiveTexture;
    PFNGLATTACHSHADERPROC glAttachShader;
    PFNGLCOMPILESHADERPROC glCompileShader;
    PFNGLCREATEPROGRAMPROC glCreateProgram;
    PFNGLCREATESHADERPROC glCreateShader;
    PFNGLDELETEPROGRAMPROC glDeleteProgram;
    PFNGLDELETESHADERPROC glDeleteShader;
    PFNGLDETACHSHADERPROC glDetachShader;
    PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
    PFNGLGETPROGRAMIVPROC glGetProgramiv;
    PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
    PFNGLGETSHADERIVPROC glGetShaderiv;
    PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
    PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
    PFNGLLINKPROGRAMPROC glLinkProgram;
    PFNGLSHADERSOURCEPROC glShaderSource;
    PFNGLUNIFORM1IPROC glUniform1i;
    PFNGLUNIFORM1FPROC glUniform1f;
    PFNGLUNIFORM2FPROC glUniform2f;
    PFNGLUNIFORM3FPROC glUniform3f;
    PFNGLUNIFORM4FPROC glUniform4f;
    PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
    PFNGLUSEPROGRAMPROC glUseProgram;

    // Debug extension.
#if defined(GL_KHR_debug)
    #if (USE_OPENGL == USE_GLES2)
    PFNGLDEBUGMESSAGECALLBACKKHRPROC glDebugMessageCallbackKHR;
    #else
    PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback;
    #endif
#endif
#if defined(GL_ARB_debug_output)
    PFNGLDEBUGMESSAGECALLBACKARBPROC glDebugMessageCallbackARB;
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

int glbuild_init(void);

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

#endif // __glbuild_priv_h__
