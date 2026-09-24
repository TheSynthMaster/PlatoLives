#ifndef PLATO_KEYBOARD_H
#define PLATO_KEYBOARD_H

#include "plato_types.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

struct plato_terminal;
typedef struct plato_terminal plato_terminal_t;

typedef enum {
    PLATO_VKEY_NONE = 0,
    PLATO_VKEY_F1, PLATO_VKEY_F2, PLATO_VKEY_F3, PLATO_VKEY_F4,
    PLATO_VKEY_F5, PLATO_VKEY_F6, PLATO_VKEY_F7, PLATO_VKEY_F8,
    PLATO_VKEY_F9, PLATO_VKEY_F10, PLATO_VKEY_F11, PLATO_VKEY_F12,
    PLATO_VKEY_UP, PLATO_VKEY_DOWN, PLATO_VKEY_LEFT, PLATO_VKEY_RIGHT,
    PLATO_VKEY_PAGEUP, PLATO_VKEY_PAGEDOWN,
    PLATO_VKEY_HOME, PLATO_VKEY_END,
    PLATO_VKEY_RETURN,
    PLATO_VKEY_BACKSPACE,
    PLATO_VKEY_ESCAPE,
    PLATO_VKEY_TAB
} plato_virtual_key_t;

typedef struct {
    plato_virtual_key_t vkey;
    uint32_t codepoint;       /* Unicode o ASCII stampabile (32..126) */
    uint32_t unmod_codepoint; /* Carattere base minuscolo senza modificatori */
    bool shift;
    bool ctrl;
    bool alt;
    bool gui;                 /* Command su macOS, Win su Windows */
} plato_key_event_t;

typedef void (*plato_feedback_callback_t)(const char *message, double duration_sec, void *user_data);

typedef struct {
    bool double_tap_enabled;
    plato_special_key_t pending_key;
    double pending_start_time;
    double pending_expiry;
    plato_feedback_callback_t feedback_cb;
    void *user_data;
} plato_keyboard_state_t;

void plato_keyboard_state_init(plato_keyboard_state_t *state);
void plato_keyboard_set_feedback_cb(plato_keyboard_state_t *state, plato_feedback_callback_t cb, void *user_data);
void plato_keyboard_poll(plato_keyboard_state_t *state, plato_terminal_t *term, double now_sec);
void plato_keyboard_flush(plato_keyboard_state_t *state, plato_terminal_t *term);

bool plato_keyboard_dispatch(plato_keyboard_state_t *state,
                             plato_terminal_t *term,
                             const plato_key_event_t *ev,
                             double now_sec);

uint16_t plato_keyboard_keycode(plato_special_key_t key, bool shift);
size_t plato_keyboard_encode(plato_special_key_t key, bool shift, uint8_t *out_buf, size_t max_len);

#endif /* PLATO_KEYBOARD_H */
