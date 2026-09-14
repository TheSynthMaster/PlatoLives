#include <stdio.h>
#include "plato/plato_graphics.h"
#include <stdlib.h>

void plato_draw_point(plato_framebuffer_t *fb, int x, int y, plato_screen_mode_t mode) {
    if (x < 0 || x >= PLATO_WIDTH || y < 0 || y >= PLATO_HEIGHT) return;
    switch (mode) {
        case PLATO_SCREEN_WRITE:
        case PLATO_SCREEN_REWRITE:
            plato_fb_set_pixel(fb, x, y, true);
            break;
        case PLATO_SCREEN_ERASE:
        case PLATO_SCREEN_INVERSE:
            plato_fb_set_pixel(fb, x, y, false);
            break;
    }
}

void plato_draw_line(plato_framebuffer_t *fb, int x0, int y0, int x1, int y1, plato_screen_mode_t mode) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    int x = x0;
    int y = y0;

    while (true) {
        plato_draw_point(fb, x, y, mode);
        if (x == x1 && y == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

void plato_draw_char(plato_framebuffer_t *fb, const plato_font_t *font,
                     plato_charset_t charset, uint8_t ch,
                     int x, int y, plato_screen_mode_t mode, int size) {
    const uint8_t *glyph = plato_font_get_glyph(font, charset, ch);
    int scale = (size == 2) ? 2 : 1;

    for (int row = 0; row < PLATO_CHAR_HEIGHT; row++) {
        uint8_t bits = glyph[row];
        int py = y + ((PLATO_CHAR_HEIGHT - 1 - row) * scale);

        for (int col = 0; col < PLATO_CHAR_WIDTH; col++) {
            bool bit_on = (bits & (0x80 >> col)) != 0;
            int px = x + (col * scale);

            for (int sy = 0; sy < scale; sy++) {
                for (int sx = 0; sx < scale; sx++) {
                    int target_x = px + sx;
                    int target_y = py + sy;
                    if (target_x >= PLATO_WIDTH || target_y >= PLATO_HEIGHT || target_x < 0 || target_y < 0) {
                        continue;
                    }

                    switch (mode) {
                        case PLATO_SCREEN_WRITE:
                            if (bit_on) plato_fb_set_pixel(fb, target_x, target_y, true);
                            break;
                        case PLATO_SCREEN_ERASE:
                            if (bit_on) plato_fb_set_pixel(fb, target_x, target_y, false);
                            break;
                        case PLATO_SCREEN_REWRITE:
                            plato_fb_set_pixel(fb, target_x, target_y, bit_on);
                            break;
                        case PLATO_SCREEN_INVERSE:
                            plato_fb_set_pixel(fb, target_x, target_y, !bit_on);
                            break;
                    }
                }
            }
        }
    }
}

void plato_draw_block(plato_framebuffer_t *fb, int x0, int y0, int x1, int y1, plato_screen_mode_t mode) {
    int min_x = (x0 < x1) ? x0 : x1;
    int max_x = (x0 > x1) ? x0 : x1;
    int min_y = (y0 < y1) ? y0 : y1;
    int max_y = (y0 > y1) ? y0 : y1;

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            plato_draw_point(fb, x, y, mode);
        }
    }
}
