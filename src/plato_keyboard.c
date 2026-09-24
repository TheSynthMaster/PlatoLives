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
}

void plato_keyboard_set_feedback_cb(plato_keyboard_state_t *state, plato_feedback_callback_t cb, void *user_data) {
    if (!state) return;
    state->feedback_cb = cb;
    state->user_data = user_data;
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

static inline void send_raw_byte(plato_terminal_t *term, uint8_t b) {
    if (term && term->transport && term->transport->connected) {
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

    if (ev->codepoint >= 32 && ev->codepoint <= 126 && !ev->ctrl && !ev->alt && !ev->gui) {
        plato_keyboard_flush(state, term);
        send_raw_byte(term, (uint8_t)ev->codepoint);
        return true;
    }

    return false;
}
