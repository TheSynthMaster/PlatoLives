#ifndef PLATO_GRAPHICS_H
#define PLATO_GRAPHICS_H

#include "plato_framebuffer.h"
#include "plato_font.h"

void plato_draw_point(plato_framebuffer_t *fb, int x, int y, plato_screen_mode_t mode);
void plato_draw_line(plato_framebuffer_t *fb, int x0, int y0, int x1, int y1, plato_screen_mode_t mode);
void plato_draw_char(plato_framebuffer_t *fb, const plato_font_t *font,
                     plato_charset_t charset, uint8_t ch,
                     int x, int y, plato_screen_mode_t mode, int size);
void plato_draw_block(plato_framebuffer_t *fb, int x0, int y0, int x1, int y1, plato_screen_mode_t mode);
void plato_draw_paint(plato_framebuffer_t *fb, int start_x, int start_y, plato_screen_mode_t mode);

#endif /* PLATO_GRAPHICS_H */
