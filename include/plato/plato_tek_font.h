#ifndef PLATO_TEK_FONT_H
#define PLATO_TEK_FONT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int8_t x;
    int8_t y;
    bool pen_down;
} plato_tek_stroke_point_t;

typedef struct {
    const plato_tek_stroke_point_t *points;
    uint8_t count;
} plato_tek_glyph_t;

/* Restituisce la definizione bitmap 5x9 (10 byte, righe 0..9, bit 5..1) per il codice ASCII fornito (32..126) */
const uint8_t *plato_tek_font_get_bitmap(uint8_t ascii_char);

/* API stroke preservata per compatibilità */
const plato_tek_glyph_t* plato_tek_font_get_glyph(uint8_t ascii_char);

#ifdef __cplusplus
}
#endif

#endif /* PLATO_TEK_FONT_H */
