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
    int scale = (size <= 1) ? 1 : size;
    int y_base = y + ((scale > 1) ? (scale - 1) * 5 : 0);

    for (int row = 0; row < PLATO_CHAR_HEIGHT; row++) {
        uint8_t bits = glyph[row];
        int py = y_base + ((PLATO_CHAR_HEIGHT - 1 - row) * scale);

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

void plato_draw_paint(plato_framebuffer_t *fb, int start_x, int start_y, plato_screen_mode_t mode) {
    if (!fb || start_x < 0 || start_x >= PLATO_WIDTH || start_y < 0 || start_y >= PLATO_HEIGHT) return;

    /* PLATO Paint riempie lo spazio vuoto delimitato da vettori tracciati.
       Se il punto di partenza (seme) è già un pixel acceso, non c'è nulla da riempire. */
    if (plato_fb_get_pixel(fb, start_x, start_y)) return;

    typedef struct { int16_t x, y; } point_t;
    point_t *q = malloc(sizeof(point_t) * PLATO_WIDTH * PLATO_HEIGHT);
    if (!q) return;

    /* Matrice di tracciamento pixel visitati (per evitare cicli infiniti), richiede solo 32 KB */
    uint8_t (*visited)[PLATO_WIDTH / 8] = calloc(PLATO_HEIGHT, PLATO_WIDTH / 8);
    if (!visited) { free(q); return; }

    size_t head = 0, tail = 0;

    /* Incolonna il primo seme */
    q[tail++] = (point_t){(int16_t)start_x, (int16_t)start_y};
    visited[start_y][start_x / 8] |= (0x80 >> (start_x % 8));

    while (head < tail) {
        point_t p = q[head++];

        /* Colora il punto usando la modalità corrente (es. colore FG per Asteroids) */
        plato_draw_point(fb, p.x, p.y, mode);

        int16_t nx, ny;

        /* UP */
        nx = p.x; ny = p.y + 1;
        if (ny < PLATO_HEIGHT && !(visited[ny][nx / 8] & (0x80 >> (nx % 8))) && !plato_fb_get_pixel(fb, nx, ny)) {
            visited[ny][nx / 8] |= (0x80 >> (nx % 8));
            q[tail++] = (point_t){nx, ny};
        }
        /* DOWN */
        nx = p.x; ny = p.y - 1;
        if (ny >= 0 && !(visited[ny][nx / 8] & (0x80 >> (nx % 8))) && !plato_fb_get_pixel(fb, nx, ny)) {
            visited[ny][nx / 8] |= (0x80 >> (nx % 8));
            q[tail++] = (point_t){nx, ny};
        }
        /* LEFT */
        nx = p.x - 1; ny = p.y;
        if (nx >= 0 && !(visited[ny][nx / 8] & (0x80 >> (nx % 8))) && !plato_fb_get_pixel(fb, nx, ny)) {
            visited[ny][nx / 8] |= (0x80 >> (nx % 8));
            q[tail++] = (point_t){nx, ny};
        }
        /* RIGHT */
        nx = p.x + 1; ny = p.y;
        if (nx < PLATO_WIDTH && !(visited[ny][nx / 8] & (0x80 >> (nx % 8))) && !plato_fb_get_pixel(fb, nx, ny)) {
            visited[ny][nx / 8] |= (0x80 >> (nx % 8));
            q[tail++] = (point_t){nx, ny};
        }
    }
    
    free(visited);
    free(q);
}
