#ifndef LINUX_ABOUT_H
#define LINUX_ABOUT_H

#include <SDL2/SDL.h>
#include <stdbool.h>

void linux_about_show(void);
void linux_about_handle_event(const SDL_Event *ev);
void linux_about_render(void);
void linux_about_destroy(void);
bool linux_about_is_open(void);

#endif /* LINUX_ABOUT_H */
