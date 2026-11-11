/*
 * PlatoLives Native Client - Commodore 64 VIC-II Display Simulation Engine
 * Implementation of VIC-II Hires Bitmap, CRT / RF Simulation & Color Pipeline
 */

#include "plato/plato_c64.h"
#include "plato/plato_c64_font.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define CANVAS_W 2048
#define CANVAS_H 2048

#define BORDER_THICKNESS 256
#define ACTIVE_W_BORDER  1536
#define ACTIVE_H_BORDER  1536
#define SCALE_BORDER     3
#define SCALE_NO_BORDER  4

#define C64_HIRES_W 320
#define C64_HIRES_H 200

static const uint32_t s_vic_palette[16] = {
    C64_COL_BLACK,
    C64_COL_WHITE,
    C64_COL_RED,
    C64_COL_CYAN,
    C64_COL_PURPLE,
    C64_COL_GREEN,
    C64_COL_DARK_BLUE,
    C64_COL_YELLOW,
    C64_COL_ORANGE,
    C64_COL_BROWN,
    C64_COL_LIGHT_RED,
    C64_COL_DARK_GREY,
    C64_COL_GREY,
    C64_COL_LIGHT_GREEN,
    C64_COL_LIGHT_BLUE,
    C64_COL_LIGHT_GREY
};

static uint32_t c64_match_palette_color(uint32_t col) {
    int b = col & 0xFF, g = (col >> 8) & 0xFF, r = (col >> 16) & 0xFF;
    int best_idx = 0, best_dist = 0x7FFFFFFF;
    for (int i = 0; i < 16; i++) {
        uint32_t pal_c = s_vic_palette[i];
        int pb = pal_c & 0xFF, pg = (pal_c >> 8) & 0xFF, pr = (pal_c >> 16) & 0xFF;
        int dr = r - pr, dg = g - pg, db = b - pb;
        int dist = dr * dr + dg * dg + db * db;
        if (dist < best_dist) { best_dist = dist; best_idx = i; }
    }
    return s_vic_palette[best_idx];
}

struct c64_context {
    uint8_t hires_grid[C64_HIRES_H][C64_HIRES_W];
    double last_time;
    uint32_t frame_count;
    double last_rx_time;
    size_t rx_accumulator;
    size_t last_processed_rx;
    uint32_t last_clear_count;
    double fx_start_time;
    double last_clear_time;
    bool fx_running;
};

void c64_notify_rx(c64_context_t *ctx, size_t bytes) {
    if (!ctx) return;
    ctx->rx_accumulator += bytes;
}

c64_context_t *c64_create(void) {
    return (c64_context_t *)calloc(1, sizeof(c64_context_t));
}

void c64_destroy(c64_context_t *ctx) {
    if (ctx) free(ctx);
}

void c64_clear(c64_context_t *ctx) {
    if (ctx) memset(ctx->hires_grid, 0, sizeof(ctx->hires_grid));
}

void c64_unwarp_touch(int *x, int *y, bool border_enabled) {
    if (!x || !y) return;
    int cx = *x;
    int cy = *y;
    if (border_enabled) {
        int px = (cx - BORDER_THICKNESS) / SCALE_BORDER;
        int py = (cy - BORDER_THICKNESS) / SCALE_BORDER;
        if (px < 0) px = 0; else if (px > 511) px = 511;
        if (py < 0) py = 0; else if (py > 511) py = 511;
        *x = px;
        *y = py;
    } else {
        int px = cx / SCALE_NO_BORDER;
        int py = cy / SCALE_NO_BORDER;
        if (px < 0) px = 0; else if (px > 511) px = 511;
        if (py < 0) py = 0; else if (py > 511) py = 511;
        *x = px;
        *y = py;
    }
}

