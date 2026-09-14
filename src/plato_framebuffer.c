#include <stdio.h>
#include "plato/plato_framebuffer.h"
#include <string.h>

#define PACK_RGBA(r, g, b) ((uint32_t)(b) | ((uint32_t)(g) << 8) | ((uint32_t)(r) << 16) | ((uint32_t)(0xFF) << 24))

void plato_fb_init(plato_framebuffer_t *fb) {
    if (!fb) return;
    fb->fg_color = PACK_RGBA(0xFF, 0x6E, 0x00); /* Default plasma amber */
    fb->bg_color = PACK_RGBA(0x0A, 0x03, 0x00); /* Default dark background */
    fb->color_enabled = false;
    plato_fb_clear(fb);
}

void plato_fb_clear(plato_framebuffer_t *fb) {
    if (!fb) return;
    memset(fb->pixels, 0, sizeof(fb->pixels));
    if (fb->color_enabled) {
        for (int y = 0; y < PLATO_HEIGHT; y++) {
            for (int x = 0; x < PLATO_WIDTH; x++) {
                fb->colors[y][x] = fb->bg_color;
            }
        }
    } else {
        memset(fb->colors, 0, sizeof(fb->colors));
    }
    fb->dirty = true;
}

void plato_fb_set_pixel(plato_framebuffer_t *fb, int x, int y, bool value) {
    if (!fb || x < 0 || x >= PLATO_WIDTH || y < 0 || y >= PLATO_HEIGHT) {
        return;
    }
    int raster_y = (PLATO_HEIGHT - 1) - y;
    int byte_idx = x / 8;
    uint8_t bit_mask = 0x80 >> (x % 8);

    if (value) {
        fb->pixels[raster_y][byte_idx] |= bit_mask;
        if (fb->color_enabled) {
            fb->colors[raster_y][x] = fb->fg_color;
        }
    } else {
        fb->pixels[raster_y][byte_idx] &= ~bit_mask;
        if (fb->color_enabled) {
            fb->colors[raster_y][x] = fb->bg_color;
        }
    }
    fb->dirty = true;
}

bool plato_fb_get_pixel(const plato_framebuffer_t *fb, int x, int y) {
    if (!fb || x < 0 || x >= PLATO_WIDTH || y < 0 || y >= PLATO_HEIGHT) {
        return false;
    }
    int raster_y = (PLATO_HEIGHT - 1) - y;
    int byte_idx = x / 8;
    uint8_t bit_mask = 0x80 >> (x % 8);
    return (fb->pixels[raster_y][byte_idx] & bit_mask) != 0;
}

void plato_fb_scroll_up(plato_framebuffer_t *fb, int lines) {
    if (!fb || lines <= 0) return;
    if (lines >= PLATO_HEIGHT) {
        plato_fb_clear(fb);
        return;
    }
    int bytes_per_row = PLATO_WIDTH / 8;
    int rows_to_move = PLATO_HEIGHT - lines;
    memmove(&fb->pixels[0], &fb->pixels[lines], rows_to_move * bytes_per_row);
    memset(&fb->pixels[rows_to_move], 0, lines * bytes_per_row);
    fb->dirty = true;
}

void plato_fb_to_rgba32(const plato_framebuffer_t *fb, uint32_t *out_rgba, plato_palette_t pal) {
    if (!fb || !out_rgba) return;
    if (fb->color_enabled) {
        memcpy(out_rgba, fb->colors, sizeof(fb->colors));
        return;
    }

    uint32_t bg_color, fg_color;
    switch (pal) {
        case PLATO_PALETTE_GREEN_P31:
            bg_color = PACK_RGBA(0x00, 0x0F, 0x02);
            fg_color = PACK_RGBA(0x33, 0xFF, 0x33);
            break;
        case PLATO_PALETTE_AMBER_P40:
            bg_color = PACK_RGBA(0x0F, 0x06, 0x00);
            fg_color = PACK_RGBA(0xFF, 0xB0, 0x00);
            break;
        case PLATO_PALETTE_WHITE_CRT:
            bg_color = PACK_RGBA(0x0F, 0x0F, 0x0F);
            fg_color = PACK_RGBA(0xF0, 0xF0, 0xF0);
            break;
        case PLATO_PALETTE_ORANGE_PLASMA:
        default:
            bg_color = PACK_RGBA(0x0A, 0x03, 0x00);
            fg_color = PACK_RGBA(0xFF, 0x6E, 0x00);
            break;
    }

    size_t out_idx = 0;
    for (int r = 0; r < PLATO_HEIGHT; r++) {
        for (int byte_i = 0; byte_i < PLATO_WIDTH / 8; byte_i++) {
            uint8_t b = fb->pixels[r][byte_i];
            for (int bit = 7; bit >= 0; bit--) {
                out_rgba[out_idx++] = (b & (1 << bit)) ? fg_color : bg_color;
            }
        }
    }
}
