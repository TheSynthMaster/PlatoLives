/*
 * PlatoLives Native Client - Commodore 64 VIC-II Display Simulation Engine
 * Module: Core VIC-II Optical Engine & Hires Architecture
 */

#ifndef PLATO_C64_H
#define PLATO_C64_H

#include <stdint.h>
#include <stdbool.h>
#include "plato_terminal.h"
#include "plato_profile.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Macro little-endian BGRA esplicita per evitare qualsiasi inversione di canale */
#define C64_BGRA(r, g, b) (0xFF000000u | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | ((uint32_t)(b)))

/* MOS Technology 6567/6569 VIC-II Canonical 16-Color Palette (VICE official vice.vpl) */
#define C64_COL_BLACK       C64_BGRA(0,   0,   0)     /* #000000 */
#define C64_COL_WHITE       C64_BGRA(255, 255, 255)   /* #FFFFFF */
#define C64_COL_RED         C64_BGRA(136, 57,  50)    /* #883932 */
#define C64_COL_CYAN        C64_BGRA(103, 182, 189)   /* #67B6BD */
#define C64_COL_PURPLE      C64_BGRA(139, 63,  150)   /* #8B3F96 */
#define C64_COL_GREEN       C64_BGRA(85,  160, 73)    /* #55A049 */
#define C64_COL_DARK_BLUE   C64_BGRA(66,  52,  144)   /* #423490 (Authentic C64 Blue $D021) */
#define C64_COL_YELLOW      C64_BGRA(191, 206, 114)   /* #BFCE72 */
#define C64_COL_ORANGE      C64_BGRA(139, 84,  41)    /* #8E5029 */
#define C64_COL_BROWN       C64_BGRA(87,  66,  0)     /* #574200 */
#define C64_COL_LIGHT_RED   C64_BGRA(184, 105, 98)    /* #B86962 */
#define C64_COL_DARK_GREY   C64_BGRA(80,  80,  80)    /* #505050 */
#define C64_COL_GREY        C64_BGRA(112, 112, 112)   /* #707070 */
#define C64_COL_LIGHT_GREEN C64_BGRA(148, 224, 137)   /* #94E089 */
#define C64_COL_LIGHT_BLUE  C64_BGRA(124, 112, 218)   /* #7C70DA (Authentic C64 Light Blue $D020) */
#define C64_COL_LIGHT_GREY  C64_BGRA(154, 154, 154)   /* #9A9A9A */

typedef struct c64_context c64_context_t;

c64_context_t *c64_create(void);
void c64_destroy(c64_context_t *ctx);
void c64_clear(c64_context_t *ctx);
void c64_notify_rx(c64_context_t *ctx, size_t bytes);

void c64_render(c64_context_t *ctx,
                const plato_terminal_t *term,
                const plato_profile_t *prof,
                uint32_t *out_bgra,
                double now_sec);

void c64_unwarp_touch(int *x, int *y, bool border_enabled);

void c64_fb_clear(plato_terminal_t *term);
void c64_fb_draw_point(plato_terminal_t *term, int x, int y, plato_screen_mode_t mode);
void c64_fb_draw_line(plato_terminal_t *term, int x0, int y0, int x1, int y1, plato_screen_mode_t mode);
void c64_fb_draw_block(plato_terminal_t *term, int x0, int y0, int x1, int y1, plato_screen_mode_t mode);
void c64_fb_draw_paint(plato_terminal_t *term, int start_x, int start_y, plato_screen_mode_t mode);
void c64_fb_draw_char(plato_terminal_t *term, const plato_font_t *font, plato_charset_t charset, uint8_t ch, int x, int y, plato_screen_mode_t mode, int size);

#ifdef __cplusplus
}
#endif

#endif /* PLATO_C64_H */
