#include "plato/plato_keyboard.h"
#include "plato/plato_terminal.h"
#include "plato/plato_transport.h"
#include "plato/plato_protocol.h"
#include <stdio.h>
#include <string.h>

uint16_t plato_keyboard_keycode(plato_special_key_t key, bool shift) {
    switch (key) {
        case PLATO_KEY_NEXT:   return shift ? 0x36 : 0x16;
        case PLATO_KEY_BACK:   return shift ? 0x38 : 0x18;
        case PLATO_KEY_HELP:   return shift ? 0x35 : 0x15;
        case PLATO_KEY_LAB:    return shift ? 0x3D : 0x1D;
        case PLATO_KEY_DATA:   return shift ? 0x39 : 0x19;
        case PLATO_KEY_STOP:   return shift ? 0x3A : 0x1A;
        case PLATO_KEY_EDIT:   return shift ? 0x37 : 0x17;
        case PLATO_KEY_COPY:   return shift ? 0x3B : 0x1B;
        case PLATO_KEY_MICRO:  return shift ? 0x34 : 0x14;
        case PLATO_KEY_FONT:   return 0x34;
        case PLATO_KEY_SUPER:  return shift ? 0x30 : 0x10;
        case PLATO_KEY_SUB:    return shift ? 0x31 : 0x11;
        case PLATO_KEY_ACCESS: return shift ? 0x1C : 0x3C;
        case PLATO_KEY_ERASE:  return shift ? 0x33 : 0x13;
        case PLATO_KEY_ANS:    return shift ? 0x32 : 0x12;
        case PLATO_KEY_TERM:   return shift ? 0x12 : 0x32;
        case PLATO_KEY_SQUARE: return shift ? 0x3C : 0x1C;
        case PLATO_KEY_NONE:
        default:               return UINT16_MAX;
    }
}

size_t plato_keyboard_encode(plato_special_key_t key, bool shift, uint8_t *out_buf, size_t max_len) {
    static const uint8_t ptat0[64] = {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x26, 0x60, 0x0A, 0x5E, 0x2B, 0x2D,
        0x13, 0x04, 0x07, 0x08, 0x7B, 0x0B, 0x0D, 0x1A,
        0x02, 0x12, 0x01, 0x03, 0x7D, 0x0C, 0x83, 0x85,
        0x3C, 0x3E, 0x5B, 0x5D, 0x24, 0x25, 0x5F, 0x7C,
        0x2A, 0x28, 0x40, 0x27, 0x1C, 0x5C, 0x23, 0x7E,
        0x17, 0x05, 0x14, 0x19, 0x7F, 0x09, 0x1E, 0x18,
        0x0E, 0x1D, 0x11, 0x16, 0x00, 0x0F, 0x87, 0x88
    };

    if (!out_buf || max_len == 0) return 0;
    uint16_t keycode = plato_keyboard_keycode(key, shift);
    if (keycode == UINT16_MAX || keycode >= 64) return 0;

    uint8_t translated = ptat0[keycode];
    if (translated & 0x80) {
        if (max_len < 2) return 0;
        out_buf[0] = 0x1B;
        out_buf[1] = translated & 0x7F;
        return 2;
    }
    out_buf[0] = translated;
    return 1;
}

void plato_keyboard_state_init(plato_keyboard_state_t *state) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->double_tap_enabled = true;
    state->arrow_mode = PLATO_ARROW_MODE_STANDARD;
    state->arrows_wasd_mode = false;
    plato_keymap_manager_init(&state->keymap_mgr);
}

void plato_keyboard_set_feedback_cb(plato_keyboard_state_t *state, plato_feedback_callback_t cb, void *user_data) {
    if (!state) return;
    state->feedback_cb = cb;
    state->user_data = user_data;
}

void plato_keyboard_set_arrow_mode(plato_keyboard_state_t *state, plato_arrow_mode_t mode) {
    if (!state) return;
    state->arrow_mode = mode;
    state->arrows_wasd_mode = (mode != PLATO_ARROW_MODE_STANDARD);
    if (mode == PLATO_ARROW_MODE_STANDARD) {
        plato_keymap_set_active_by_name(&state->keymap_mgr, "Standard PLATO");
    } else if (mode == PLATO_ARROW_MODE_WASD) {
        plato_keymap_set_active_by_name(&state->keymap_mgr, "WASD Gaming");
    } else if (mode == PLATO_ARROW_MODE_WASX) {
        plato_keymap_set_active_by_name(&state->keymap_mgr, "WASX / Avatar");
    }
}

