#ifndef WIN_SCRIPTS_H
#define WIN_SCRIPTS_H

#include <windows.h>
#include "plato/plato_script.h"

void win_scripts_dialog_show(HWND hParent, plato_script_list_t *list, plato_terminal_t *terminal);

#endif /* WIN_SCRIPTS_H */
