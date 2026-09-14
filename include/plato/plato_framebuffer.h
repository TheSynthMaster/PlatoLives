#ifndef PLATO_FRAMEBUFFER_H
#define PLATO_FRAMEBUFFER_H

#include "plato_types.h"

typedef struct {
    uint8_t pixels[PLATO_HEIGHT][PLATO_WIDTH / 8];
    uint32_t colors[PLATO_HEIGHT][PLATO_WIDTH];
    uint32_t fg_color;
    uint32_t bg_color;
    bool color_enabled;
    bool dirty;
} plato_framebuffer_t;

void plato_fb_init(plato_framebuffer_t *fb);
void plato_fb_clear(plato_framebuffer_t *fb);
void plato_fb_set_pixel(plato_framebuffer_t *fb, int x, int y, bool value);
bool plato_fb_get_pixel(const plato_framebuffer_t *fb, int x, int y);
void plato_fb_scroll_up(plato_framebuffer_t *fb, int lines);
void plato_fb_to_rgba32(const plato_framebuffer_t *fb, uint32_t *out_rgba, plato_palette_t pal);

#endif /* PLATO_FRAMEBUFFER_H */
