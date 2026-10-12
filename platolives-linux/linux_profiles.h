#ifndef LINUX_PROFILES_H
#define LINUX_PROFILES_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include "plato/plato_profile.h"

typedef void (*linux_profile_connect_cb)(const plato_profile_t *profile, void *user_data);

void linux_profiles_show(plato_profile_list_t *list, linux_profile_connect_cb cb, void *user_data);
void linux_profiles_handle_event(const SDL_Event *ev);
void linux_profiles_render(void);
void linux_profiles_destroy(void);
bool linux_profiles_is_open(void);

#endif /* LINUX_PROFILES_H */
