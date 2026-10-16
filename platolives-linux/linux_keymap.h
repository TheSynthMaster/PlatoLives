#ifndef LINUX_KEYMAP_H
#define LINUX_KEYMAP_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include "plato/plato_keymap.h"

void linux_keymap_show(plato_keymap_manager_t *mgr);
void linux_keymap_handle_event(const SDL_Event *ev);
void linux_keymap_render(void);
void linux_keymap_destroy(void);
bool linux_keymap_is_open(void);

#endif /* LINUX_KEYMAP_H */
