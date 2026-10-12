/*
 * PlatoLives - Native About Dialog Window (SDL2)
 * High-fidelity Plasma palette, vector typography and credentials.
 */

#include "linux_about.h"
#include "plato/plato_version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plato/stb_truetype.h"
#include "fonts/roboto_font_data.h"

#define ABOUT_WIN_W 520
#define ABOUT_WIN_H 380

static SDL_Window *g_ab_win = NULL;
static SDL_Renderer *g_ab_ren = NULL;

static stbtt_bakedchar g_ab_cdata[96];
static unsigned char *g_ab_atlas = NULL;
static SDL_Texture *g_ab_atlas_tex = NULL;
static bool g_ab_font_ready = false;

static bool g_ab_flashing = false;
static uint32_t g_ab_flash_until = 0;

static void init_ab_font(void) {
    if (g_ab_font_ready || !g_ab_ren) return;
    if (!g_ab_atlas) {
        g_ab_atlas = (unsigned char *)malloc(512 * 512);
    }
    stbtt_BakeFontBitmap(g_roboto_ttf, 0, 16.5f, g_ab_atlas, 512, 512, 32, 96, g_ab_cdata);

    uint32_t *rgba = (uint32_t *)malloc(512 * 512 * sizeof(uint32_t));
    if (!rgba) return;

    for (int i = 0; i < 512 * 512; i++) {
        uint8_t a = g_ab_atlas[i];
        rgba[i] = ((uint32_t)a << 24) | 0x00FFFFFF;
    }

    g_ab_atlas_tex = SDL_CreateTexture(g_ab_ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 512, 512);
    SDL_UpdateTexture(g_ab_atlas_tex, NULL, rgba, 512 * sizeof(uint32_t));
    SDL_SetTextureBlendMode(g_ab_atlas_tex, SDL_BLENDMODE_BLEND);

    free(rgba);
    g_ab_font_ready = true;
}

static float get_char_width_f(char c) {
    if ((unsigned char)c < 32 || (unsigned char)c >= 128) return 6.0f;
    return g_ab_cdata[(unsigned char)c - 32].xadvance;
}

static float get_string_width_f(const char *str, int count) {
    if (!str) return 0.0f;
    float w = 0.0f;
    for (int i = 0; i < count && str[i]; i++) {
        w += get_char_width_f(str[i]);
    }
    return w;
}

static void draw_ab_text(SDL_Renderer *ren, float x, float y, const char *str, uint8_t r, uint8_t g, uint8_t b) {
    if (!g_ab_atlas_tex || !str) return;
    SDL_SetTextureColorMod(g_ab_atlas_tex, r, g, b);

    while (*str) {
        char c = *str++;
        if ((unsigned char)c < 32 || (unsigned char)c >= 128) continue;
        const stbtt_bakedchar *bc = &g_ab_cdata[(unsigned char)c - 32];

        SDL_Rect src = { bc->x0, bc->y0, bc->x1 - bc->x0, bc->y1 - bc->y0 };
        SDL_Rect dst = { (int)(x + bc->xoff + 0.5f), (int)(y + bc->yoff + 0.5f), src.w, src.h };

        SDL_RenderCopy(ren, g_ab_atlas_tex, &src, &dst);
        x += bc->xadvance;
    }
}

static void draw_ab_rect(SDL_Renderer *ren, int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, bool outline, uint8_t or, uint8_t og, uint8_t ob) {
    SDL_Rect rc = { x, y, w, h };
    SDL_SetRenderDrawColor(ren, r, g, b, 255);
    SDL_RenderFillRect(ren, &rc);
    if (outline) {
        SDL_SetRenderDrawColor(ren, or, og, ob, 255);
        SDL_RenderDrawRect(ren, &rc);
    }
}

void linux_about_show(void) {
    if (g_ab_win) {
        SDL_RaiseWindow(g_ab_win);
        return;
    }

    g_ab_win = SDL_CreateWindow(
        "About PlatoLives",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        ABOUT_WIN_W, ABOUT_WIN_H,
        SDL_WINDOW_SHOWN
    );
    if (!g_ab_win) return;

    g_ab_ren = SDL_CreateRenderer(g_ab_win, -1, SDL_RENDERER_SOFTWARE);
    if (!g_ab_ren) {
        g_ab_ren = SDL_CreateRenderer(g_ab_win, -1, 0);
    }

    g_ab_font_ready = false;
    init_ab_font();
    g_ab_flashing = false;
}

bool linux_about_is_open(void) {
    return (g_ab_win != NULL);
}

void linux_about_destroy(void) {
    if (g_ab_atlas_tex) {
        SDL_DestroyTexture(g_ab_atlas_tex);
        g_ab_atlas_tex = NULL;
    }
    if (g_ab_atlas) {
        free(g_ab_atlas);
        g_ab_atlas = NULL;
    }
    if (g_ab_ren) {
        SDL_DestroyRenderer(g_ab_ren);
        g_ab_ren = NULL;
    }
    if (g_ab_win) {
        SDL_DestroyWindow(g_ab_win);
        g_ab_win = NULL;
    }
    g_ab_font_ready = false;
}

