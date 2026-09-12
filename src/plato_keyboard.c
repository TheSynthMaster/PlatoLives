#include "plato/plato_keyboard.h"
#include <stdint.h>

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
