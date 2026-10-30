/**
 * @file plato_pdp1.c
 * @brief DEC PDP-1 Type 30 Precision CRT (1964) Engine Implementation.
 */

#include "plato_pdp1.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static inline float clampf(float v, float min_val, float max_val) {
    if (v < min_val) return min_val;
    if (v > max_val) return max_val;
    return v;
}

static inline uint8_t clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

static void precompute_psf(pdp1_context_t *ctx) {
    for (int lvl = 0; lvl < 8; ++lvl) {
        float rel_level = (float)lvl / 7.0f;
        float nom_sigma = (ctx->preset == PDP1_PRESET_MODERN) ? 0.450f : PDP1_BASE_SIGMA;
        float sigma = nom_sigma * (1.0f + 0.20f * rel_level);
        float two_sigma_sq = 2.0f * sigma * sigma;
        float base_intensity = (ctx->preset == PDP1_PRESET_MODERN) ? (0.95f + 0.15f * rel_level) : (0.85f + 0.45f * rel_level); /* Piena saturazione per riempire i vuoti tra i punti */

        for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
                float dist_sq = (float)(dx * dx + dy * dy);
                float weight = base_intensity * expf(-dist_sq / two_sigma_sq);
                if (ctx->preset == PDP1_PRESET_MODERN && dist_sq > 2.0f) weight = 0.0f;
                ctx->psf_lut[lvl][dy + 2][dx + 2] = weight;
            }
        }
    }
}

pdp1_context_t *pdp1_create(void) {
    pdp1_context_t *ctx = (pdp1_context_t *)calloc(1, sizeof(pdp1_context_t));
    if (!ctx) return NULL;

    ctx->flash_accum = (float *)calloc(PDP1_TOTAL_PIXELS, sizeof(float));
    ctx->sustain_accum = (float *)calloc(PDP1_TOTAL_PIXELS, sizeof(float));
    ctx->display_plane = (uint8_t *)calloc(PDP1_TOTAL_PIXELS, sizeof(uint8_t));

    if (!ctx->flash_accum || !ctx->sustain_accum || !ctx->display_plane) {
        pdp1_destroy(ctx);
        return NULL;
    }

    ctx->beam_intensity = 1.0f;
    ctx->phosphor_override = false;
    ctx->override_color = (pdp1_rgb_t){0.0f, 0.831f, 0.667f};
    ctx->font_mode = PDP1_FONT_MODE_1961_CODEWORD;
    ctx->refresh_rate = 45;
    ctx->preset = PDP1_PRESET_HISTORIC;
    ctx->refresh_accumulator = 0.0f;

    precompute_psf(ctx);
    return ctx;
}

void pdp1_destroy(pdp1_context_t *ctx) {
    if (!ctx) return;
    if (ctx->flash_accum) free(ctx->flash_accum);
    if (ctx->sustain_accum) free(ctx->sustain_accum);
    if (ctx->display_plane) free(ctx->display_plane);
    free(ctx);
}

void pdp1_clear(pdp1_context_t *ctx) {
    if (!ctx) return;
    if (ctx->display_plane) memset(ctx->display_plane, 0, PDP1_TOTAL_PIXELS);
    if (ctx->flash_accum) memset(ctx->flash_accum, 0, PDP1_TOTAL_PIXELS * sizeof(float));
    if (ctx->sustain_accum) memset(ctx->sustain_accum, 0, PDP1_TOTAL_PIXELS * sizeof(float));
    ctx->refresh_accumulator = 0.0f;
}

void pdp1_display_plane_clear(pdp1_context_t *ctx) {
    if (ctx && ctx->display_plane) {
        memset(ctx->display_plane, 0, PDP1_TOTAL_PIXELS);
    }
}

void pdp1_display_plane_set_pixel(pdp1_context_t *ctx, int x, int y) {
    if (ctx && ctx->display_plane && x >= 0 && x < PDP1_SCREEN_WIDTH && y >= 0 && y < PDP1_SCREEN_HEIGHT) {
        ctx->display_plane[y * PDP1_SCREEN_WIDTH + x] = 1;
    }
}

