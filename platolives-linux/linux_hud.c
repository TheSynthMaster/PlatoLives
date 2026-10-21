/*
 * PlatoLives - Dedicated Performance HUD Window (SDL2)
 * Decoupled telemetry monitor preventing Heisenberg observer overhead.
 */

#include "linux_hud.h"
#include "linux_theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plato/stb_truetype.h"
#include "fonts/roboto_font_data.h"

#define HUD_WIN_W 380
#define HUD_WIN_H 210

static SDL_Window *g_hud_win = NULL;
static SDL_Renderer *g_hud_ren = NULL;

static stbtt_bakedchar g_hud_cdata[96];
static unsigned char *g_hud_atlas = NULL;
static SDL_Texture *g_hud_atlas_tex = NULL;
static bool g_hud_font_ready = false;

static linux_hud_telemetry_t g_hud_data = {0};
static bool g_hud_needs_redraw = true;

static void init_hud_font(void) {
    if (g_hud_font_ready || !g_hud_ren) return;
    if (!g_hud_atlas) {
        g_hud_atlas = (unsigned char *)malloc(512 * 512);
    }
    stbtt_BakeFontBitmap(g_roboto_ttf, 0, 16.0f, g_hud_atlas, 512, 512, 32, 96, g_hud_cdata);

    uint32_t *rgba = (uint32_t *)malloc(512 * 512 * sizeof(uint32_t));
    if (!rgba) return;

    for (int i = 0; i < 512 * 512; i++) {
        uint8_t a = g_hud_atlas[i];
        rgba[i] = ((uint32_t)a << 24) | 0x00FFFFFF;
    }

    g_hud_atlas_tex = SDL_CreateTexture(g_hud_ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 512, 512);
    SDL_UpdateTexture(g_hud_atlas_tex, NULL, rgba, 512 * sizeof(uint32_t));
    SDL_SetTextureBlendMode(g_hud_atlas_tex, SDL_BLENDMODE_BLEND);

    free(rgba);
    g_hud_font_ready = true;
}

static void draw_hud_text(SDL_Renderer *ren, float x, float y, const char *str, uint8_t r, uint8_t g, uint8_t b) {
    if (!g_hud_atlas_tex || !str) return;
    SDL_SetTextureColorMod(g_hud_atlas_tex, r, g, b);

    while (*str) {
        char c = *str++;
        if ((unsigned char)c < 32 || (unsigned char)c >= 128) continue;
        const stbtt_bakedchar *bc = &g_hud_cdata[(unsigned char)c - 32];

        SDL_Rect src = { bc->x0, bc->y0, bc->x1 - bc->x0, bc->y1 - bc->y0 };
        SDL_Rect dst = { (int)(x + bc->xoff + 0.5f), (int)(y + bc->yoff + 0.5f), src.w, src.h };

        SDL_RenderCopy(ren, g_hud_atlas_tex, &src, &dst);
        x += bc->xadvance;
    }
}

static void draw_hud_box(SDL_Renderer *ren, int x, int y, int w, int h,
                         uint8_t bg_r, uint8_t bg_g, uint8_t bg_b,
                         uint8_t border_r, uint8_t border_g, uint8_t border_b) {
    SDL_Rect rc = { x, y, w, h };
    SDL_SetRenderDrawColor(ren, bg_r, bg_g, bg_b, 255);
    SDL_RenderFillRect(ren, &rc);
    SDL_SetRenderDrawColor(ren, border_r, border_g, border_b, 255);
    SDL_RenderDrawRect(ren, &rc);
}

void linux_hud_toggle(void) {
    if (g_hud_win) {
        linux_hud_destroy();
    } else {
        g_hud_win = SDL_CreateWindow(
            "PLATO Telemetry HUD",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            HUD_WIN_W, HUD_WIN_H,
            SDL_WINDOW_SHOWN
        );
        if (!g_hud_win) return;

        g_hud_ren = SDL_CreateRenderer(g_hud_win, -1, SDL_RENDERER_SOFTWARE);
        if (!g_hud_ren) {
            g_hud_ren = SDL_CreateRenderer(g_hud_win, -1, 0);
        }
        if (!g_hud_ren) {
            SDL_DestroyWindow(g_hud_win);
            g_hud_win = NULL;
            return;
        }
        g_hud_font_ready = false;
        init_hud_font();
        SDL_RaiseWindow(g_hud_win);
        g_hud_needs_redraw = true;
    }
}