plato_arrow_mode_t plato_keyboard_get_arrow_mode(const plato_keyboard_state_t *state) {
    if (!state) return PLATO_ARROW_MODE_STANDARD;
    const plato_keymap_profile_t *p = plato_keymap_get_active_profile(&state->keymap_mgr);
    if (p) {
        if (strcmp(p->name, "WASD Gaming") == 0) return PLATO_ARROW_MODE_WASD;
        if (strcmp(p->name, "WASX / Avatar") == 0) return PLATO_ARROW_MODE_WASX;
        if (strcmp(p->name, "Standard PLATO") == 0) return PLATO_ARROW_MODE_STANDARD;
    }
    return state->arrow_mode;
}

plato_arrow_mode_t plato_keyboard_cycle_arrow_mode(plato_keyboard_state_t *state) {
    if (!state) return PLATO_ARROW_MODE_STANDARD;
    int next_idx = plato_keymap_cycle_profile(&state->keymap_mgr);
    const plato_keymap_profile_t *p = plato_keymap_get_profile(&state->keymap_mgr, next_idx);

    plato_arrow_mode_t mode = PLATO_ARROW_MODE_STANDARD;
    if (p) {
        if (strcmp(p->name, "WASD Gaming") == 0) mode = PLATO_ARROW_MODE_WASD;
        else if (strcmp(p->name, "WASX / Avatar") == 0) mode = PLATO_ARROW_MODE_WASX;
        else mode = PLATO_ARROW_MODE_STANDARD;
    }
    state->arrow_mode = mode;
    state->arrows_wasd_mode = (mode != PLATO_ARROW_MODE_STANDARD);

    /* Feedback visivo unificato */
    if (state->feedback_cb && p) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Keyboard Profile: %s", p->name);
        state->feedback_cb(msg, 1.5, state->user_data);
    }
    return mode;
}

plato_keymap_manager_t *plato_keyboard_get_keymap_manager(plato_keyboard_state_t *state) {
    return state ? &state->keymap_mgr : NULL;
}

const char *plato_keyboard_get_active_keymap_name(const plato_keyboard_state_t *state) {
    if (!state) return "Standard PLATO";
    const plato_keymap_profile_t *p = plato_keymap_get_active_profile(&state->keymap_mgr);
    return p ? p->name : "Standard PLATO";
}

bool plato_keyboard_set_active_keymap_by_index(plato_keyboard_state_t *state, int index) {
    if (!state) return false;
    bool ok = plato_keymap_set_active_by_index(&state->keymap_mgr, index);
    if (ok) {
        const plato_keymap_profile_t *p = plato_keymap_get_profile(&state->keymap_mgr, index);
        if (p) {
            if (strcmp(p->name, "WASD Gaming") == 0) state->arrow_mode = PLATO_ARROW_MODE_WASD;
            else if (strcmp(p->name, "WASX / Avatar") == 0) state->arrow_mode = PLATO_ARROW_MODE_WASX;
            else state->arrow_mode = PLATO_ARROW_MODE_STANDARD;
            state->arrows_wasd_mode = (state->arrow_mode != PLATO_ARROW_MODE_STANDARD);

            if (state->feedback_cb) {
                char msg[128];
                snprintf(msg, sizeof(msg), "Keyboard Profile: %s", p->name);
                state->feedback_cb(msg, 1.5, state->user_data);
            }
        }
    }
    return ok;
}

void plato_keyboard_set_wasd_mode(plato_keyboard_state_t *state, bool enabled) {
    plato_keyboard_set_arrow_mode(state, enabled ? PLATO_ARROW_MODE_WASD : PLATO_ARROW_MODE_STANDARD);
}

bool plato_keyboard_get_wasd_mode(const plato_keyboard_state_t *state) {
    return state ? (state->arrow_mode != PLATO_ARROW_MODE_STANDARD) : false;
}

bool plato_keyboard_toggle_wasd_mode(plato_keyboard_state_t *state) {
    plato_arrow_mode_t mode = plato_keyboard_cycle_arrow_mode(state);
    return (mode != PLATO_ARROW_MODE_STANDARD);
}

