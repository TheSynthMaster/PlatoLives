#ifndef WIN_PROFILES_H
#define WIN_PROFILES_H

#include <windows.h>
#include "plato/plato_profile.h"

typedef void (*win_profile_connect_cb)(const plato_profile_t *profile, void *user_data);

void win_profiles_dialog_show(HWND hParent, plato_profile_list_t *list, win_profile_connect_cb cb, void *user_data);

#endif /* WIN_PROFILES_H */
