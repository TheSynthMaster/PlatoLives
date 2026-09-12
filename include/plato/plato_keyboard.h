#ifndef PLATO_KEYBOARD_H
#define PLATO_KEYBOARD_H

#include "plato_types.h"

/* Restituisce il codice interno PLATO/PTAT del tasto, oppure UINT16_MAX. */
uint16_t plato_keyboard_keycode(plato_special_key_t key, bool shift);

/* API legacy mantenuta per compatibilita con eventuali chiamanti esterni. */
size_t plato_keyboard_encode(plato_special_key_t key, bool shift, uint8_t *out_buf, size_t max_len);

#endif /* PLATO_KEYBOARD_H */