void plato_keyboard_flush(plato_keyboard_state_t *state, plato_terminal_t *term) {
    if (!state || state->pending_key == PLATO_KEY_NONE) return;
    if (term) {
        uint16_t code = plato_keyboard_keycode(state->pending_key, false);
        if (code != UINT16_MAX) {
            plato_protocol_send_key(term, code);
        }
    }
    state->pending_key = PLATO_KEY_NONE;
}

void plato_keyboard_poll(plato_keyboard_state_t *state, plato_terminal_t *term, double now_sec) {
    if (!state || state->pending_key == PLATO_KEY_NONE) return;
    if (now_sec >= state->pending_expiry) {
        plato_keyboard_flush(state, term);
    }
}

static void trigger_key_double_tap(plato_keyboard_state_t *state,
                                   plato_terminal_t *term,
                                   plato_special_key_t key,
                                   bool explicit_shift,
                                   double now_sec) {
    if (explicit_shift || !state || !state->double_tap_enabled) {
        plato_keyboard_flush(state, term);
        if (term) {
            plato_protocol_send_key(term, plato_keyboard_keycode(key, explicit_shift));
        }
        return;
    }

    if (state->pending_key == key && (now_sec - state->pending_start_time) < 0.06) {
        return;
    }

    if (state->pending_key == key && now_sec < state->pending_expiry) {
        state->pending_key = PLATO_KEY_NONE;
        if (term) {
            plato_protocol_send_key(term, plato_keyboard_keycode(key, true));
        }
        if (state->feedback_cb) {
            const char *name = "COMANDO";
            switch (key) {
                case PLATO_KEY_NEXT: name = "SHIFT-NEXT"; break;
                case PLATO_KEY_STOP: name = "SHIFT-STOP"; break;
                case PLATO_KEY_BACK: name = "SHIFT-BACK"; break;
                case PLATO_KEY_HELP: name = "SHIFT-HELP"; break;
                case PLATO_KEY_LAB:  name = "SHIFT-LAB"; break;
                case PLATO_KEY_DATA: name = "SHIFT-DATA"; break;
                case PLATO_KEY_EDIT: name = "SHIFT-EDIT"; break;
                default: break;
            }
            char fb[64];
            snprintf(fb, sizeof(fb), "*** [ %s INVIATO ] ***", name);
            state->feedback_cb(fb, 1.2, state->user_data);
        }
        return;
    }

    plato_keyboard_flush(state, term);
    state->pending_key = key;
    state->pending_start_time = now_sec;
    state->pending_expiry = now_sec + 0.50;
}

void plato_keyboard_send_ascii(plato_terminal_t *term, uint8_t ch) {
    if (!term) return;
    if (term->mt.local_boot || ((term->ram[0x22FA] & 1) != 0)) {
        plato_microtutor_send_ascii_key(&term->mt, ch);
        return;
    }
    if (term->transport && term->transport->connected) {
        plato_transport_send(term->transport, &ch, 1);
    }
}

static inline void send_raw_byte(plato_terminal_t *term, uint8_t b) {
    if (!term) return;
    if (term->mt.local_boot || ((term->ram[0x22FA] & 1) != 0)) {
        plato_microtutor_send_ascii_key(&term->mt, b);
        return;
    }
    if (term->transport && term->transport->connected) {
        plato_transport_send(term->transport, &b, 1);
    }
}

