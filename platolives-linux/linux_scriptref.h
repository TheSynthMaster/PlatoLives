#ifndef LINUX_SCRIPTREF_H
#define LINUX_SCRIPTREF_H

#include <SDL2/SDL.h>
#include <stdbool.h>

void linux_scriptref_toggle(void);
void linux_scriptref_handle_event(const SDL_Event *ev);
void linux_scriptref_render(void);
void linux_scriptref_destroy(void);
bool linux_scriptref_is_open(void);

#endif /* LINUX_SCRIPTREF_H */
