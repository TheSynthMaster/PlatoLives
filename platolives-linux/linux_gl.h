#ifndef LINUX_GL_H
#define LINUX_GL_H

#include <stdbool.h>

#define GL_SILENCE_DEPRECATION
#include <SDL2/SDL.h>

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

/* Puntatori a TUTTE le funzioni OpenGL usate, incluse le 1.1 storiche */
extern void (APIENTRY *glGenTextures_ptr)(GLsizei n, GLuint *textures);
extern void (APIENTRY *glBindTexture_ptr)(GLenum target, GLuint texture);
extern void (APIENTRY *glTexParameteri_ptr)(GLenum target, GLenum pname, GLint param);
extern void (APIENTRY *glTexImage2D_ptr)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
extern void (APIENTRY *glTexSubImage2D_ptr)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);
extern void (APIENTRY *glDeleteTextures_ptr)(GLsizei n, const GLuint *textures);
extern void (APIENTRY *glViewport_ptr)(GLint x, GLint y, GLsizei width, GLsizei height);
extern void (APIENTRY *glClearColor_ptr)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
extern void (APIENTRY *glClear_ptr)(GLbitfield mask);
extern void (APIENTRY *glEnable_ptr)(GLenum cap);
extern void (APIENTRY *glDisable_ptr)(GLenum cap);
extern void (APIENTRY *glBlendFunc_ptr)(GLenum sfactor, GLenum dfactor);
extern void (APIENTRY *glDrawArrays_ptr)(GLenum mode, GLint first, GLsizei count);

extern PFNGLGENVERTEXARRAYSPROC glGenVertexArrays_ptr;
extern PFNGLBINDVERTEXARRAYPROC glBindVertexArray_ptr;
extern PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays_ptr;
extern PFNGLGENBUFFERSPROC glGenBuffers_ptr;
extern PFNGLBINDBUFFERPROC glBindBuffer_ptr;
extern PFNGLBUFFERDATAPROC glBufferData_ptr;
extern PFNGLBUFFERSUBDATAPROC glBufferSubData_ptr;
extern PFNGLDELETEBUFFERSPROC glDeleteBuffers_ptr;
extern PFNGLMAPBUFFERPROC glMapBuffer_ptr;
extern PFNGLUNMAPBUFFERPROC glUnmapBuffer_ptr;
extern PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer_ptr;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray_ptr;
extern PFNGLCREATESHADERPROC glCreateShader_ptr;
extern PFNGLSHADERSOURCEPROC glShaderSource_ptr;
extern PFNGLCOMPILESHADERPROC glCompileShader_ptr;
extern PFNGLGETSHADERIVPROC glGetShaderiv_ptr;
extern PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog_ptr;
extern PFNGLDELETESHADERPROC glDeleteShader_ptr;
extern PFNGLCREATEPROGRAMPROC glCreateProgram_ptr;
extern PFNGLATTACHSHADERPROC glAttachShader_ptr;
extern PFNGLLINKPROGRAMPROC glLinkProgram_ptr;
extern PFNGLGETPROGRAMIVPROC glGetProgramiv_ptr;
extern PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog_ptr;
extern PFNGLUSEPROGRAMPROC glUseProgram_ptr;
extern PFNGLDELETEPROGRAMPROC glDeleteProgram_ptr;

#define glGenTextures            glGenTextures_ptr
#define glBindTexture            glBindTexture_ptr
#define glTexParameteri          glTexParameteri_ptr
#define glTexImage2D             glTexImage2D_ptr
#define glTexSubImage2D          glTexSubImage2D_ptr
#define glDeleteTextures         glDeleteTextures_ptr
#define glViewport               glViewport_ptr
#define glClearColor             glClearColor_ptr
#define glClear                  glClear_ptr
#define glEnable                 glEnable_ptr
#define glDisable                glDisable_ptr
#define glBlendFunc              glBlendFunc_ptr
#define glDrawArrays             glDrawArrays_ptr

#define glGenVertexArrays        glGenVertexArrays_ptr
#define glBindVertexArray        glBindVertexArray_ptr
#define glDeleteVertexArrays     glDeleteVertexArrays_ptr
#define glGenBuffers             glGenBuffers_ptr
#define glBindBuffer             glBindBuffer_ptr
#define glBufferData             glBufferData_ptr
#define glBufferSubData          glBufferSubData_ptr
#define glDeleteBuffers          glDeleteBuffers_ptr
#define glMapBuffer              glMapBuffer_ptr
#define glUnmapBuffer            glUnmapBuffer_ptr
#define glVertexAttribPointer    glVertexAttribPointer_ptr
#define glEnableVertexAttribArray glEnableVertexAttribArray_ptr
#define glCreateShader           glCreateShader_ptr
#define glShaderSource           glShaderSource_ptr
#define glCompileShader          glCompileShader_ptr
#define glGetShaderiv            glGetShaderiv_ptr
#define glGetShaderInfoLog       glGetShaderInfoLog_ptr
#define glDeleteShader           glDeleteShader_ptr
#define glCreateProgram          glCreateProgram_ptr
#define glAttachShader           glAttachShader_ptr
#define glLinkProgram            glLinkProgram_ptr
#define glGetProgramiv           glGetProgramiv_ptr
#define glGetProgramInfoLog      glGetProgramInfoLog_ptr
#define glUseProgram             glUseProgram_ptr
#define glDeleteProgram          glDeleteProgram_ptr

#endif

bool linux_gl_load_procs(void);

#endif /* LINUX_GL_H */
