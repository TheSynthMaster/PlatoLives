#ifndef WIN_TEXT_BUFFER_H
#define WIN_TEXT_BUFFER_H

#include <windows.h>
#include "plato/plato_terminal.h"

void win_text_buffer_show(HWND hParent, plato_terminal_t *terminal);

#endif /* WIN_TEXT_BUFFER_H */
