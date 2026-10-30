/**
 * @file plato_pdp1.h
 * @brief DEC PDP-1 Type 30 Precision CRT (1964) Optical Simulation Engine.
 */

#ifndef PLATO_PDP1_H
#define PLATO_PDP1_H

#include <stdint.h>
#include <stdbool.h>
#include "plato_pdp1_font.h"
#include "plato_types.h"
#include "plato_font.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PDP1_SCREEN_WIDTH        1024
#define PDP1_SCREEN_HEIGHT       1024
#define PDP1_TOTAL_PIXELS        (PDP1_SCREEN_WIDTH * PDP1_SCREEN_HEIGHT)

#define PDP1_FLASH_TAU_SEC       0.350f   /* ZnS:Ag long lingering flash (1.2-1.5s visible decay) */
#define PDP1_SUSTAIN_BETA        0.200f   /* (Zn,Cd)S:Cu bimolecular decay rate */
#define PDP1_BASE_SIGMA          1.100f   /* Spot PSF nominal radius with authentic CRT bloom */   /* Spot PSF nominal radius in pixels */

/* Native P7 cascade phosphor reference RGB triplets */
#define PDP1_P7_FLASH_R          0.900f   /* #60E1FF electric blue prompt flash */
#define PDP1_P7_FLASH_G          0.900f
#define PDP1_P7_FLASH_B          1.000f

#define PDP1_P7_SUSTAIN_R        0.000f   /* #00D4AA turquoise radar green sustain */
#define PDP1_P7_SUSTAIN_G        0.831f
#define PDP1_P7_SUSTAIN_B        0.667f

typedef enum {
    PDP1_FONT_MODE_1961_CODEWORD = 0,
    PDP1_FONT_MODE_1964_CHARACTER = 1
} pdp1_font_mode_t;

typedef enum {
    PDP1_PRESET_HISTORIC = 0,
    PDP1_PRESET_HISTORIC_FAST = 1,
    PDP1_PRESET_MODERN = 2
} pdp1_preset_t;

typedef struct {
    float r;
    float g;
    float b;
} pdp1_rgb_t;

typedef struct {
    float *flash_accum;
    float *sustain_accum;
    uint8_t *display_plane;     /* 1024x1024 Core Memory Display Buffer (Single Source of Truth) */
    float refresh_accumulator;  /* Virtual CPU core-memory refresh timer */

    float beam_intensity;
    bool  phosphor_override;
    pdp1_rgb_t override_color;
    pdp1_font_mode_t font_mode;
    int refresh_rate;
    pdp1_preset_t preset;

    float psf_lut[8][5][5];
} pdp1_context_t;

pdp1_context_t *pdp1_create(void);
void pdp1_destroy(pdp1_context_t *ctx);
void pdp1_clear(pdp1_context_t *ctx);

void pdp1_display_plane_clear(pdp1_context_t *ctx);
void pdp1_display_plane_set_pixel(pdp1_context_t *ctx, int x, int y);
void pdp1_display_plane_draw_glyph(pdp1_context_t *ctx, int x, int y, uint8_t c, int scale);
void pdp1_refresh_sweep(pdp1_context_t *ctx, uint8_t beam_level);

void pdp1_set_phosphor_override(pdp1_context_t *ctx, bool enable, float r, float g, float b);
void pdp1_set_font_mode(pdp1_context_t *ctx, pdp1_font_mode_t mode);
void pdp1_set_refresh_rate(pdp1_context_t *ctx, int hz);
void pdp1_set_preset(pdp1_context_t *ctx, pdp1_preset_t preset);
void pdp1_set_beam_intensity(pdp1_context_t *ctx, float intensity);

void pdp1_plot_point(pdp1_context_t *ctx, int x, int y, uint8_t level);
void pdp1_strike_point(pdp1_context_t *ctx, int x, int y, uint8_t level);
void pdp1_draw_line(pdp1_context_t *ctx, int x0, int y0, int x1, int y1, uint8_t level);
void pdp1_draw_glyph(pdp1_context_t *ctx, int x, int y, uint8_t c, uint8_t level, int scale);
void pdp1_project_plato_surface(pdp1_context_t *ctx, const uint8_t *plato_mono_512, uint8_t level);

void pdp1_simulate_step(pdp1_context_t *ctx, float dt_sec);
void pdp1_render_framebuffer(const pdp1_context_t *ctx, void *dst_pixels, int dst_stride_bytes, bool is_bgra);

/* Dedicated 1024x1024 DEC PDP-1 Type 30 Framebuffer Primitives */
void pdp1_fb_clear(uint8_t *fb);
void pdp1_fb_draw_point(uint8_t *fb, int x, int y, plato_screen_mode_t mode);
void pdp1_fb_draw_line(uint8_t *fb, int x0, int y0, int x1, int y1, plato_screen_mode_t mode);
void pdp1_fb_draw_block(uint8_t *fb, int x0, int y0, int x1, int y1, plato_screen_mode_t mode);
void pdp1_fb_draw_paint(uint8_t *fb, int start_x, int start_y, plato_screen_mode_t mode);
void pdp1_fb_draw_char(uint8_t *fb, const plato_font_t *native_font,
                       int font_mode, plato_charset_t charset, uint8_t ch,
                       int x, int y, plato_screen_mode_t mode, int scale);

#ifdef __cplusplus
}
#endif

#endif /* PLATO_PDP1_H */