bool linux_hud_is_open(void) {
    return (g_hud_win != NULL);
}

void linux_hud_update_data(const linux_hud_telemetry_t *telem) {
    if (!telem) return;
    g_hud_data = *telem;
    g_hud_needs_redraw = true;
}

void linux_hud_update_queue(int tek_queue_count, int display_mode, int tek_baud) {
    g_hud_data.tek_queue_count = tek_queue_count;
    g_hud_data.display_mode = display_mode;
    g_hud_data.tek_baud = tek_baud;
}

void linux_hud_handle_event(const SDL_Event *ev) {
    if (!ev || !g_hud_win) return;
    if (ev->type == SDL_WINDOWEVENT && ev->window.windowID == SDL_GetWindowID(g_hud_win)) {
        if (ev->window.event == SDL_WINDOWEVENT_CLOSE) {
            linux_hud_destroy();
        } else if (ev->window.event == SDL_WINDOWEVENT_EXPOSED) {
            g_hud_needs_redraw = true;
        }
    }
}

static const char *get_display_mode_name(int mode) {
    switch (mode) {
        case 0: return "Real Plasma";
        case 1: return "Crisp Monochrome";
        case 2: return "Split Monochrome";
        case 3: return "Crisp Color";
        case 4: return "Real Color CRT";
        case 5: return "Real Tektronix 4014";
        default: return "Unknown";
    }
}

