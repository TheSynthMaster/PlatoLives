#ifndef WIN_CONN_H
#define WIN_CONN_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdbool.h>
#include "plato/plato_profile.h"
#include "plato/plato_transport.h"

typedef void (*win_conn_done_cb)(bool success, void *user_data);

void win_conn_show(HWND hParent, const plato_profile_t *p, plato_transport_t *t, win_conn_done_cb cb, void *user_data);
void win_conn_close(void);
bool win_conn_is_open(void);

#endif /* WIN_CONN_H */
