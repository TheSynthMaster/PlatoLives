#ifndef PLATO_FONT_H
#define PLATO_FONT_H

#include "plato_types.h"

#define PLATO_FONT_GLYPHS 128

typedef struct {
    uint8_t m0[PLATO_FONT_GLYPHS][PLATO_CHAR_HEIGHT];
    uint8_t m1[PLATO_FONT_GLYPHS][PLATO_CHAR_HEIGHT];
    uint8_t m2[PLATO_FONT_GLYPHS][PLATO_CHAR_HEIGHT];
    uint8_t m3[PLATO_FONT_GLYPHS][PLATO_CHAR_HEIGHT];
} plato_font_t;

void plato_font_init(plato_font_t *font);
const uint8_t* plato_font_get_glyph(const plato_font_t *font, plato_charset_t set, uint8_t ch);
void plato_font_load_glyph(plato_font_t *font, plato_charset_t set, uint8_t ch, const uint8_t bitmap[16]);

#endif /* PLATO_FONT_H */