bool plato_keyboard_dispatch(plato_keyboard_state_t *state,
                             plato_terminal_t *term,
                             const plato_key_event_t *ev,
                             double now_sec) {
    if (!ev) return false;



    plato_keyboard_poll(state, term, now_sec);

    if (ev->vkey >= PLATO_VKEY_F1 && ev->vkey <= PLATO_VKEY_F12) {
        switch (ev->vkey) {
            case PLATO_VKEY_F1:
            case PLATO_VKEY_F6:
                trigger_key_double_tap(state, term, PLATO_KEY_HELP, ev->shift, now_sec);
                return true;
            case PLATO_VKEY_F2:
            case PLATO_VKEY_F7:
                trigger_key_double_tap(state, term, PLATO_KEY_LAB, ev->shift, now_sec);
                return true;
            case PLATO_VKEY_F3:
            case PLATO_VKEY_F9:
                trigger_key_double_tap(state, term, PLATO_KEY_DATA, ev->shift, now_sec);
                return true;
            case PLATO_VKEY_F4:
            case PLATO_VKEY_F10:
                trigger_key_double_tap(state, term, PLATO_KEY_STOP, ev->shift, now_sec);
                return true;
            case PLATO_VKEY_F5:
                trigger_key_double_tap(state, term, PLATO_KEY_EDIT, ev->shift, now_sec);
                return true;
            case PLATO_VKEY_F8:
                trigger_key_double_tap(state, term, PLATO_KEY_BACK, ev->shift, now_sec);
                return true;
            default:
                return false;
        }
    }

    if ((ev->ctrl || ev->alt) && !ev->gui) {
        if (ev->alt && !ev->ctrl && ev->vkey == PLATO_VKEY_LEFT) {
            plato_keyboard_flush(state, term);
            if (term) plato_protocol_send_key(term, ev->shift ? 0x2D : 0x0D);
            return true;
        }

        uint32_t uc = ev->unmod_codepoint;
        if (ev->ctrl && (uc >= '0' && uc <= '9')) {
            plato_keyboard_flush(state, term);
            int digit = (int)(uc - '0');
            if (term) plato_protocol_send_key(term, (uint16_t)(digit | 0x20));
            return true;
        }

        if (ev->ctrl && ev->shift && strchr("!@#$%^&*()", (char)uc)) {
            plato_keyboard_flush(state, term);
            int digit = 0;
            switch (uc) {
                case '!': digit = 1; break;
                case '@': digit = 2; break;
                case '#': digit = 3; break;
                case '$': digit = 4; break;
                case '%': digit = 5; break;
                case '^': digit = 6; break;
                case '&': digit = 7; break;
                case '*': digit = 8; break;
                case '(': digit = 9; break;
                case ')': digit = 0; break;
            }
            if (term) plato_protocol_send_key(term, (uint16_t)(digit | 0x20));
            return true;
        }

        switch (uc) {
            case 'a': case '/':
                plato_keyboard_flush(state, term);
                if (term) plato_protocol_send_key(term, plato_keyboard_keycode(PLATO_KEY_ANS, ev->shift));
                return true;
            case 'b': trigger_key_double_tap(state, term, PLATO_KEY_BACK, ev->shift, now_sec); return true;
            case 'c':
                plato_keyboard_flush(state, term);
                if (term) plato_protocol_send_key(term, plato_keyboard_keycode(PLATO_KEY_COPY, ev->shift));
                return true;
            case 'd': trigger_key_double_tap(state, term, PLATO_KEY_DATA, ev->shift, now_sec); return true;
            case 'e': trigger_key_double_tap(state, term, PLATO_KEY_EDIT, ev->shift, now_sec); return true;
            case 'f':
                plato_keyboard_flush(state, term);
                if (term) plato_protocol_send_key(term, plato_keyboard_keycode(PLATO_KEY_FONT, ev->shift));
                return true;
            case 'g':
                plato_keyboard_flush(state, term);
                if (term) plato_protocol_send_key(term, 0x0B);
                return true;
            case 'h': trigger_key_double_tap(state, term, PLATO_KEY_HELP, ev->shift, now_sec); return true;
            case 'l': trigger_key_double_tap(state, term, PLATO_KEY_LAB, ev->shift, now_sec); return true;
            case 'm':
                plato_keyboard_flush(state, term);
                if (term) plato_protocol_send_key(term, plato_keyboard_keycode(PLATO_KEY_MICRO, ev->shift));
                return true;
            case 'n': trigger_key_double_tap(state, term, PLATO_KEY_NEXT, ev->shift, now_sec); return true;
            case 'p':
                plato_keyboard_flush(state, term);
                if (term) plato_protocol_send_key(term, plato_keyboard_keycode(PLATO_KEY_SUPER, ev->shift));
                return true;
            case 'q':
                plato_keyboard_flush(state, term);
                if (term) plato_protocol_send_key(term, plato_keyboard_keycode(PLATO_KEY_SQUARE, ev->shift));
                return true;
            case 'r':
                plato_keyboard_flush(state, term);
                if (term) plato_protocol_send_key(term, plato_keyboard_keycode(PLATO_KEY_ERASE, ev->shift));
                return true;
            case 's': trigger_key_double_tap(state, term, PLATO_KEY_STOP, ev->shift, now_sec); return true;
            case 't':
                plato_keyboard_flush(state, term);
                if (term) plato_protocol_send_key(term, plato_keyboard_keycode(PLATO_KEY_TERM, ev->shift));
                return true;
            case 'x':
                plato_keyboard_flush(state, term);
                if (term) plato_protocol_send_key(term, 0x0A);
                return true;
            case 'y':
                plato_keyboard_flush(state, term);
                if (term) plato_protocol_send_key(term, plato_keyboard_keycode(PLATO_KEY_SUB, ev->shift));
                return true;
            default: break;
        }
    }


    /* 1. Lookup nel Profilo Tastiera Attivo (Keymap Manager) - Scansione O(1) */
    if (state) {
        const plato_keymap_profile_t *active_profile = plato_keymap_get_active_profile(&state->keymap_mgr);
        if (active_profile && active_profile->binding_count > 0) {
            const plato_key_binding_t *binding = plato_keymap_lookup(active_profile, ev);
            if (binding) {
                plato_keyboard_flush(state, term);
                if (binding->type == PLATO_BIND_ACTION_RAW_BYTE) {
                    uint8_t b = binding->target.raw_byte;
                    /* Se il tasto premuto include shift e il target e' alfabetico, converti in maiuscolo */
                    if (ev->shift && !ev->ctrl && !ev->alt && (b >= 'a' && b <= 'z')) {
                        b = (uint8_t)(b - ('a' - 'A'));
                    }
                    send_raw_byte(term, b);
                    return true;
                } else if (binding->type == PLATO_BIND_ACTION_PLATO_KEY) {
                    if (term) {
                        plato_protocol_send_key(term, plato_keyboard_keycode((plato_special_key_t)binding->target.plato_key, ev->shift));
                    }
                    return true;
                } else if (binding->type == PLATO_BIND_ACTION_PLATO_SEQ) {
                    uint32_t seq = binding->target.sequence;
                    for (int k = 0; k < 4; k++) {
                        uint8_t oct = (uint8_t)((seq >> (k * 8)) & 0xFF);
                        if (oct == 0xFF) break;
                        if (term) plato_protocol_send_key(term, oct);
                    }
                    return true;
                }
            }
        }
    }

    /* 2. Modalità Standard PLATO (videoscrittura e navigazione TUTOR canonica) */
    if (ev->vkey == PLATO_VKEY_LEFT) {
        plato_keyboard_flush(state, term);
        if (term) plato_protocol_send_key(term, plato_keyboard_keycode(PLATO_KEY_ERASE, ev->shift));
        return true;
    }
    if (ev->vkey == PLATO_VKEY_RIGHT || ev->vkey == PLATO_VKEY_TAB || ev->codepoint == 9) {
        plato_keyboard_flush(state, term);
        send_raw_byte(term, 0x09);
        return true;
    }
    if (ev->vkey == PLATO_VKEY_UP || ev->vkey == PLATO_VKEY_PAGEUP) {
        plato_keyboard_flush(state, term);
        if (term) plato_protocol_send_key(term, plato_keyboard_keycode(PLATO_KEY_SUPER, ev->shift));
        return true;
    }
    if (ev->vkey == PLATO_VKEY_DOWN || ev->vkey == PLATO_VKEY_PAGEDOWN) {
        plato_keyboard_flush(state, term);
        if (term) plato_protocol_send_key(term, plato_keyboard_keycode(PLATO_KEY_SUB, ev->shift));
        return true;
    }

    if (ev->vkey == PLATO_VKEY_RETURN || ev->codepoint == 13 || ev->codepoint == 10) {
        trigger_key_double_tap(state, term, PLATO_KEY_NEXT, ev->shift, now_sec);
        return true;
    }

    if (ev->vkey == PLATO_VKEY_BACKSPACE || ev->codepoint == 127 || ev->codepoint == 8) {
        plato_keyboard_flush(state, term);
        if (term) plato_protocol_send_key(term, plato_keyboard_keycode(PLATO_KEY_ERASE, ev->shift));
        return true;
    }

    if (ev->vkey == PLATO_VKEY_ESCAPE || ev->codepoint == 27) {
        trigger_key_double_tap(state, term, PLATO_KEY_BACK, ev->shift, now_sec);
        return true;
    }

    /* La Vera Tavola di Rosetta PLATO (da ptermkeytabs.h di Paul Koning).
       Traduce ogni ASCII (incluso '@', '[', ']' e ' ') nell'esatta sequenza di tasti PLATO. */
    static const uint32_t pterm_ascii_to_plato[128] = {
    #define K(a,b,c,d) (((uint32_t)(a)&0xFF) | (((uint32_t)(b)&0xFF)<<8) | (((uint32_t)(c)&0xFF)<<16) | (((uint32_t)(d)&0xFF)<<24))
    #define N 0xFF
    #define NONE 0xFFFFFFFF
       NONE, K(0022, N, N, N), K(0030, N, N, N), K(0033, N, N, N), K(0031, N, N, N), K(0027, N, N, N), K(0064, N, N, N), K(0013, N, N, N),
       K(0025, N, N, N), NONE, NONE, NONE, K(0035, N, N, N), K(0024, N, N, N), K(0026, N, N, N), NONE,
       K(0020, N, N, N), K(0034, N, N, N), K(0023, N, N, N), K(0032, N, N, N), K(0062, N, N, N), NONE, NONE, NONE,
       K(0012, N, N, N), K(0021, N, N, N), NONE, NONE, NONE, NONE, NONE, NONE,
       K(0100, N, N, N), K(0176, N, N, N), K(0177, N, N, N), K(0074, 0044, N, N), K(0044, N, N, N), K(0045, N, N, N), K(0074, 0016, N, N), K(0047, N, N, N),
       K(0051, N, N, N), K(0173, N, N, N), K(0050, N, N, N), K(0016, N, N, N), K(0137, N, N, N), K(0017, N, N, N), K(0136, N, N, N), K(0135, N, N, N),
       K(0000, N, N, N), K(0001, N, N, N), K(0002, N, N, N), K(0003, N, N, N), K(0004, N, N, N), K(0005, N, N, N), K(0006, N, N, N), K(0007, N, N, N),
       K(0010, N, N, N), K(0011, N, N, N), K(0174, N, N, N), K(0134, N, N, N), K(0040, N, N, N), K(0133, N, N, N), K(0041, N, N, N), K(0175, N, N, N),
       K(0074, 0005, N, N), K(0141, N, N, N), K(0142, N, N, N), K(0143, N, N, N), K(0144, N, N, N), K(0145, N, N, N), K(0146, N, N, N), K(0147, N, N, N),
       K(0150, N, N, N), K(0151, N, N, N), K(0152, N, N, N), K(0153, N, N, N), K(0154, N, N, N), K(0155, N, N, N), K(0156, N, N, N), K(0157, N, N, N),
       K(0160, N, N, N), K(0161, N, N, N), K(0162, N, N, N), K(0163, N, N, N), K(0164, N, N, N), K(0165, N, N, N), K(0166, N, N, N), K(0167, N, N, N),
       K(0170, N, N, N), K(0171, N, N, N), K(0172, N, N, N), K(0042, N, N, N), K(0074, 0135, N, N), K(0043, N, N, N), K(0100, 0074, 0130, N), K(0046, N, N, N),
       K(0100, 0074, 0121, N), K(0101, N, N, N), K(0102, N, N, N), K(0103, N, N, N), K(0104, N, N, N), K(0105, N, N, N), K(0106, N, N, N), K(0107, N, N, N),
       K(0110, N, N, N), K(0111, N, N, N), K(0112, N, N, N), K(0113, N, N, N), K(0114, N, N, N), K(0115, N, N, N), K(0116, N, N, N), K(0117, N, N, N),
       K(0120, N, N, N), K(0121, N, N, N), K(0122, N, N, N), K(0123, N, N, N), K(0124, N, N, N), K(0125, N, N, N), K(0126, N, N, N), K(0127, N, N, N),
       K(0130, N, N, N), K(0131, N, N, N), K(0132, N, N, N), K(0074, 0042, N, N), K(0074, 0151, N, N), K(0074, 0043, N, N), K(0100, 0074, 0116, N), NONE
    #undef K
    #undef N
    #undef NONE
    };

    if (ev->codepoint >= 32 && ev->codepoint <= 126 && !ev->ctrl && !ev->gui) {
        uint32_t seq = pterm_ascii_to_plato[ev->codepoint];
        if (seq != 0xFFFFFFFF) {
            plato_keyboard_flush(state, term);
            if (term) {
                for (int i = 0; i < 4; i++) {
                    uint8_t k = (seq >> (i * 8)) & 0xFF;
                    if (k != 0xFF) {
                        plato_protocol_send_key(term, k);
                    }
                }
            }
            return true;
        }
    }

    return false;
}
