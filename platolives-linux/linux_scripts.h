#ifndef LINUX_SCRIPTS_H
#define LINUX_SCRIPTS_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include "plato/plato_script.h"

typedef void (*linux_script_run_cb)(const plato_script_t *script, void *user_data);

void linux_scripts_show(plato_script_list_t *list, linux_script_run_cb run_cb, void *user_data);
void linux_scripts_handle_event(const SDL_Event *ev);
void linux_scripts_render(void);
void linux_scripts_destroy(void);
bool linux_scripts_is_open(void);

#endif /* LINUX_SCRIPTS_H */