static inline uint32_t safe_dim_pixel(uint32_t c, float factor) {
    uint32_t b = (uint32_t)((c & 0xFF) * factor);
    uint32_t g = (uint32_t)(((c >> 8) & 0xFF) * factor);
    uint32_t r = (uint32_t)(((c >> 16) & 0xFF) * factor);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

#define SAFE_DIM(c, f, bx, by) \
    ((border > 0 && ((bx) < BORDER_THICKNESS || \
                     (bx) >= CANVAS_W - BORDER_THICKNESS || \
                     (by) < BORDER_THICKNESS || \
                     (by) >= CANVAS_H - BORDER_THICKNESS)) \
         ? (c) : safe_dim_pixel((c), (f)))

void c64_render(c64_context_t *ctx,
                const plato_terminal_t *term,
                const plato_profile_t *prof,
                uint32_t *out_bgra,
                double now_sec) {
    if (!ctx || !term || !out_bgra || !prof) return;

    ctx->frame_count++;
    ctx->last_time = now_sec;

    int border = prof->c64_border;
    int preset = prof->c64_preset;
    int scheme = prof->c64_color_scheme;

    uint32_t col_border, col_bg, col_fg;

    if (scheme == 1) {
        /* Multi-Color non applicabile a Single-Source, ripieghiamo su Classic Blue */
        col_border = C64_COL_LIGHT_BLUE; 
        col_bg     = C64_COL_DARK_BLUE;  
        col_fg     = C64_COL_LIGHT_BLUE;
    } else if (scheme == 2) {
        /* Calibrated Amber Monitor P40 */
        col_border = C64_BGRA(16, 8, 0);     /* #100800 */
        col_bg     = C64_COL_BLACK;          
        col_fg     = C64_BGRA(255, 176, 0);  /* #FFB000 */
    } else if (scheme == 3) {
        /* Calibrated Green Monitor P31 */
        col_border = C64_BGRA(0, 20, 4);     /* #001404 */
        col_bg     = C64_COL_BLACK;          
        col_fg     = C64_BGRA(51, 255, 51);  /* #33FF33 */
    } else {
        /* Classic Blue Two-Tone (Default C64 BASIC) */
        col_border = C64_COL_LIGHT_BLUE; 
        col_bg     = C64_COL_DARK_BLUE;  
        col_fg     = C64_COL_LIGHT_BLUE; 
    }

        int active_w = border > 0 ? ACTIVE_W_BORDER : CANVAS_W;
    int active_h = border > 0 ? ACTIVE_H_BORDER : CANVAS_H;
    int base_x   = border > 0 ? BORDER_THICKNESS : 0;
    int base_y   = border > 0 ? BORDER_THICKNESS : 0;
    int scale    = border > 0 ? SCALE_BORDER : SCALE_NO_BORDER;

    /* 1. Monitoraggio traffico RX: tiene vivo il loader durante il flusso dati */
    if (ctx->rx_accumulator > ctx->last_processed_rx) {
        ctx->last_processed_rx = ctx->rx_accumulator;
        ctx->last_rx_time = now_sec;
    }

    /* 2. Logica Hardware Clear Screen + Ignore Continui (Anti-Asteroids Strobe) */
    if (term->clear_count != ctx->last_clear_count) {
        ctx->last_clear_count = term->clear_count;
        
        bool is_valid_trigger = (now_sec - ctx->last_clear_time >= 1.0);
        ctx->last_clear_time = now_sec;
        
        if (is_valid_trigger) {
            ctx->fx_start_time = now_sec;
            ctx->last_rx_time = now_sec;
            ctx->fx_running = true;
        }
    }

    /* 3. Stop e Hard Timeout 5.0s */
    if (ctx->fx_running) {
        if ((now_sec - ctx->fx_start_time >= 3.0) || (now_sec - ctx->last_rx_time >= 0.3)) {
            ctx->fx_running = false;
        }
    }

    bool is_loading = (border == 2) && ctx->fx_running;

    if (border > 0) {
        for (int y = 0; y < CANVAS_H; y++) {
            bool is_top_bottom = (y < BORDER_THICKNESS) || (y >= CANVAS_H - BORDER_THICKNESS);
            uint32_t line_color = col_border;
            if (is_loading) {
                int time_shift = (int)(now_sec * 8000.0);
                int chunk = (y + time_shift) / 64;
                int color_idx = ((chunk * 13) ^ (ctx->frame_count * 7)) & 15;
                line_color = s_vic_palette[color_idx];
            }
            
            uint32_t *row = out_bgra + y * CANVAS_W;
            if (is_top_bottom) {
                for (int x = 0; x < CANVAS_W; x++) row[x] = line_color;
            } else {
                for (int x = 0; x < BORDER_THICKNESS; x++) row[x] = line_color;
                for (int x = CANVAS_W - BORDER_THICKNESS; x < CANVAS_W; x++) row[x] = line_color;
            }
        }
    }

    /* 2. Sfondo Display Centrale ($D021) */
    for (int y = 0; y < active_h; y++) {
        uint32_t *row = out_bgra + (base_y + y) * CANVAS_W + base_x;
        for (int x = 0; x < active_w; x++) row[x] = col_bg;
    }

    /* 3. FUSIONE GRAFICA VETTORIALE 320x200 (Bresenham scalettato autentico C64) */
    if (term->fb_c64_gfx) {
        for (int vy = 0; vy < C64_HIRES_H; vy++) {
            int canvas_vy = (C64_HIRES_H - 1) - vy; /* Y=0 in alto su Canvas */
            int dy0 = base_y + (canvas_vy * active_h) / C64_HIRES_H;
            int dy1 = base_y + ((canvas_vy + 1) * active_h) / C64_HIRES_H;

            for (int vx = 0; vx < C64_HIRES_W; vx++) {
                int idx = vy * C64_HIRES_W + vx;
                if (term->fb_c64_gfx[idx]) {
                    uint32_t c = col_fg;
                    if (scheme == 1 && term->fb_c64_gfx_color) {
                        uint32_t raw_c = term->fb_c64_gfx_color[idx];
                        if (raw_c != 0) c = c64_match_palette_color(raw_c);
                        /* Se il colore coincide con lo sfondo, eleva a Light Blue per visibilita */
                        if (c == col_bg || c == C64_COL_BLACK) c = C64_COL_LIGHT_BLUE;
                    }
                    int dx0 = base_x + (vx * active_w) / C64_HIRES_W;
                    int dx1 = base_x + ((vx + 1) * active_w) / C64_HIRES_W;
                    for (int y = dy0; y < dy1; y++) {
                        uint32_t *drow = out_bgra + y * CANVAS_W;
                        for (int x = dx0; x < dx1; x++) drow[x] = c;
                    }
                }
            }
        }
    }

    /* 4. FUSIONE TESTO PETSCII/HD 512x512 */
    if (term->fb_c64_text) {
        for (int ty = 0; ty < 512; ty++) {
            int canvas_ty = 511 - ty;
            int base_dst_y = base_y + canvas_ty * scale;

            for (int tx = 0; tx < 512; tx++) {
                int idx = ty * 512 + tx;
                if (term->fb_c64_text[idx]) {
                    uint32_t c = col_fg;
                    if (scheme == 1 && term->fb_c64_text_color) {
                        uint32_t raw_c = term->fb_c64_text_color[idx];
                        if (raw_c != 0) c = c64_match_palette_color(raw_c);
                        if (c == col_bg || c == C64_COL_BLACK) c = C64_COL_LIGHT_BLUE;
                    }
                    int base_dst_x = base_x + tx * scale;
                    for (int sy = 0; sy < scale; sy++) {
                        uint32_t *drow = out_bgra + (base_dst_y + sy) * CANVAS_W + base_dst_x;
                        for (int sx = 0; sx < scale; sx++) drow[sx] = c;
                    }
                }
            }
        }
    }

    /* 6. APPLICAZIONE PRESET OTTICI (RF COMPOSITE / 1702 CRT) */
    if (preset == 0) {
        /* PRESET 0: Authentic RF / Composite - Heavy Smear */
        int start_y = border > 0 ? BORDER_THICKNESS : 0;
        int end_y   = border > 0 ? (CANVAS_H - BORDER_THICKNESS) : CANVAS_H;
        int start_x = border > 0 ? BORDER_THICKNESS : 0;
        int end_x   = border > 0 ? (CANVAS_W - BORDER_THICKNESS) : CANVAS_W;

        for (int y = start_y; y < end_y; y++) {
            uint32_t *row = out_bgra + y * CANVAS_W;
            bool is_scanline = ((y - start_y) % scale) == (scale - 1);
            float scan_factor = is_scanline ? 0.70f : 1.0f; /* Scanlines molto più marcate */

            uint32_t c_prev1 = row[start_x];
            uint32_t c_prev2 = row[start_x];
            
            for (int x = start_x; x < end_x; x++) {
                uint32_t cur = row[x];
                /* Filtro FIR pesante 50% / 30% / 20% per simulare la sbavatura RF */
                uint32_t b_cur = cur & 0xFF, g_cur = (cur >> 8) & 0xFF, r_cur = (cur >> 16) & 0xFF;
                uint32_t b_p1 = c_prev1 & 0xFF, g_p1 = (c_prev1 >> 8) & 0xFF, r_p1 = (c_prev1 >> 16) & 0xFF;
                uint32_t b_p2 = c_prev2 & 0xFF, g_p2 = (c_prev2 >> 8) & 0xFF, r_p2 = (c_prev2 >> 16) & 0xFF;
                
                uint32_t r_f = (r_cur * 50 + r_p1 * 30 + r_p2 * 20) / 100;
                uint32_t g_f = (g_cur * 50 + g_p1 * 30 + g_p2 * 20) / 100;
                uint32_t b_f = (b_cur * 50 + b_p1 * 30 + b_p2 * 20) / 100;
                
                uint32_t blended = 0xFF000000u | (r_f << 16) | (g_f << 8) | b_f;
                
                c_prev2 = c_prev1;
                c_prev1 = cur;
                
                if (is_scanline) blended = SAFE_DIM(blended, scan_factor, x, y);
                row[x] = blended;
            }
        }
    } else if (preset == 1) {
        /* PRESET 1: Commodore 1702 CRT */
        int start_y = border > 0 ? BORDER_THICKNESS : 0;
        int end_y   = border > 0 ? (CANVAS_H - BORDER_THICKNESS) : CANVAS_H;
        int start_x = border > 0 ? BORDER_THICKNESS : 0;
        int end_x   = border > 0 ? (CANVAS_W - BORDER_THICKNESS) : CANVAS_W;

        for (int y = start_y; y < end_y; y++) {
            if (((y - start_y) % scale) == (scale - 1)) {
                uint32_t *row = out_bgra + y * CANVAS_W;
                for (int x = start_x; x < end_x; x++) {
                    row[x] = SAFE_DIM(row[x], 0.78f, x, y);
                }
            }
        }
    }
}


/* === C64 DUAL-FRAMEBUFFER PRIMITIVES (HIRES GFX 320x200 & PETSCII TEXT 512x512) === */

void c64_fb_clear(plato_terminal_t *term) {
    if (!term) return;
    if (term->fb_c64_gfx) memset(term->fb_c64_gfx, 0, C64_HIRES_W * C64_HIRES_H * sizeof(uint8_t));
    if (term->fb_c64_gfx_color) memset(term->fb_c64_gfx_color, 0, C64_HIRES_W * C64_HIRES_H * sizeof(uint32_t));
    if (term->fb_c64_text) memset(term->fb_c64_text, 0, 512 * 512 * sizeof(uint8_t));
    if (term->fb_c64_text_color) memset(term->fb_c64_text_color, 0, 512 * 512 * sizeof(uint32_t));
    term->fb_c64_dirty = true;
}

void c64_fb_draw_point(plato_terminal_t *term, int x, int y, plato_screen_mode_t mode) {
    if (!term || !term->fb_c64_gfx || x < 0 || x >= 512 || y < 0 || y >= 512) return;
    int vx = (x * C64_HIRES_W) / 512;
    int vy = (y * C64_HIRES_H) / 512;
    if (vx < 0) vx = 0; else if (vx >= C64_HIRES_W) vx = C64_HIRES_W - 1;
    if (vy < 0) vy = 0; else if (vy >= C64_HIRES_H) vy = C64_HIRES_H - 1;

    int idx = vy * C64_HIRES_W + vx;
    uint32_t color = term->fb.fg_color ? term->fb.fg_color : 0xFFFFFFFFu;

    switch (mode) {
        case PLATO_SCREEN_WRITE:
        case PLATO_SCREEN_REWRITE:
            term->fb_c64_gfx[idx] = 1;
            if (term->fb_c64_gfx_color) term->fb_c64_gfx_color[idx] = color;
            break;
        case PLATO_SCREEN_ERASE:
            term->fb_c64_gfx[idx] = 0;
            if (term->fb_c64_gfx_color) term->fb_c64_gfx_color[idx] = 0;
            break;
        case PLATO_SCREEN_INVERSE:
            term->fb_c64_gfx[idx] ^= 1;
            if (term->fb_c64_gfx_color) {
                term->fb_c64_gfx_color[idx] = term->fb_c64_gfx[idx] ? color : 0;
            }
            break;
    }
    term->fb_c64_dirty = true;
}

void c64_fb_draw_line(plato_terminal_t *term, int x0, int y0, int x1, int y1, plato_screen_mode_t mode) {
    if (!term || !term->fb_c64_gfx) return;
    int vx0 = (x0 * C64_HIRES_W) / 512;
    int vy0 = (y0 * C64_HIRES_H) / 512;
    int vx1 = (x1 * C64_HIRES_W) / 512;
    int vy1 = (y1 * C64_HIRES_H) / 512;

    if (vx0 < 0) vx0 = 0; else if (vx0 >= C64_HIRES_W) vx0 = C64_HIRES_W - 1;
    if (vy0 < 0) vy0 = 0; else if (vy0 >= C64_HIRES_H) vy0 = C64_HIRES_H - 1;
    if (vx1 < 0) vx1 = 0; else if (vx1 >= C64_HIRES_W) vx1 = C64_HIRES_W - 1;
    if (vy1 < 0) vy1 = 0; else if (vy1 >= C64_HIRES_H) vy1 = C64_HIRES_H - 1;

    int dx = abs(vx1 - vx0);
    int dy = abs(vy1 - vy0);
    int sx = (vx0 < vx1) ? 1 : -1;
    int sy = (vy0 < vy1) ? 1 : -1;
    int err = dx - dy;

    int x = vx0;
    int y = vy0;
    uint32_t color = term->fb.fg_color ? term->fb.fg_color : 0xFFFFFFFFu;

    for (;;) {
        int idx = y * C64_HIRES_W + x;
        switch (mode) {
            case PLATO_SCREEN_WRITE:
            case PLATO_SCREEN_REWRITE:
                term->fb_c64_gfx[idx] = 1;
                if (term->fb_c64_gfx_color) term->fb_c64_gfx_color[idx] = color;
                break;
            case PLATO_SCREEN_ERASE:
                term->fb_c64_gfx[idx] = 0;
                if (term->fb_c64_gfx_color) term->fb_c64_gfx_color[idx] = 0;
                break;
            case PLATO_SCREEN_INVERSE:
                term->fb_c64_gfx[idx] ^= 1;
                if (term->fb_c64_gfx_color) {
                    term->fb_c64_gfx_color[idx] = term->fb_c64_gfx[idx] ? color : 0;
                }
                break;
        }

        if (x == vx1 && y == vy1) break;
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
    term->fb_c64_dirty = true;
}

void c64_fb_draw_block(plato_terminal_t *term, int x0, int y0, int x1, int y1, plato_screen_mode_t mode) {
    if (!term) return;
    int min_x = (x0 < x1) ? x0 : x1;
    int max_x = (x0 > x1) ? x0 : x1;
    int min_y = (y0 < y1) ? y0 : y1;
    int max_y = (y0 > y1) ? y0 : y1;

    if (min_x < 0) min_x = 0; if (max_x > 511) max_x = 511;
    if (min_y < 0) min_y = 0; if (max_y > 511) max_y = 511;

    int min_vx = (min_x * C64_HIRES_W) / 512;
    int max_vx = (max_x * C64_HIRES_W) / 512;
    int min_vy = (min_y * C64_HIRES_H) / 512;
    int max_vy = (max_y * C64_HIRES_H) / 512;

    if (min_vx < 0) min_vx = 0; if (max_vx >= C64_HIRES_W) max_vx = C64_HIRES_W - 1;
    if (min_vy < 0) min_vy = 0; if (max_vy >= C64_HIRES_H) max_vy = C64_HIRES_H - 1;

    uint32_t color = term->fb.fg_color ? term->fb.fg_color : 0xFFFFFFFFu;

    if (mode == PLATO_SCREEN_ERASE || mode == PLATO_SCREEN_INVERSE || mode == PLATO_SCREEN_REWRITE) {
        if (term->fb_c64_gfx) {
            for (int vy = min_vy; vy <= max_vy; vy++) {
                for (int vx = min_vx; vx <= max_vx; vx++) {
                    int idx = vy * C64_HIRES_W + vx;
                    term->fb_c64_gfx[idx] = 0;
                    if (term->fb_c64_gfx_color) term->fb_c64_gfx_color[idx] = 0;
                }
            }
        }
        if (term->fb_c64_text) {
            for (int ty = min_y; ty <= max_y; ty++) {
                for (int tx = min_x; tx <= max_x; tx++) {
                    int idx = ty * 512 + tx;
                    term->fb_c64_text[idx] = 0;
                    if (term->fb_c64_text_color) term->fb_c64_text_color[idx] = 0;
                }
            }
        }
    } else {
        if (term->fb_c64_gfx) {
            for (int vy = min_vy; vy <= max_vy; vy++) {
                for (int vx = min_vx; vx <= max_vx; vx++) {
                    int idx = vy * C64_HIRES_W + vx;
                    term->fb_c64_gfx[idx] = 1;
                    if (term->fb_c64_gfx_color) term->fb_c64_gfx_color[idx] = color;
                }
            }
        }
        if (term->fb_c64_text) {
            for (int ty = min_y; ty <= max_y; ty++) {
                for (int tx = min_x; tx <= max_x; tx++) {
                    int idx = ty * 512 + tx;
                    term->fb_c64_text[idx] = 0;
                    if (term->fb_c64_text_color) term->fb_c64_text_color[idx] = 0;
                }
            }
        }
    }
    term->fb_c64_dirty = true;
}

void c64_fb_draw_paint(plato_terminal_t *term, int start_x, int start_y, plato_screen_mode_t mode) {
    if (!term || !term->fb_c64_gfx || start_x < 0 || start_x >= 512 || start_y < 0 || start_y >= 512) return;
    int seed_vx = (start_x * C64_HIRES_W) / 512;
    int seed_vy = (start_y * C64_HIRES_H) / 512;
    if (seed_vx < 0) seed_vx = 0; else if (seed_vx >= C64_HIRES_W) seed_vx = C64_HIRES_W - 1;
    if (seed_vy < 0) seed_vy = 0; else if (seed_vy >= C64_HIRES_H) seed_vy = C64_HIRES_H - 1;

    int total_pixels = C64_HIRES_W * C64_HIRES_H;
    int seed_idx = seed_vy * C64_HIRES_W + seed_vx;
    if (term->fb_c64_gfx[seed_idx]) return;

    typedef struct { int16_t x, y; } c64_pt_t;
    c64_pt_t *q = (c64_pt_t *)malloc(sizeof(c64_pt_t) * total_pixels);
    if (!q) return;

    uint8_t *visited = (uint8_t *)calloc(total_pixels / 8 + 1, 1);
    if (!visited) { free(q); return; }

    uint32_t color = term->fb.fg_color ? term->fb.fg_color : 0xFFFFFFFFu;
    size_t head = 0, tail = 0;
    q[tail++] = (c64_pt_t){(int16_t)seed_vx, (int16_t)seed_vy};
    visited[seed_idx / 8] |= (uint8_t)(1 << (seed_idx % 8));

    while (head < tail) {
        c64_pt_t p = q[head++];
        int idx = p.y * C64_HIRES_W + p.x;
        term->fb_c64_gfx[idx] = 1;
        if (term->fb_c64_gfx_color) term->fb_c64_gfx_color[idx] = color;

        const int dxs[4] = {0, 0, -1, 1};
        const int dys[4] = {-1, 1, 0, 0};
        for (int d = 0; d < 4; ++d) {
            int nx = p.x + dxs[d];
            int ny = p.y + dys[d];
            if (nx >= 0 && nx < C64_HIRES_W && ny >= 0 && ny < C64_HIRES_H) {
                int nidx = ny * C64_HIRES_W + nx;
                if (!(visited[nidx / 8] & (1 << (nidx % 8))) && !term->fb_c64_gfx[nidx]) {
                    visited[nidx / 8] |= (uint8_t)(1 << (nidx % 8));
                    q[tail++] = (c64_pt_t){(int16_t)nx, (int16_t)ny};
                }
            }
        }
    }
    free(visited);
    free(q);
    term->fb_c64_dirty = true;
}

void c64_fb_draw_char(plato_terminal_t *term, const plato_font_t *font, plato_charset_t charset, uint8_t ch, int x, int y, plato_screen_mode_t mode, int size) {
    if (!term || !term->fb_c64_text) return;
    int scale = (size <= 1) ? 1 : size;
    int y_base = y + ((scale > 1) ? (scale - 1) * 5 : 0);

    const uint8_t *glyph = NULL;
    if (charset == PLATO_CHARSET_M0) {
        glyph = plato_c64_get_ascii_glyph_8x16(ch);
    } else {
        glyph = plato_font_get_glyph(font, charset, ch);
    }
    if (!glyph) return;

    uint32_t color = term->fb.fg_color ? term->fb.fg_color : 0xFFFFFFFFu;

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
                    if (target_x < 0 || target_x >= 512 || target_y < 0 || target_y >= 512) continue;

                    int idx = target_y * 512 + target_x;
                    switch (mode) {
                        case PLATO_SCREEN_WRITE:
                            if (bit_on) {
                                term->fb_c64_text[idx] = 1;
                                if (term->fb_c64_text_color) term->fb_c64_text_color[idx] = color;
                            }
                            break;
                        case PLATO_SCREEN_ERASE:
                            if (bit_on) {
                                term->fb_c64_text[idx] = 0;
                                if (term->fb_c64_text_color) term->fb_c64_text_color[idx] = 0;
                            }
                            break;
                        case PLATO_SCREEN_REWRITE:
                            term->fb_c64_text[idx] = bit_on ? 1 : 0;
                            if (term->fb_c64_text_color) {
                                term->fb_c64_text_color[idx] = bit_on ? color : 0;
                            }
                            break;
                        case PLATO_SCREEN_INVERSE:
                            term->fb_c64_text[idx] = bit_on ? 0 : 1;
                            if (term->fb_c64_text_color) {
                                term->fb_c64_text_color[idx] = bit_on ? 0 : color;
                            }
                            break;
                    }
                }
            }
        }
    }
    term->fb_c64_dirty = true;
}

