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
    PLATO_VKEY_TAB,
    PLATO_VKEY_NUMPAD0, PLATO_VKEY_NUMPAD1, PLATO_VKEY_NUMPAD2,
    PLATO_VKEY_NUMPAD3, PLATO_VKEY_NUMPAD4, PLATO_VKEY_NUMPAD5,
    PLATO_VKEY_NUMPAD6, PLATO_VKEY_NUMPAD7, PLATO_VKEY_NUMPAD8, PLATO_VKEY_NUMPAD9
} plato_virtual_key_t;

typedef struct plato_key_event {
    plato_virtual_key_t vkey;
    uint32_t codepoint;       /* Unicode o ASCII stampabile (32..126) */
    uint32_t unmod_codepoint; /* Carattere base minuscolo senza modificatori */
    bool shift;
    bool ctrl;
    bool alt;
    bool gui;                 /* Command su macOS, Win su Windows */
} plato_key_event_t;

#include "plato_keymap.h"

typedef void (*plato_feedback_callback_t)(const char *message, double duration_sec, void *user_data);

typedef enum {
    PLATO_ARROW_MODE_STANDARD = 0, /* PLATO Standard: SUPER, SUB, ERASE, TAB */
    PLATO_ARROW_MODE_WASD,         /* WASD Gaming: Up=W, Down=S, Left=A, Right=D */
    PLATO_ARROW_MODE_WASX,         /* WASX Gaming (Avatar/Oubliette): Up=W, Down=X, Left=A, Right=D */
    PLATO_ARROW_MODE_WADX          /* WADX Classic PLATO / TUTOR: Up=W, Down=X, Left=A, Right=D */
} plato_arrow_mode_t;

typedef struct {
    bool double_tap_enabled;
    plato_arrow_mode_t arrow_mode; /* Modalità frecce direzionali (retrocompatibilità) */
    bool arrows_wasd_mode;         /* Retrocompatibilità booleana */
    plato_special_key_t pending_key;
    double pending_start_time;
    double pending_expiry;
    plato_feedback_callback_t feedback_cb;
    void *user_data;
    plato_keymap_manager_t keymap_mgr; /* Gestore profili di tastiera universale */
} plato_keyboard_state_t;

void plato_keyboard_state_init(plato_keyboard_state_t *state);
void plato_keyboard_set_feedback_cb(plato_keyboard_state_t *state, plato_feedback_callback_t cb, void *user_data);
void plato_keyboard_send_ascii(plato_terminal_t *term, uint8_t ch);
void plato_keyboard_set_arrow_mode(plato_keyboard_state_t *state, plato_arrow_mode_t mode);
plato_arrow_mode_t plato_keyboard_get_arrow_mode(const plato_keyboard_state_t *state);
plato_arrow_mode_t plato_keyboard_cycle_arrow_mode(plato_keyboard_state_t *state);
void plato_keyboard_set_wasd_mode(plato_keyboard_state_t *state, bool enabled);
bool plato_keyboard_get_wasd_mode(const plato_keyboard_state_t *state);
bool plato_keyboard_toggle_wasd_mode(plato_keyboard_state_t *state);
void plato_keyboard_poll(plato_keyboard_state_t *state, plato_terminal_t *term, double now_sec);
void plato_keyboard_flush(plato_keyboard_state_t *state, plato_terminal_t *term);

/* Accesso al gestore profili di tastiera */
plato_keymap_manager_t *plato_keyboard_get_keymap_manager(plato_keyboard_state_t *state);
const char *plato_keyboard_get_active_keymap_name(const plato_keyboard_state_t *state);
bool plato_keyboard_set_active_keymap_by_index(plato_keyboard_state_t *state, int index);

bool plato_keyboard_dispatch(plato_keyboard_state_t *state,
                             plato_terminal_t *term,
                             const plato_key_event_t *ev,
                             double now_sec);

uint16_t plato_keyboard_keycode(plato_special_key_t key, bool shift);
size_t plato_keyboard_encode(plato_special_key_t key, bool shift, uint8_t *out_buf, size_t max_len);

#endif /* PLATO_KEYBOARD_H */