void linux_about_handle_event(const SDL_Event *ev) {
    if (!g_ab_win) return;
    uint32_t win_id = SDL_GetWindowID(g_ab_win);

    if (ev->type == SDL_WINDOWEVENT && ev->window.windowID == win_id) {
        if (ev->window.event == SDL_WINDOWEVENT_CLOSE) {
            linux_about_destroy();
            return;
        }
    } else if (ev->type == SDL_KEYDOWN && ev->key.windowID == win_id) {
        if (ev->key.keysym.sym == SDLK_ESCAPE || ev->key.keysym.sym == SDLK_RETURN) {
            linux_about_destroy();
            return;
        }
    } else if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.windowID == win_id) {
        int mx = ev->button.x, my = ev->button.y;
        /* Click pulsante Close (X=210, Y=330, W=100, H=32) */
        if (mx >= 210 && mx <= 310 && my >= 330 && my <= 362) {
            g_ab_flashing = true;
            g_ab_flash_until = SDL_GetTicks() + 250;
            return;
        }
    } else if (ev->type == SDL_MOUSEBUTTONUP && ev->button.windowID == win_id) {
        if (g_ab_flashing) {
            linux_about_destroy();
            return;
        }
    }
}

void linux_about_render(void) {
    if (!g_ab_win || !g_ab_ren) return;

    /* Sfondo Principale Dark Amber */
    SDL_SetRenderDrawColor(g_ab_ren, 16, 11, 7, 255);
    SDL_RenderClear(g_ab_ren);

    /* Bordo Esterno Finestra Radiante */
    draw_ab_rect(g_ab_ren, 0, 0, ABOUT_WIN_W, ABOUT_WIN_H, 16, 11, 7, true, 190, 85, 12);

    /* Testata Top Bar a 44px */
    draw_ab_rect(g_ab_ren, 0, 0, ABOUT_WIN_W, 44, 36, 18, 6, true, 190, 85, 12);
    draw_ab_text(g_ab_ren, 20.0f, 27.0f, "ABOUT PLATOLIVES", 255, 215, 60);

    /* Titolo Grande */
    draw_ab_text(g_ab_ren, 30.0f, 78.0f, "PlatoLives v" PLATO_VERSION_STRING, 255, 220, 65);
    draw_ab_text(g_ab_ren, 30.0f, 102.0f, "CDC PLATO IV & CYBER1 TERMINAL EMULATOR", 255, 160, 40);

    /* Riquadro Informativo */
    draw_ab_rect(g_ab_ren, 25, 120, ABOUT_WIN_W - 50, 185, 24, 14, 8, true, 96, 42, 10);

    draw_ab_text(g_ab_ren, 38.0f, 145.0f, "Created by Fabio Montarsolo  (fabio.montarsolo@gmail.com)", 255, 225, 120);
    draw_ab_text(g_ab_ren, 38.0f, 175.0f, "A modern, high-fidelity universal terminal emulator for", 230, 160, 45);
    draw_ab_text(g_ab_ren, 38.0f, 197.0f, "CDC PLATO IV, CDC IST-II and IST-III Cyber1 mainframe systems.", 230, 160, 45);
    draw_ab_text(g_ab_ren, 38.0f, 230.0f, "Featuring true vector typography, 5 optical shader modes,", 210, 145, 40);
    draw_ab_text(g_ab_ren, 38.0f, 252.0f, "phosphor persistence, Z80 MicroTutor and dynamic scripting.", 210, 145, 40);

    draw_ab_text(g_ab_ren, 38.0f, 286.0f, "(C) 2026 Fabio Montarsolo - All Rights Reserved.", 160, 105, 35);

    /* Pulsante Close */
    bool fl = (g_ab_flashing && SDL_GetTicks() < g_ab_flash_until);
    uint8_t bg_r = fl ? 255 : 36;
    uint8_t bg_g = fl ? 220 : 18;
    uint8_t bg_b = fl ? 0   : 8;
    uint8_t bd_r = fl ? 255 : 120;
    uint8_t bd_g = fl ? 245 : 60;
    uint8_t bd_b = fl ? 120 : 20;
    draw_ab_rect(g_ab_ren, 210, 325, 100, 32, bg_r, bg_g, bg_b, true, bd_r, bd_g, bd_b);

    float tw = get_string_width_f("Close", 5);
    float tx = 210.0f + (100.0f - tw) / 2.0f;
    uint8_t fg_r = fl ? 20 : 255;
    uint8_t fg_g = fl ? 12 : 215;
    uint8_t fg_b = fl ? 4  : 60;
    draw_ab_text(g_ab_ren, tx, 346.0f, "Close", fg_r, fg_g, fg_b);

    SDL_RenderPresent(g_ab_ren);
}
