#ifndef WIN_KEYMAP_H
#define WIN_KEYMAP_H
#include "plato/plato_keymap.h"
#include "plato/plato_keyboard.h"
#include <windows.h>
#include <stdbool.h>

void win_keymap_show(HWND hParent, plato_keyboard_state_t *kstate, HMENU hMenu);

#endif /* WIN_KEYMAP_H */
