/*
 * PlatoLives - Dedicated Performance HUD Window (SDL2)
 * Decoupled telemetry monitor preventing Heisenberg observer overhead.
 */

#include "linux_hud.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plato/stb_truetype.h"
#include "fonts/roboto_font_data.h"

#define HUD_WIN_W 420
#define HUD_WIN_H 290

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

static void draw_hud_box(SDL_Renderer *ren, int x, int y, int w, int h, uint8_t bg_r, uint8_t bg_g, uint8_t bg_b, uint8_t border_r, uint8_t border_g, uint8_t border_b) {
    SDL_Rect rc = { x, y, w, h };
    SDL_SetRenderDrawColor(ren, bg_r, bg_g, bg_b, 255);
    SDL_RenderFillRect(ren, &rc);

    SDL_SetRenderDrawColor(ren, border_r, border_g, border_b, 255);
    SDL_RenderDrawRect(ren, &rc);
}

void linux_hud_toggle(void) {
    if (g_hud_win) {
        linux_hud_destroy();
        return;
    }

    g_hud_win = SDL_CreateWindow(
        "PlatoLives - Performance Telemetry",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        HUD_WIN_W, HUD_WIN_H,
        SDL_WINDOW_SHOWN
    );
    if (!g_hud_win) return;

    g_hud_ren = SDL_CreateRenderer(g_hud_win, -1, SDL_RENDERER_SOFTWARE);
    if (!g_hud_ren) {
        g_hud_ren = SDL_CreateRenderer(g_hud_win, -1, 0);
    }
    g_hud_font_ready = false;
    init_hud_font();
    g_hud_needs_redraw = true;
}

bool linux_hud_is_open(void) {
    return (g_hud_win != NULL);
}

void linux_hud_update_data(const linux_hud_telemetry_t *telem) {
    if (!telem) return;
    g_hud_data = *telem;
    g_hud_needs_redraw = true;
}

void linux_hud_handle_event(const SDL_Event *ev) {
    if (!g_hud_win) return;
    uint32_t win_id = SDL_GetWindowID(g_hud_win);

    if (ev->type == SDL_WINDOWEVENT && ev->window.windowID == win_id) {
        if (ev->window.event == SDL_WINDOWEVENT_CLOSE) {
            linux_hud_destroy();
            return;
        }
        if (ev->window.event == SDL_WINDOWEVENT_EXPOSED) {
            g_hud_needs_redraw = true;
        }
    } else if (ev->type == SDL_KEYDOWN && ev->key.windowID == win_id) {
        if (ev->key.keysym.sym == SDLK_ESCAPE || ev->key.keysym.sym == SDLK_F12) {
            linux_hud_destroy();
            return;
        }
    }
}

static const char* get_display_mode_name(int mode) {
    switch (mode) {
        case 0: return "Real Plasma (Orange Glow)";
        case 1: return "Crisp Mono (Pure Vector)";
        case 2: return "Split Mono (Luminance)";
        case 3: return "Crisp Color (RGB Matrix)";
        case 4: return "Real CRT (Phosphor Decay)";
        default: return "Custom Pipeline";
    }
}

void linux_hud_render(void) {
    if (!g_hud_win || !g_hud_ren || !g_hud_needs_redraw) return;
    g_hud_needs_redraw = false;

    /* Sfondo principale Dark Charcoal Amber */
    SDL_SetRenderDrawColor(g_hud_ren, 12, 8, 5, 255);
    SDL_RenderClear(g_hud_ren);

    /* 1. Testata Superiore Plasma */
    draw_hud_box(g_hud_ren, 8, 8, HUD_WIN_W - 16, 36, 22, 12, 6, 95, 45, 12);
    draw_hud_text(g_hud_ren, 20.0f, 31.0f, "PLATOLIVES REAL-TIME TELEMETRY", 255, 190, 40);

    /* Indicatore stato connessione in alto a destra */
    if (g_hud_data.connected) {
        draw_hud_text(g_hud_ren, HUD_WIN_W - 130.0f, 31.0f, "[CONNECTED]", 80, 225, 80);
    } else {
        draw_hud_text(g_hud_ren, HUD_WIN_W - 145.0f, 31.0f, "[OFFLINE / IDLE]", 200, 100, 30);
    }

    /* 2. Riquadro Framerate & CPU */
    draw_hud_box(g_hud_ren, 8, 50, HUD_WIN_W - 16, 75, 16, 10, 6, 65, 30, 8);
    draw_hud_text(g_hud_ren, 18.0f, 68.0f, "RENDERING PERFORMANCE", 255, 150, 20);

    char buf[128];
    snprintf(buf, sizeof(buf), "FPS TOTAL:    %5.1f       FULL: %5.1f", g_hud_data.fps_total, g_hud_data.fps_full);
    draw_hud_text(g_hud_ren, 22.0f, 88.0f, buf, 240, 210, 160);

    snprintf(buf, sizeof(buf), "CPU USAGE:    %5.1f %%     PARTIAL: %5.1f", g_hud_data.cpu_percent, g_hud_data.fps_partial);
    draw_hud_text(g_hud_ren, 22.0f, 107.0f, buf, 240, 210, 160);

    /* 3. Riquadro Ottica & Display Mode */
    draw_hud_box(g_hud_ren, 8, 131, HUD_WIN_W - 16, 85, 16, 10, 6, 65, 30, 8);
    draw_hud_text(g_hud_ren, 18.0f, 149.0f, "DISPLAY & OPTICAL EMULATION", 255, 150, 20);

    snprintf(buf, sizeof(buf), "MODE:         %s", get_display_mode_name(g_hud_data.display_mode));
    draw_hud_text(g_hud_ren, 22.0f, 169.0f, buf, 240, 210, 160);

    snprintf(buf, sizeof(buf), "PERSISTENCE:  %d ms        DISTORTION: %d %%", g_hud_data.persistence_ms, g_hud_data.distortion);
    draw_hud_text(g_hud_ren, 22.0f, 189.0f, buf, 240, 210, 160);

    /* 4. Riquadro Sessione Cyber1 */
    draw_hud_box(g_hud_ren, 8, 222, HUD_WIN_W - 16, 58, 16, 10, 6, 65, 30, 8);
    draw_hud_text(g_hud_ren, 18.0f, 240.0f, "CYBER1 / PLATO IV SESSION", 255, 150, 20);

    snprintf(buf, sizeof(buf), "USER INFO:    %s", (g_hud_data.user_info[0] ? g_hud_data.user_info : "GUEST / NONE"));
    draw_hud_text(g_hud_ren, 22.0f, 260.0f, buf, 255, 180, 40);

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
