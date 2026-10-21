#ifndef LINUX_THEME_H
#define LINUX_THEME_H

#include <stdint.h>
#include <stdbool.h>

#define COLOR_RGBA(r, g, b, a) ((uint32_t)(r) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16) | ((uint32_t)(a) << 24))

typedef struct {
    uint8_t r, g, b, a;
} linux_color_t;

typedef struct {
    linux_color_t normal;
    linux_color_t bright;
    linux_color_t disabled;
    linux_color_t muted;
    linux_color_t dim;
    linux_color_t bg_dark;
    linux_color_t bg_box;
    linux_color_t hover_bg;
    linux_color_t border;

    uint32_t rgba_normal;
    uint32_t rgba_bright;
    uint32_t rgba_disabled;
    uint32_t rgba_muted;
    uint32_t rgba_dim;
    uint32_t rgba_bg_dark;
    uint32_t rgba_bg_box;
    uint32_t rgba_hover_bg;
    uint32_t rgba_border;
} linux_theme_colors_t;

extern int g_linux_theme_mode;
extern int g_linux_theme_phosphor;

static inline linux_theme_colors_t linux_theme_get(int mode, int phosphor) {
    linux_theme_colors_t c;
    if (mode == 5 || phosphor == 1) {
        /* Mode 5 (Tektronix 4014) o Phosphor 1 (P31 Green): Verde Smeraldo Tektronix */
        c.normal    = (linux_color_t){ 51, 255,  51, 255 }; /* #33FF33 */
        c.bright    = (linux_color_t){120, 255, 120, 255 };
        c.disabled  = (linux_color_t){ 25, 110,  25, 255 };
        c.muted     = (linux_color_t){ 30, 150,  30, 255 };
        c.dim       = (linux_color_t){ 20,  90,  20, 255 };
        c.bg_dark   = (linux_color_t){ 14,  10,   6, 255 };
        c.bg_box    = (linux_color_t){ 20,  15,  10, 255 };
        c.hover_bg  = (linux_color_t){ 10,  45,  10, 255 };
        c.border    = (linux_color_t){ 15,  60,  15, 255 }; /* #0A400A / emerald border */
    } else if (phosphor == 2) {
        /* Phosphor 2 (P40 Amber) */
        c.normal    = (linux_color_t){255, 176,   0, 255 }; /* #FFB000 */
        c.bright    = (linux_color_t){255, 210,  80, 255 };
        c.disabled  = (linux_color_t){120,  80,   0, 255 };
        c.muted     = (linux_color_t){190, 130,   0, 255 };
        c.dim       = (linux_color_t){140,  90,   0, 255 };
        c.bg_dark   = (linux_color_t){ 16,  10,   6, 255 };
        c.bg_box    = (linux_color_t){ 24,  14,   8, 255 };
        c.hover_bg  = (linux_color_t){ 45,  30,   0, 255 };
        c.border    = (linux_color_t){ 60,  40,  10, 255 };
    } else if (phosphor == 3 || mode == 3 || mode == 4) {
        /* Crisp Color (3) o Real Color CRT (4) o White P4 (3): Bianco Freddo */
        c.normal    = (linux_color_t){235, 235, 235, 255 }; /* #EBEBEB */
        c.bright    = (linux_color_t){255, 255, 255, 255 };
        c.disabled  = (linux_color_t){110, 110, 110, 255 };
        c.muted     = (linux_color_t){180, 180, 180, 255 };
        c.dim       = (linux_color_t){130, 130, 130, 255 };
        c.bg_dark   = (linux_color_t){ 16,  12,  10, 255 };
        c.bg_box    = (linux_color_t){ 24,  18,  14, 255 };
        c.hover_bg  = (linux_color_t){ 40,  40,  40, 255 };
        c.border    = (linux_color_t){ 60,  60,  60, 255 }; /* #3C3C3C */
    } else {
        /* Authentic Plasma: Mode 0 (Real Plasma), Mode 1 (Crisp Mono), Mode 2 (Split Mono) */
        c.normal    = (linux_color_t){255, 110,   0, 255 }; /* #FF6E00 / Neon Plasma */
        c.bright    = (linux_color_t){255, 160,  40, 255 };
        c.disabled  = (linux_color_t){120,  55,   0, 255 };
        c.muted     = (linux_color_t){190,  85,  10, 255 };
        c.dim       = (linux_color_t){140,  70,  20, 255 };
        c.bg_dark   = (linux_color_t){ 16,  10,   6, 255 };
        c.bg_box    = (linux_color_t){ 24,  14,   8, 255 };
        c.hover_bg  = (linux_color_t){ 45,  20,   0, 255 };
        c.border    = (linux_color_t){ 60,  40,  10, 255 };
    }

    c.rgba_normal   = COLOR_RGBA(c.normal.r,   c.normal.g,   c.normal.b,   c.normal.a);
    c.rgba_bright   = COLOR_RGBA(c.bright.r,   c.bright.g,   c.bright.b,   c.bright.a);
    c.rgba_disabled = COLOR_RGBA(c.disabled.r, c.disabled.g, c.disabled.b, c.disabled.a);
    c.rgba_muted    = COLOR_RGBA(c.muted.r,    c.muted.g,    c.muted.b,    c.muted.a);
    c.rgba_dim      = COLOR_RGBA(c.dim.r,      c.dim.g,      c.dim.b,      c.dim.a);
    c.rgba_bg_dark  = COLOR_RGBA(c.bg_dark.r,  c.bg_dark.g,  c.bg_dark.b,  c.bg_dark.a);
    c.rgba_bg_box   = COLOR_RGBA(c.bg_box.r,   c.bg_box.g,   c.bg_box.b,   c.bg_box.a);
    c.rgba_hover_bg = COLOR_RGBA(c.hover_bg.r, c.hover_bg.g, c.hover_bg.b, c.hover_bg.a);
    c.rgba_border   = COLOR_RGBA(c.border.r,   c.border.g,   c.border.b,   c.border.a);

    return c;
}

static inline void linux_theme_set_current(int mode, int phosphor) {
    g_linux_theme_mode = mode;
    g_linux_theme_phosphor = phosphor;
}

static inline linux_theme_colors_t linux_theme_current(void) {
    return linux_theme_get(g_linux_theme_mode, g_linux_theme_phosphor);
}

#endif /* LINUX_THEME_H */
