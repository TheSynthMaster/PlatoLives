#ifndef LINUX_CONN_H
#define LINUX_CONN_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include "plato/plato_profile.h"
#include "plato/plato_transport.h"

typedef void (*linux_conn_done_cb)(bool success, void *user_data);

void linux_conn_show(const plato_profile_t *p, plato_transport_t *t, linux_conn_done_cb cb, void *user_data);
void linux_conn_handle_event(const SDL_Event *ev);
void linux_conn_render(void);
void linux_conn_close(void);
bool linux_conn_is_open(void);

#endif /* LINUX_CONN_H */
