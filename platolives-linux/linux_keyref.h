#ifndef LINUX_KEYREF_H
#define LINUX_KEYREF_H

#include <SDL2/SDL.h>
#include <stdbool.h>

void linux_keyref_toggle(void);
void linux_keyref_handle_event(const SDL_Event *ev);
void linux_keyref_render(void);
void linux_keyref_destroy(void);

#endif /* LINUX_KEYREF_H */
