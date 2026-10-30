/*
 * PlatoLives Native Client - DEC PDP-1 Type 30 Precision CRT Display (1964)
 * Module: PDP-1 Character Generator Fonts (1961 Codeword & 1964 Character Display)
 * Target Version: v5.3.0
 */

#ifndef PLATO_PDP1_FONT_H
#define PLATO_PDP1_FONT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t rows[7];       /* 7 righe: bit 4 (Colonna 0) .. bit 0 (Colonna 4) */
    uint8_t descender;     /* 0: Standard Baseline (r0..r6), 1: Descender Drop +2 (r2..r8) */
} pdp1_glyph_t;

/* Simboli estesi logici, insiemistici e di controllo PDP-1 (0x80..0x8F) */
#define PDP1_GLYPH_UP_ARROW     0x80  /* ↑ */
#define PDP1_GLYPH_RIGHT_ARROW  0x81  /* → */
#define PDP1_GLYPH_IMPLIES      0x82  /* ⊃ */
#define PDP1_GLYPH_OR           0x83  /* ∨ (1961) */
#define PDP1_GLYPH_AND          0x84  /* ∧ (1961) */
#define PDP1_GLYPH_UNION        0x85  /* ∪ (1964) */
#define PDP1_GLYPH_INTERSECT    0x86  /* ∩ (1964) */
#define PDP1_GLYPH_POINTER      0x87  /* ⇒ (1964 Screen Editor Pointer / 1961 Tab) */
#define PDP1_GLYPH_SPACE_SYM    0x88  /* ␣ (1961 visible space) */
#define PDP1_GLYPH_LC_SYM       0x89  /* ⇓ (1961 lower case shift) */
#define PDP1_GLYPH_UC_SYM       0x8A  /* ⇑ (1961 upper case shift) */
#define PDP1_GLYPH_BKSPC_SYM    0x8B  /* ⇐ (1961 backspace arrow) */
#define PDP1_GLYPH_CR_SYM       0x8C  /* ↵ (1961 carriage return hook) */

/* Font 1: 1961 Codeword Display (MIT AI Lab / Spacewar!) */
extern const pdp1_glyph_t pdp1_font_1961_ascii[128];
extern const pdp1_glyph_t pdp1_font_1961_extended[14];
const pdp1_glyph_t* pdp1_font_1961_get_glyph(uint8_t c);

/* Font 2: 1964 Character Display (PDP-1X / Type 33 Symbol Generator) */
extern const pdp1_glyph_t pdp1_font_1964_ascii[128];
extern const pdp1_glyph_t pdp1_font_1964_extended[14];
const pdp1_glyph_t* pdp1_font_1964_get_glyph(uint8_t c);

#ifdef __cplusplus
}
#endif

#endif /* PLATO_PDP1_FONT_H */