void pdp1_display_plane_draw_glyph(pdp1_context_t *ctx, int x, int y, uint8_t c, int scale) {
    if (!ctx || !ctx->display_plane) return;
    const pdp1_glyph_t *glyph = (ctx->font_mode == PDP1_FONT_MODE_1961_CODEWORD)
        ? pdp1_font_1961_get_glyph(c)
        : pdp1_font_1964_get_glyph(c);

    if (!glyph) return;

    int dot_step = (scale > 0) ? scale : 1;
    int y_start = y;
    if (glyph->descender) {
        y_start += (2 * dot_step);
    }

    for (int r = 0; r < 7; ++r) {
        uint8_t row_bits = glyph->rows[r];
        int py = y_start + (r * dot_step);

        for (int col = 0; col < 5; ++col) {
            if (row_bits & (0x10 >> col)) {
                int px = x + (col * dot_step);
                pdp1_display_plane_set_pixel(ctx, px, py);
            }
        }
    }
}

void pdp1_refresh_sweep(pdp1_context_t *ctx, uint8_t beam_level) {
    if (!ctx || !ctx->display_plane) return;
    int idx = 0;
    for (int y = 0; y < PDP1_SCREEN_HEIGHT; ++y) {
        for (int x = 0; x < PDP1_SCREEN_WIDTH; ++x, ++idx) {
            if (ctx->display_plane[idx]) {
                pdp1_plot_point(ctx, x, y, beam_level);
            }
        }
    }
}

void pdp1_set_phosphor_override(pdp1_context_t *ctx, bool enable, float r, float g, float b) {
    if (!ctx) return;
    ctx->phosphor_override = enable;
    ctx->override_color.r = clampf(r, 0.0f, 1.0f);
    ctx->override_color.g = clampf(g, 0.0f, 1.0f);
    ctx->override_color.b = clampf(b, 0.0f, 1.0f);
}

void pdp1_set_font_mode(pdp1_context_t *ctx, pdp1_font_mode_t mode) {
    if (!ctx) return;
    ctx->font_mode = mode;
}

void pdp1_set_beam_intensity(pdp1_context_t *ctx, float intensity) {
    if (!ctx) return;
    ctx->beam_intensity = clampf(intensity, 0.1f, 3.0f);
}


void pdp1_strike_point(pdp1_context_t *ctx, int x, int y, uint8_t level) {
    if (!ctx || x < 0 || x >= PDP1_SCREEN_WIDTH || y < 0 || y >= PDP1_SCREEN_HEIGHT) return;
    uint8_t lvl = (level > 7) ? 7 : level;
    float gain = ctx->beam_intensity;

    for (int dy = -2; dy <= 2; ++dy) {
        int py = y + dy;
        if (py < 0 || py >= PDP1_SCREEN_HEIGHT) continue;
        int row_offset = py * PDP1_SCREEN_WIDTH;

        for (int dx = -2; dx <= 2; ++dx) {
            int px = x + dx;
            if (px < 0 || px >= PDP1_SCREEN_WIDTH) continue;

            float w = ctx->psf_lut[lvl][dy + 2][dx + 2] * gain;
            int idx = row_offset + px;

            /* Prompt flash iniziale immediato a saturazione piena */
            float f_val = ctx->flash_accum[idx] + (w * 4.50f);
            ctx->flash_accum[idx] = (f_val > 5.00f) ? 5.00f : f_val;

            /* Il sustain parte SUBITO al massimo (1.0), MAI dal basso! */
            float s_target = w * 1.00f;
            if (ctx->sustain_accum[idx] < s_target) {
                ctx->sustain_accum[idx] = s_target;
            }
        }
    }
}

void pdp1_plot_point(pdp1_context_t *ctx, int x, int y, uint8_t level) {
    if (!ctx || x < 0 || x >= PDP1_SCREEN_WIDTH || y < 0 || y >= PDP1_SCREEN_HEIGHT) {
        return;
    }

    uint8_t lvl = (level > 7) ? 7 : level;
    float gain = ctx->beam_intensity;

    for (int dy = -2; dy <= 2; ++dy) {
        int py = y + dy;
        if (py < 0 || py >= PDP1_SCREEN_HEIGHT) continue;
        int row_offset = py * PDP1_SCREEN_WIDTH;

        for (int dx = -2; dx <= 2; ++dx) {
            int px = x + dx;
            if (px < 0 || px >= PDP1_SCREEN_WIDTH) continue;

            float w = ctx->psf_lut[lvl][dy + 2][dx + 2] * gain;
            int idx = row_offset + px;

            /* Piccolo rinfresco per il battito a 45 Hz: non azzera mai un flash piu' alto gia' presente */
            float f_topup = w * 0.20f;
            if (ctx->flash_accum[idx] < f_topup) {
                ctx->flash_accum[idx] = f_topup;
            }

            /* Il refresh mantiene il punto al livello nominale (1.0), NON lo carica lentamente a rate! */
            float s_target = w * 1.00f;
            if (ctx->sustain_accum[idx] < s_target) {
                ctx->sustain_accum[idx] = s_target;
            }
        }
    }
}