void linux_hud_render(void) {
    if (!g_hud_win || !g_hud_ren) return;

    linux_theme_colors_t th = linux_theme_current();

    SDL_SetRenderDrawColor(g_hud_ren, th.bg_dark.r, th.bg_dark.g, th.bg_dark.b, 255);
    SDL_RenderClear(g_hud_ren);

    /* Riquadro Unico Compatto */
    draw_hud_box(g_hud_ren, 8, 8, HUD_WIN_W - 16, HUD_WIN_H - 16,
                 th.bg_box.r, th.bg_box.g, th.bg_box.b,
                 th.border.r, th.border.g, th.border.b);

    draw_hud_text(g_hud_ren, 20.0f, 26.0f, "PLATO TELEMETRY MONITOR", th.bright.r, th.bright.g, th.bright.b);

    /* Linea di separazione */
    SDL_SetRenderDrawColor(g_hud_ren, th.border.r, th.border.g, th.border.b, 255);
    SDL_Rect sep_rc = { 16, 36, HUD_WIN_W - 32, 1 }; SDL_RenderFillRect(g_hud_ren, &sep_rc);

    char label[32], val[128];
    float y = 58.0f;
    float dy = 24.0f;

    /* Riga 1: USER / SLOT */
    snprintf(label, sizeof(label), "USER    ");
    snprintf(val, sizeof(val), "%s", (g_hud_data.user_info[0] ? g_hud_data.user_info : "STANDALONE"));
    draw_hud_text(g_hud_ren, 22.0f, y, label, th.dim.r, th.dim.g, th.dim.b);
    draw_hud_text(g_hud_ren, 90.0f, y, val, th.bright.r, th.bright.g, th.bright.b);
    y += dy;

    /* Riga 2: FPS */
    snprintf(label, sizeof(label), "FPS     ");
    snprintf(val, sizeof(val), "%5.1f", g_hud_data.fps_total);
    draw_hud_text(g_hud_ren, 22.0f, y, label, th.dim.r, th.dim.g, th.dim.b);
    draw_hud_text(g_hud_ren, 90.0f, y, val, th.normal.r, th.normal.g, th.normal.b);
    y += dy;

    int mode = g_hud_data.display_mode;
    if (mode == 5) {
        /* Tektronix 4014: SPEED, QUEUE, CPU */
        snprintf(label, sizeof(label), "SPEED   ");
        if (g_hud_data.tek_baud > 0) snprintf(val, sizeof(val), "%d BAUD", g_hud_data.tek_baud);
        else snprintf(val, sizeof(val), "INSTANT (UNCAPPED)");
        draw_hud_text(g_hud_ren, 22.0f, y, label, th.dim.r, th.dim.g, th.dim.b);
        draw_hud_text(g_hud_ren, 90.0f, y, val, th.bright.r, th.bright.g, th.bright.b);
        y += dy;

        snprintf(label, sizeof(label), "QUEUE   ");
        int pct = (int)(((double)g_hud_data.tek_queue_count * 100.0 / 8192.0) + 0.5);
        if (g_hud_data.tek_queue_count > 0 && pct == 0) pct = 1;
        snprintf(val, sizeof(val), "%2d%% (%d/8192)", pct, g_hud_data.tek_queue_count);
        draw_hud_text(g_hud_ren, 22.0f, y, label, th.dim.r, th.dim.g, th.dim.b);
        draw_hud_text(g_hud_ren, 90.0f, y, val, th.normal.r, th.normal.g, th.normal.b);
        y += dy;

        snprintf(label, sizeof(label), "CPU     ");
        snprintf(val, sizeof(val), "%5.1f%%", g_hud_data.cpu_percent);
        draw_hud_text(g_hud_ren, 22.0f, y, label, th.dim.r, th.dim.g, th.dim.b);
        draw_hud_text(g_hud_ren, 90.0f, y, val, th.normal.r, th.normal.g, th.normal.b);
        y += dy;
    } else if (mode == 1 || mode == 3) {
        /* Crisp Mono o Crisp Color: CPU */
        snprintf(label, sizeof(label), "CPU     ");
        snprintf(val, sizeof(val), "%5.1f%%", g_hud_data.cpu_percent);
        draw_hud_text(g_hud_ren, 22.0f, y, label, th.dim.r, th.dim.g, th.dim.b);
        draw_hud_text(g_hud_ren, 90.0f, y, val, th.normal.r, th.normal.g, th.normal.b);
        y += dy;
    } else {
        /* Plasma (0, 2) o CRT (4): PARTIAL, FULL, CPU */
        snprintf(label, sizeof(label), "PARTIAL ");
        snprintf(val, sizeof(val), "%5.1f", g_hud_data.fps_partial);
        draw_hud_text(g_hud_ren, 22.0f, y, label, th.dim.r, th.dim.g, th.dim.b);
        draw_hud_text(g_hud_ren, 90.0f, y, val, th.normal.r, th.normal.g, th.normal.b);
        y += dy;

        snprintf(label, sizeof(label), "FULL    ");
        snprintf(val, sizeof(val), "%5.1f", g_hud_data.fps_full);
        draw_hud_text(g_hud_ren, 22.0f, y, label, th.dim.r, th.dim.g, th.dim.b);
        draw_hud_text(g_hud_ren, 90.0f, y, val, th.normal.r, th.normal.g, th.normal.b);
        y += dy;

        snprintf(label, sizeof(label), "CPU     ");
        snprintf(val, sizeof(val), "%5.1f%%", g_hud_data.cpu_percent);
        draw_hud_text(g_hud_ren, 22.0f, y, label, th.dim.r, th.dim.g, th.dim.b);
        draw_hud_text(g_hud_ren, 90.0f, y, val, th.normal.r, th.normal.g, th.normal.b);
        y += dy;
    }

    /* Riga inferiore: modalità attiva */
    char mode_str[64];
    snprintf(mode_str, sizeof(mode_str), "MODE: %s", get_display_mode_name(mode));
    draw_hud_text(g_hud_ren, 22.0f, (float)(HUD_WIN_H - 24), mode_str, th.muted.r, th.muted.g, th.muted.b);

    SDL_RenderPresent(g_hud_ren);
}

void linux_hud_destroy(void) {
    if (g_hud_atlas_tex) {
        SDL_DestroyTexture(g_hud_atlas_tex);
        g_hud_atlas_tex = NULL;
    }
    if (g_hud_atlas) {
        free(g_hud_atlas);
        g_hud_atlas = NULL;
    }
    g_hud_font_ready = false;
    if (g_hud_ren) {
        SDL_DestroyRenderer(g_hud_ren);
        g_hud_ren = NULL;
    }
    if (g_hud_win) {
        SDL_DestroyWindow(g_hud_win);
        g_hud_win = NULL;
    }
}
