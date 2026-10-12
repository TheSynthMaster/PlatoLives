#ifndef LINUX_LIVETEXT_H
#define LINUX_LIVETEXT_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include "plato/plato_terminal.h"

void linux_livetext_show(plato_terminal_t *term);
void linux_livetext_toggle(plato_terminal_t *term);
void linux_livetext_handle_event(const SDL_Event *ev);
void linux_livetext_render(void);
void linux_livetext_destroy(void);
bool linux_livetext_is_open(void);

#endif /* LINUX_LIVETEXT_H */