void pdp1_draw_line(pdp1_context_t *ctx, int x0, int y0, int x1, int y1, uint8_t level) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    for (;;) {
        pdp1_plot_point(ctx, x0, y0, level);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void pdp1_draw_glyph(pdp1_context_t *ctx, int x, int y, uint8_t c, uint8_t level, int scale) {
    if (!ctx) return;
    const pdp1_glyph_t *glyph = (ctx->font_mode == PDP1_FONT_MODE_1961_CODEWORD)
        ? pdp1_font_1961_get_glyph(c)
        : pdp1_font_1964_get_glyph(c);

    if (!glyph) return;

    int dot_step = (scale > 0) ? scale : 1;
    int y_start = y;
    if (glyph->descender) {
        y_start += (2 * dot_step);
    }

    for (int r = 0; r < 7; ++r) {
        uint8_t row_bits = glyph->rows[r];
        int py = y_start + (r * dot_step);

        for (int col = 0; col < 5; ++col) {
            if (row_bits & (0x10 >> col)) {
                int px = x + (col * dot_step);
                pdp1_plot_point(ctx, px, py, level);
            }
        }
    }
}

void pdp1_project_plato_surface(pdp1_context_t *ctx, const uint8_t *plato_mono_512, uint8_t level) {
    if (!ctx || !plato_mono_512) return;

    for (int py = 0; py < 512; ++py) {
        int src_offset = py * 512;
        int dst_y = py * 2;

        for (int px = 0; px < 512; ++px) {
            if (plato_mono_512[src_offset + px]) {
                int dst_x = px * 2;
                pdp1_plot_point(ctx, dst_x, dst_y, level);
                pdp1_plot_point(ctx, dst_x + 1, dst_y, level);
                pdp1_plot_point(ctx, dst_x, dst_y + 1, level);
                pdp1_plot_point(ctx, dst_x + 1, dst_y + 1, level);
            }
        }
    }
}

void pdp1_simulate_step(pdp1_context_t *ctx, float dt_sec) {
    if (!ctx || !ctx->flash_accum || !ctx->sustain_accum || dt_sec <= 0.0f) return;

    float tau_f = PDP1_FLASH_TAU_SEC;
    float beta_s = PDP1_SUSTAIN_BETA;
    if (ctx->preset == PDP1_PRESET_HISTORIC_FAST) { tau_f = 0.120f; beta_s = 0.600f; }
    else if (ctx->preset == PDP1_PRESET_MODERN) { tau_f = 0.050f; beta_s = 1.500f; }
    float flash_factor = expf(-dt_sec / tau_f);
    float sustain_denom_delta = beta_s * dt_sec;

    for (int i = 0; i < PDP1_TOTAL_PIXELS; ++i) {
        /* Flash prompt decay (ZnS:Ag) */
        float f = ctx->flash_accum[i] * flash_factor;
        ctx->flash_accum[i] = (f < 0.001f) ? 0.0f : f;

        /* Persistent sustain decay (Zn,Cd)S:Cu */
        float s = ctx->sustain_accum[i];
        if (s > 0.001f) {
            float s_next = s / (1.0f + sustain_denom_delta);
            ctx->sustain_accum[i] = (s_next < 0.001f) ? 0.0f : s_next;
        } else {
            ctx->sustain_accum[i] = 0.0f;
        }
    }
}

void pdp1_render_framebuffer(const pdp1_context_t *ctx, void *dst_pixels, int dst_stride_bytes, bool is_bgra) {
    if (!ctx || !dst_pixels || !ctx->flash_accum || !ctx->sustain_accum) return;

    pdp1_rgb_t flash_color;
    pdp1_rgb_t sustain_color;

    if (!ctx->phosphor_override) {
        flash_color = (pdp1_rgb_t){PDP1_P7_FLASH_R, PDP1_P7_FLASH_G, PDP1_P7_FLASH_B};
        sustain_color = (pdp1_rgb_t){PDP1_P7_SUSTAIN_R, PDP1_P7_SUSTAIN_G, PDP1_P7_SUSTAIN_B};
    } else {
        sustain_color = ctx->override_color;
        flash_color.r = 0.40f * ctx->override_color.r + 0.60f;
        flash_color.g = 0.40f * ctx->override_color.g + 0.60f;
        flash_color.b = 0.40f * ctx->override_color.b + 0.60f;
    }

    uint8_t *dst_row = (uint8_t *)dst_pixels;
    int src_idx = 0;

    for (int y = 0; y < PDP1_SCREEN_HEIGHT; ++y) {
        uint32_t *dst_ptr = (uint32_t *)dst_row;

        for (int x = 0; x < PDP1_SCREEN_WIDTH; ++x) {
            float f = ctx->flash_accum[src_idx];
            float s = ctx->sustain_accum[src_idx];
            src_idx++;

            if (f <= 0.0001f && s <= 0.0001f) {
                *dst_ptr++ = 0xFF000000;
                continue;
            }

            float r = f * flash_color.r + s * sustain_color.r;
            float g = f * flash_color.g + s * sustain_color.g;
            float b = f * flash_color.b + s * sustain_color.b;

            uint8_t ru8 = clamp_u8((int)(r * 255.0f));
            uint8_t gu8 = clamp_u8((int)(g * 255.0f));
            uint8_t bu8 = clamp_u8((int)(b * 255.0f));

            if (is_bgra) {
                *dst_ptr++ = (uint32_t)(0xFF000000 | (ru8 << 16) | (gu8 << 8) | bu8);
            } else {
                *dst_ptr++ = (uint32_t)(0xFF000000 | (bu8 << 16) | (gu8 << 8) | ru8);
            }
        }
        dst_row += dst_stride_bytes;
    }
}


/* =========================================================================
 * DEC PDP-1 Type 30 Dedicated Framebuffer (1024x1024) Operations
 * ========================================================================= */

void pdp1_fb_clear(uint8_t *fb) {
    if (fb) {
        memset(fb, 0, PDP1_TOTAL_PIXELS);
    }
}

static inline void pdp1_fb_set_pixel_raw(uint8_t *fb, int x, int y, plato_screen_mode_t mode, uint8_t val) {
    if (x < 0 || x >= PDP1_SCREEN_WIDTH || y < 0 || y >= PDP1_SCREEN_HEIGHT) return;
    int idx = y * PDP1_SCREEN_WIDTH + x;
    switch (mode) {
        case PLATO_SCREEN_WRITE:
            if (val) fb[idx] = 1;
            break;
        case PLATO_SCREEN_ERASE:
            if (val) fb[idx] = 0;
            break;
        case PLATO_SCREEN_REWRITE:
            fb[idx] = val ? 1 : 0;
            break;
        case PLATO_SCREEN_INVERSE:
            if (val) fb[idx] ^= 1;
            break;
    }
}

void pdp1_fb_draw_point(uint8_t *fb, int x, int y, plato_screen_mode_t mode) {
    if (!fb || x < 0 || x >= 512 || y < 0 || y >= 512) return;
    int px = x * 2;
    int py = (511 - y) * 2;
    pdp1_fb_set_pixel_raw(fb, px, py, mode, 1);
    pdp1_fb_set_pixel_raw(fb, px + 1, py, mode, 1);
    pdp1_fb_set_pixel_raw(fb, px, py + 1, mode, 1);
    pdp1_fb_set_pixel_raw(fb, px + 1, py + 1, mode, 1);
}

void pdp1_fb_draw_line(uint8_t *fb, int x0, int y0, int x1, int y1, plato_screen_mode_t mode) {
    if (!fb) return;
    int px0 = x0 * 2;
    int py0 = (511 - y0) * 2;
    int px1 = x1 * 2;
    int py1 = (511 - y1) * 2;

    int dx = abs(px1 - px0);
    int dy = abs(py1 - py0);
    int sx = (px0 < px1) ? 1 : -1;
    int sy = (py0 < py1) ? 1 : -1;
    int err = dx - dy;

    for (;;) {
        pdp1_fb_set_pixel_raw(fb, px0, py0, mode, 1);
        if (px0 == px1 && py0 == py1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            px0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            py0 += sy;
        }
    }
}

void pdp1_fb_draw_block(uint8_t *fb, int x0, int y0, int x1, int y1, plato_screen_mode_t mode) {
    if (!fb) return;
    int min_x = (x0 < x1) ? x0 : x1;
    int max_x = (x0 > x1) ? x0 : x1;
    int min_y = (y0 < y1) ? y0 : y1;
    int max_y = (y0 > y1) ? y0 : y1;

    int px0 = min_x * 2;
    int px1 = max_x * 2 + 1;
    int py0 = (511 - max_y) * 2;
    int py1 = (511 - min_y) * 2 + 1;

    if (px0 < 0) px0 = 0;
    if (px1 >= PDP1_SCREEN_WIDTH) px1 = PDP1_SCREEN_WIDTH - 1;
    if (py0 < 0) py0 = 0;
    if (py1 >= PDP1_SCREEN_HEIGHT) py1 = PDP1_SCREEN_HEIGHT - 1;

    /* Semantica Vettoriale Pura: i blocchi solidi bruciano il CRT.
       Qualsiasi comando BLOCK viene trattato come una "Gomma" (CLEAR/ERASE)
       per pulire l'area prima di scriverci vettori luminosi. */
    for (int y = py0; y <= py1; ++y) {
        for (int x = px0; x <= px1; ++x) {
            pdp1_fb_set_pixel_raw(fb, x, y, PLATO_SCREEN_ERASE, 1);
        }
    }
}

void pdp1_fb_draw_paint(uint8_t *fb, int start_x, int start_y, plato_screen_mode_t mode) {
    if (!fb || start_x < 0 || start_x >= 512 || start_y < 0 || start_y >= 512) return;
    int px = start_x * 2;
    int py = (511 - start_y) * 2;
    if (fb[py * PDP1_SCREEN_WIDTH + px]) return;

    typedef struct { int16_t x, y; } pdp_pt_t;
    pdp_pt_t *q = (pdp_pt_t *)malloc(sizeof(pdp_pt_t) * PDP1_TOTAL_PIXELS);
    if (!q) return;

    uint8_t *visited = (uint8_t *)calloc(PDP1_TOTAL_PIXELS / 8, 1);
    if (!visited) { free(q); return; }

    size_t head = 0, tail = 0;
    q[tail++] = (pdp_pt_t){(int16_t)px, (int16_t)py};
    int init_idx = py * PDP1_SCREEN_WIDTH + px;
    visited[init_idx / 8] |= (uint8_t)(1 << (init_idx % 8));

    while (head < tail) {
        pdp_pt_t p = q[head++];
        pdp1_fb_set_pixel_raw(fb, p.x, p.y, mode, 1);

        const int dxs[4] = {0, 0, -1, 1};
        const int dys[4] = {-1, 1, 0, 0};
        for (int d = 0; d < 4; ++d) {
            int nx = p.x + dxs[d];
            int ny = p.y + dys[d];
            if (nx >= 0 && nx < PDP1_SCREEN_WIDTH && ny >= 0 && ny < PDP1_SCREEN_HEIGHT) {
                int nidx = ny * PDP1_SCREEN_WIDTH + nx;
                if (!(visited[nidx / 8] & (1 << (nidx % 8))) && !fb[nidx]) {
                    visited[nidx / 8] |= (uint8_t)(1 << (nidx % 8));
                    q[tail++] = (pdp_pt_t){(int16_t)nx, (int16_t)ny};
                }
            }
        }
    }
    free(visited);
    free(q);
}

void pdp1_fb_draw_char(uint8_t *fb, const plato_font_t *native_font,
                       int font_mode, plato_charset_t charset, uint8_t ch,
                       int x, int y, plato_screen_mode_t mode, int scale) {
    if (!fb) return;
    int s = (scale <= 1) ? 1 : scale;
    int y_base = y + ((s > 1) ? (s - 1) * 5 : 0);
    int plato_top_y = y_base + (16 - 1) * s;
    int pdp_top_y = (511 - plato_top_y) * 2;
    int pdp_left_x = x * 2;
    int cell_w = 8 * s * 2;
    int cell_h = 16 * s * 2;

    /* REWRITE e INVERSE: pulizia della cella senza sfondi solidi */
    if (mode == PLATO_SCREEN_REWRITE || mode == PLATO_SCREEN_INVERSE) {
        for (int cy = 0; cy < cell_h; ++cy) {
            int row_y = pdp_top_y + cy;
            if (row_y < 0 || row_y >= PDP1_SCREEN_HEIGHT) continue;
            int row_offset = row_y * PDP1_SCREEN_WIDTH;
            for (int cx = 0; cx < cell_w; ++cx) {
                int col_x = pdp_left_x + cx;
                if (col_x >= 0 && col_x < PDP1_SCREEN_WIDTH) {
                    fb[row_offset + col_x] = 0;
                }
            }
        }
    }

    if (charset == PLATO_CHARSET_M0) {
        if (ch <= 0x20 || ch > 0x7E) return;
        const pdp1_glyph_t *glyph = (font_mode == PDP1_FONT_MODE_1964_CHARACTER)
            ? pdp1_font_1964_get_glyph(ch)
            : pdp1_font_1961_get_glyph(ch);
        if (!glyph) return;

        int dot_step = 2 * s;
        int glyph_x = pdp_left_x + (3 * s);
        int glyph_y = pdp_top_y + (9 * s);
        if (glyph->descender) {
            glyph_y += (2 * dot_step);
        }

        for (int r = 0; r < 7; ++r) {
            uint8_t row_bits = glyph->rows[r];
            int py = glyph_y + (r * dot_step);
            for (int col = 0; col < 5; ++col) {
                if (row_bits & (0x10 >> col)) {
                    int px = glyph_x + (col * dot_step);
                    if (mode == PLATO_SCREEN_ERASE) {
                        pdp1_fb_set_pixel_raw(fb, px, py, PLATO_SCREEN_ERASE, 1);
                        if (s > 1) {
                            for (int dy = 0; dy < s; ++dy) {
                                for (int dx = 0; dx < s; ++dx) {
                                    pdp1_fb_set_pixel_raw(fb, px + dx, py + dy, PLATO_SCREEN_ERASE, 1);
                                }
                            }
                        }
                    } else {
                        /* WRITE, REWRITE, INVERSE producono tutti tratti luminosi */
                        pdp1_fb_set_pixel_raw(fb, px, py, PLATO_SCREEN_WRITE, 1);
                        if (s > 1) {
                            for (int dy = 0; dy < s; ++dy) {
                                for (int dx = 0; dx < s; ++dx) {
                                    pdp1_fb_set_pixel_raw(fb, px + dx, py + dy, PLATO_SCREEN_WRITE, 1);
                                }
                            }
                        }
                    }
                }
            }
        }
    } else {
        if (!native_font) return;
        const uint8_t *glyph = plato_font_get_glyph(native_font, charset, ch);
        if (!glyph) return;

        int px_step = 2 * s;
        for (int row = 0; row < 16; ++row) {
            uint8_t bits = glyph[row];
            int py = pdp_top_y + (row * px_step);
            for (int col = 0; col < 8; ++col) {
                bool bit_on = (bits & (0x80 >> col)) != 0;
                if (!bit_on) continue;
                int px = pdp_left_x + (col * px_step);
                for (int dy = 0; dy < px_step; ++dy) {
                    for (int dx = 0; dx < px_step; ++dx) {
                        int tx = px + dx;
                        int ty = py + dy;
                        if (mode == PLATO_SCREEN_ERASE) {
                            pdp1_fb_set_pixel_raw(fb, tx, ty, PLATO_SCREEN_ERASE, 1);
                        } else {
                            pdp1_fb_set_pixel_raw(fb, tx, ty, PLATO_SCREEN_WRITE, 1);
                        }
                    }
                }
            }
        }
    }
}

void pdp1_set_refresh_rate(pdp1_context_t *ctx, int hz) {
    if (!ctx) return;
    ctx->refresh_rate = (hz == 60 || hz == 45 || hz == 30 || hz == 15 || hz == 10 || hz == 5) ? hz : 45;
}

void pdp1_set_preset(pdp1_context_t *ctx, pdp1_preset_t preset) {
    if (!ctx) return;
    if (ctx->preset != preset) {
        ctx->preset = preset;
        precompute_psf(ctx);
    }
}
