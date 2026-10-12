#include "linux_gl.h"
#include <stdio.h>

#if !defined(__APPLE__)

void (APIENTRY *glGenTextures_ptr)(GLsizei n, GLuint *textures) = NULL;
void (APIENTRY *glBindTexture_ptr)(GLenum target, GLuint texture) = NULL;
void (APIENTRY *glTexParameteri_ptr)(GLenum target, GLenum pname, GLint param) = NULL;
void (APIENTRY *glTexImage2D_ptr)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels) = NULL;
void (APIENTRY *glTexSubImage2D_ptr)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels) = NULL;
void (APIENTRY *glDeleteTextures_ptr)(GLsizei n, const GLuint *textures) = NULL;
void (APIENTRY *glViewport_ptr)(GLint x, GLint y, GLsizei width, GLsizei height) = NULL;
void (APIENTRY *glClearColor_ptr)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) = NULL;
void (APIENTRY *glClear_ptr)(GLbitfield mask) = NULL;
void (APIENTRY *glEnable_ptr)(GLenum cap) = NULL;
void (APIENTRY *glDisable_ptr)(GLenum cap) = NULL;
void (APIENTRY *glBlendFunc_ptr)(GLenum sfactor, GLenum dfactor) = NULL;
void (APIENTRY *glDrawArrays_ptr)(GLenum mode, GLint first, GLsizei count) = NULL;
const GLubyte* (APIENTRY *glGetString_ptr)(GLenum name) = NULL;
void (APIENTRY *glBindAttribLocation_ptr)(GLuint program, GLuint index, const GLchar *name) = NULL;

PFNGLGENVERTEXARRAYSPROC glGenVertexArrays_ptr = NULL;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray_ptr = NULL;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays_ptr = NULL;
PFNGLGENBUFFERSPROC glGenBuffers_ptr = NULL;
PFNGLBINDBUFFERPROC glBindBuffer_ptr = NULL;
PFNGLBUFFERDATAPROC glBufferData_ptr = NULL;
PFNGLBUFFERSUBDATAPROC glBufferSubData_ptr = NULL;
PFNGLDELETEBUFFERSPROC glDeleteBuffers_ptr = NULL;
PFNGLMAPBUFFERPROC glMapBuffer_ptr = NULL;
PFNGLUNMAPBUFFERPROC glUnmapBuffer_ptr = NULL;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer_ptr = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray_ptr = NULL;
PFNGLCREATESHADERPROC glCreateShader_ptr = NULL;
PFNGLSHADERSOURCEPROC glShaderSource_ptr = NULL;
PFNGLCOMPILESHADERPROC glCompileShader_ptr = NULL;
PFNGLGETSHADERIVPROC glGetShaderiv_ptr = NULL;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog_ptr = NULL;
PFNGLDELETESHADERPROC glDeleteShader_ptr = NULL;
PFNGLCREATEPROGRAMPROC glCreateProgram_ptr = NULL;
PFNGLATTACHSHADERPROC glAttachShader_ptr = NULL;
PFNGLLINKPROGRAMPROC glLinkProgram_ptr = NULL;
PFNGLGETPROGRAMIVPROC glGetProgramiv_ptr = NULL;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog_ptr = NULL;
PFNGLUSEPROGRAMPROC glUseProgram_ptr = NULL;
PFNGLDELETEPROGRAMPROC glDeleteProgram_ptr = NULL;

typedef void (APIENTRY *generic_gl_func)(void);

