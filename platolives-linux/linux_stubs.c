/* Stub per il Linker ELF: solo i simboli OpenGL 1.1 esportati da libGL.so.1 */
#include <stdint.h>
#include <stddef.h>

int  SDL_GetCPUCount(void) { return 4; }
void SDL_Init(void) {}
void SDL_Quit(void) {}
const char* SDL_GetError(void) { return ""; }
const char* SDL_GetKeyName(int key) { (void)key; return ""; }
void* SDL_CreateWindow(void) { return (void*)1; }
void SDL_DestroyWindow(void) {}
void* SDL_GL_CreateContext(void) { return (void*)1; }
void SDL_GL_DeleteContext(void) {}
void SDL_GL_SwapWindow(void) {}
int  SDL_GL_SetAttribute(void) { return 0; }
int  SDL_GL_SetSwapInterval(void) { return 0; }
void SDL_GL_GetDrawableSize(void) {}
void SDL_GetWindowSize(void) {}
int  SDL_PollEvent(void) { return 0; }
uint32_t SDL_GetTicks(void) { return 0; }
void SDL_Delay(void) {}
void SDL_StartTextInput(void) {}
void SDL_StopTextInput(void) {}
int  SDL_SetWindowFullscreen(void) { return 0; }
void SDL_SetWindowTitle(void *window, const char *title) { (void)window; (void)title; }
int  SDL_SetClipboardText(void) { return 0; }
char* SDL_GetClipboardText(void) { return NULL; }
int  SDL_HasClipboardText(void) { return 0; }
void SDL_free(void *ptr) { (void)ptr; }
void* SDL_GL_GetProcAddress(void) { return NULL; }
int  SDL_GetModState(void) { return 0; }
void SDL_RaiseWindow(void) {}
void* SDL_CreateRenderer(void) { return (void*)1; }
void* SDL_CreateTexture(void) { return (void*)1; }
int   SDL_UpdateTexture(void) { return 0; }
int   SDL_SetTextureBlendMode(void) { return 0; }
uint32_t SDL_GetWindowID(void) { return 1; }
void  SDL_DestroyTexture(void) {}
void  SDL_DestroyRenderer(void) {}
int   SDL_SetRenderDrawColor(void) { return 0; }
int   SDL_RenderClear(void) { return 0; }
int   SDL_SetTextureColorMod(void) { return 0; }
int   SDL_RenderCopy(void) { return 0; }
int   SDL_RenderFillRect(void) { return 0; }
int   SDL_RenderDrawRect(void) { return 0; }
void  SDL_RenderPresent(void) {}


void glGenTextures(void) {}
void glBindTexture(void) {}
void glTexParameteri(void) {}
void glTexImage2D(void) {}
void glTexSubImage2D(void) {}
void glDeleteTextures(void) {}
void glViewport(void) {}
void glClearColor(void) {}
void glClear(void) {}
void glEnable(void) {}
void glDisable(void) {}
void glBlendFunc(void) {}
void glDrawArrays(void) {}

/* Funzioni Audio SDL2 per il Linker ELF */
int  SDL_InitSubSystem(uint32_t flags) { (void)flags; return 0; }
void SDL_QuitSubSystem(uint32_t flags) { (void)flags; }
uint32_t SDL_OpenAudioDevice(const char *device, int iscapture, const void *desired, void *obtained, int allowed_changes) { (void)device; (void)iscapture; (void)desired; (void)obtained; (void)allowed_changes; return 1; }
void SDL_CloseAudioDevice(uint32_t dev) { (void)dev; }
void SDL_PauseAudioDevice(uint32_t dev, int pause_on) { (void)dev; (void)pause_on; }
int  SDL_QueueAudio(uint32_t dev, const void *data, uint32_t len) { (void)dev; (void)data; (void)len; return 0; }
void SDL_ClearQueuedAudio(uint32_t dev) { (void)dev; }

void* SDL_memset(void *dst, int c, size_t len) { (void)c; (void)len; return dst; }

/* Funzioni Gestione Icone Finestra SDL2 */
void* SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height, int depth, int pitch, uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask) { (void)pixels; (void)width; (void)height; (void)depth; (void)pitch; (void)Rmask; (void)Gmask; (void)Bmask; (void)Amask; return (void*)1; }
void  SDL_SetWindowIcon(void *window, void *icon) { (void)window; (void)icon; }
void  SDL_FreeSurface(void *surface) { (void)surface; }

void SDL_GL_ResetAttributes(void) {}

int SDL_GL_MakeCurrent(void *window, void *context) { (void)window; (void)context; return 0; }

void SDL_RenderDrawLine(void) {}

int SDL_SetTextureAlphaMod(void *texture, unsigned char alpha) { (void)texture; (void)alpha; return 0; }
