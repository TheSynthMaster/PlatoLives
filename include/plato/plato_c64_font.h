/*
 * PlatoLives Native Client - Commodore 64 VIC-II Display Engine
 * Module: C64 Character Generator ROM (MOS Part No. 901225-01)
 */

#ifndef PLATO_C64_FONT_H
#define PLATO_C64_FONT_H

#include <stdint.h>
#include <stdbool.h>
#include "plato_font.h"

#ifdef __cplusplus
extern "C" {
#endif

#define C64_FONT_BANK_UPPER_GFX   0
#define C64_FONT_BANK_LOWER_UPPER 1

const uint8_t *plato_c64_get_screen_glyph_8x8(uint8_t screen_code, int bank);
uint8_t plato_c64_ascii_to_screen_code(uint8_t ascii_ch);
const uint8_t *plato_c64_get_ascii_glyph_8x16(uint8_t ascii_ch);
void plato_c64_load_font_m0(plato_font_t *font);

#ifdef __cplusplus
}
#endif

#endif /* PLATO_C64_FONT_H */