#define LOAD_PROC(name, type) \
    name##_ptr = (type)SDL_GL_GetProcAddress(#name); \
    if (!name##_ptr) { \
        fprintf(stderr, "[OpenGL] Impossibile caricare: %s\n", #name); \
        ok = false; \
    }

#endif

bool linux_gl_load_procs(void) {
#if defined(__APPLE__)
    return true;
#else
    bool ok = true;
    LOAD_PROC(glGenTextures, void (APIENTRY *)(GLsizei, GLuint*));
    LOAD_PROC(glBindTexture, void (APIENTRY *)(GLenum, GLuint));
    LOAD_PROC(glTexParameteri, void (APIENTRY *)(GLenum, GLenum, GLint));
    LOAD_PROC(glTexImage2D, void (APIENTRY *)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*));
    LOAD_PROC(glTexSubImage2D, void (APIENTRY *)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*));
    LOAD_PROC(glDeleteTextures, void (APIENTRY *)(GLsizei, const GLuint*));
    LOAD_PROC(glViewport, void (APIENTRY *)(GLint, GLint, GLsizei, GLsizei));
    LOAD_PROC(glClearColor, void (APIENTRY *)(GLfloat, GLfloat, GLfloat, GLfloat));
    LOAD_PROC(glClear, void (APIENTRY *)(GLbitfield));
    LOAD_PROC(glEnable, void (APIENTRY *)(GLenum));
    LOAD_PROC(glDisable, void (APIENTRY *)(GLenum));
    LOAD_PROC(glBlendFunc, void (APIENTRY *)(GLenum, GLenum));
    LOAD_PROC(glDrawArrays, void (APIENTRY *)(GLenum, GLint, GLsizei));
    LOAD_PROC(glGetString, const GLubyte* (APIENTRY *)(GLenum));
    LOAD_PROC(glBindAttribLocation, void (APIENTRY *)(GLuint, GLuint, const GLchar*));

    glGenVertexArrays_ptr = (PFNGLGENVERTEXARRAYSPROC)SDL_GL_GetProcAddress("glGenVertexArrays");
    if (!glGenVertexArrays_ptr) glGenVertexArrays_ptr = (PFNGLGENVERTEXARRAYSPROC)SDL_GL_GetProcAddress("glGenVertexArraysARB");
    glBindVertexArray_ptr = (PFNGLBINDVERTEXARRAYPROC)SDL_GL_GetProcAddress("glBindVertexArray");
    if (!glBindVertexArray_ptr) glBindVertexArray_ptr = (PFNGLBINDVERTEXARRAYPROC)SDL_GL_GetProcAddress("glBindVertexArrayARB");
    glDeleteVertexArrays_ptr = (PFNGLDELETEVERTEXARRAYSPROC)SDL_GL_GetProcAddress("glDeleteVertexArrays");
    if (!glDeleteVertexArrays_ptr) glDeleteVertexArrays_ptr = (PFNGLDELETEVERTEXARRAYSPROC)SDL_GL_GetProcAddress("glDeleteVertexArraysARB");
    LOAD_PROC(glGenBuffers, PFNGLGENBUFFERSPROC);
    LOAD_PROC(glBindBuffer, PFNGLBINDBUFFERPROC);
    LOAD_PROC(glBufferData, PFNGLBUFFERDATAPROC);
    LOAD_PROC(glBufferSubData, PFNGLBUFFERSUBDATAPROC);
    LOAD_PROC(glDeleteBuffers, PFNGLDELETEBUFFERSPROC);
    LOAD_PROC(glMapBuffer, PFNGLMAPBUFFERPROC);
    LOAD_PROC(glUnmapBuffer, PFNGLUNMAPBUFFERPROC);
    LOAD_PROC(glVertexAttribPointer, PFNGLVERTEXATTRIBPOINTERPROC);
    LOAD_PROC(glEnableVertexAttribArray, PFNGLENABLEVERTEXATTRIBARRAYPROC);
    LOAD_PROC(glCreateShader, PFNGLCREATESHADERPROC);
    LOAD_PROC(glShaderSource, PFNGLSHADERSOURCEPROC);
    LOAD_PROC(glCompileShader, PFNGLCOMPILESHADERPROC);
    LOAD_PROC(glGetShaderiv, PFNGLGETSHADERIVPROC);
    LOAD_PROC(glGetShaderInfoLog, PFNGLGETSHADERINFOLOGPROC);
    LOAD_PROC(glDeleteShader, PFNGLDELETESHADERPROC);
    LOAD_PROC(glCreateProgram, PFNGLCREATEPROGRAMPROC);
    LOAD_PROC(glAttachShader, PFNGLATTACHSHADERPROC);
    LOAD_PROC(glLinkProgram, PFNGLLINKPROGRAMPROC);
    LOAD_PROC(glGetProgramiv, PFNGLGETPROGRAMIVPROC);
    LOAD_PROC(glGetProgramInfoLog, PFNGLGETPROGRAMINFOLOGPROC);
    LOAD_PROC(glUseProgram, PFNGLUSEPROGRAMPROC);
    LOAD_PROC(glDeleteProgram, PFNGLDELETEPROGRAMPROC);

    return ok;
#endif
}
