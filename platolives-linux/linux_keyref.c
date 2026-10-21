/*
 * PlatoLives - Independent Keyboard Reference Window (SDL2)
 * Full PLATO shortcut documentation with native scrolling & Plasma Top-Bar Look.
 */

#include "linux_keyref.h"
#include "linux_theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plato/stb_truetype.h"
#include "fonts/roboto_font_data.h"

static SDL_Window *g_kr_win = NULL;
static SDL_Renderer *g_kr_ren = NULL;
static int g_kr_scroll_y = 0;
static bool g_kr_dragging = false;
static int g_kr_drag_start_y = 0;
static int g_kr_drag_start_scroll = 0;
static stbtt_bakedchar g_kr_cdata[96];
static unsigned char *g_kr_atlas = NULL;
static SDL_Texture *g_kr_atlas_tex = NULL;
static bool g_kr_font_ready = false;

typedef struct {
    const char *key;
    const char *shortcut;
    bool is_header;
} keyref_entry_t;

static const keyref_entry_t g_kr_table[] = {
    {"NEXT", "Return / Enter / Ctrl+N", false},
    {"SHIFT-NEXT", "Shift+Return / Shift+Ctrl+N", false},
    {"BACK", "Esc / Backspace / F8 / Ctrl+B", false},
    {"SHIFT-BACK", "Shift+Esc / Shift+F8 / Shift+Ctrl+B", false},
    {"STOP", "F10 / F4 / Ctrl+S / Alt+S", false},
    {"SHIFT-STOP", "Shift + any STOP shortcut", false},
    {"ERASE", "Backspace / Delete / Left Arrow / Ctrl+R", false},
    {"SHIFT-ERASE", "Shift + any ERASE shortcut", false},
    {"HELP", "F1 / F6 / Ctrl+H / Alt+H", false},
    {"SHIFT-HELP", "Shift + any HELP shortcut", false},
    {"LAB", "F2 / F7 / Ctrl+L / Alt+L", false},
    {"SHIFT-LAB", "Shift + any LAB shortcut", false},
    {"DATA", "F3 / F9 / Ctrl+D / Alt+D", false},
    {"SHIFT-DATA", "Shift + any DATA shortcut", false},
    {"EDIT", "F5 / Ctrl+E / Alt+E", false},
    {"SHIFT-EDIT", "Shift+F5 / Shift+Ctrl+E", false},
    {"COPY", "Ctrl+C / Alt+C", false},
    {"SHIFT-COPY", "Shift+Ctrl+C / Shift+Alt+C", false},
    {"MICRO", "Ctrl+M / Alt+M", false},
    {"FONT", "Ctrl+F / Alt+F", false},
    {"SUPER", "Up Arrow / Page Up / Ctrl+P", false},
    {"SHIFT-SUPER", "Shift + any SUPER shortcut (Ascend)", false},
    {"SUB", "Down Arrow / Page Down / Ctrl+Y", false},
    {"SHIFT-SUB", "Shift + any SUB shortcut (Descend)", false},
    {"ANS", "Ctrl+A / Ctrl+/ / Alt+A", false},
    {"TERM", "Ctrl+T / Alt+T / Shift+Ctrl+A", false},
    {"SQUARE", "Ctrl+Q / Alt+Q", false},
    {"ACCESS", "Shift+Ctrl+Q / Shift+Alt+Q", false},
    {"TAB", "Tab / Right Arrow", false},
    {"ASSIGN (<=)", "Alt + Left Arrow", false},
    {"MULTIPLY (*)", "Ctrl+X / Alt+X", false},
    {"DIVIDE (/)", "Ctrl+G / Alt+G", false},
    {"", "", false},
    {"APPLICATION SHORTCUTS", "", true},
    {"-------------------------", "-----------------------------------", false},
    {"Full Screen", "F11", false},
    {"Performance HUD", "F12", false},
    {"Keyboard Reference", "Ctrl+K", false},
    {"Copy Text (Verbatim)", "Ctrl+C", false},
    {"Copy Text (Compact)", "Ctrl+Shift+C", false},
    {"Paste Text", "Ctrl+V", false},
    {"Cancel Script", "Ctrl+Shift+X", false},
    {"Disconnect", "Ctrl+D", false},
    {"Close / Exit", "Ctrl+W / Ctrl+Q", false}
};

static void init_kr_font(void) {
    if (g_kr_font_ready || !g_kr_ren) return;

    if (!g_kr_atlas) {
        g_kr_atlas = (unsigned char *)malloc(512 * 512);
    }
    stbtt_BakeFontBitmap(g_roboto_ttf, 0, 17.5f, g_kr_atlas, 512, 512, 32, 96, g_kr_cdata);

    uint32_t *rgba = (uint32_t *)malloc(512 * 512 * sizeof(uint32_t));
    if (!rgba) return;

    for (int i = 0; i < 512 * 512; i++) {
        uint8_t a = g_kr_atlas[i];
        rgba[i] = ((uint32_t)a << 24) | 0x00FFFFFF;
    }

    g_kr_atlas_tex = SDL_CreateTexture(g_kr_ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 512, 512);
    SDL_UpdateTexture(g_kr_atlas_tex, NULL, rgba, 512 * sizeof(uint32_t));
    SDL_SetTextureBlendMode(g_kr_atlas_tex, SDL_BLENDMODE_BLEND);

    free(rgba);
    g_kr_font_ready = true;
}

void linux_keyref_toggle(void) {
    if (g_kr_win) {
        SDL_RaiseWindow(g_kr_win);
        return;
    }

    g_kr_win = SDL_CreateWindow(
        "PLATO Keyboard Reference",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        640, 560,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!g_kr_win) return;

    g_kr_ren = SDL_CreateRenderer(g_kr_win, -1, SDL_RENDERER_SOFTWARE);
    if (!g_kr_ren) {
        g_kr_ren = SDL_CreateRenderer(g_kr_win, -1, 0);
    }
    g_kr_scroll_y = 0;
    g_kr_font_ready = false;
    init_kr_font();
}

void linux_keyref_handle_event(const SDL_Event *ev) {
    if (!g_kr_win) return;
    uint32_t win_id = SDL_GetWindowID(g_kr_win);

    if (ev->type == SDL_WINDOWEVENT && ev->window.windowID == win_id) {
        if (ev->window.event == SDL_WINDOWEVENT_CLOSE) {
            linux_keyref_destroy();
            return;
        }
    } else if (ev->type == SDL_MOUSEWHEEL && ev->wheel.windowID == win_id) {
        g_kr_scroll_y -= ev->wheel.y * 32;
        if (g_kr_scroll_y < 0) g_kr_scroll_y = 0;
        int max_scroll = (int)(sizeof(g_kr_table)/sizeof(g_kr_table[0])) * 25 - 460;
        if (max_scroll < 0) max_scroll = 0;
        if (g_kr_scroll_y > max_scroll) g_kr_scroll_y = max_scroll;
    } else if (ev->type == SDL_MOUSEBUTTONUP && ev->window.windowID == win_id) {
        if (ev->button.button == SDL_BUTTON_LEFT) {
            g_kr_dragging = false;
        }
    } else if (ev->type == SDL_MOUSEMOTION && ev->window.windowID == win_id) {
        if (g_kr_dragging && (ev->motion.state & SDL_BUTTON_LMASK)) {
            int win_w, win_h;
            SDL_GetWindowSize(g_kr_win, &win_w, &win_h);
            int total_entries = (int)(sizeof(g_kr_table) / sizeof(g_kr_table[0]));
            int row_h = 25;
            int top_bar_h = 72;
            int content_h = total_entries * row_h;
            int view_h = win_h - top_bar_h;
            int max_scroll = content_h - view_h;
            if (max_scroll > 0) {
                int bar_h = (view_h * view_h) / content_h;
                if (bar_h < 30) bar_h = 30;
                int track_h = view_h - bar_h;
                if (track_h > 0) {
                    int dy = ev->motion.y - g_kr_drag_start_y;
                    int new_scroll = g_kr_drag_start_scroll + (dy * max_scroll) / track_h;
                    if (new_scroll < 0) new_scroll = 0;
                    if (new_scroll > max_scroll) new_scroll = max_scroll;
                    g_kr_scroll_y = new_scroll;
                }
            }
            return;
        }
    } else if (ev->type == SDL_MOUSEBUTTONDOWN && ev->window.windowID == win_id) {
        if (ev->button.button == SDL_BUTTON_LEFT) {
            int win_w, win_h;
            SDL_GetWindowSize(g_kr_win, &win_w, &win_h);
            int top_bar_h = 72;
            int mx = ev->button.x, my = ev->button.y;
            if (mx >= win_w - 20 && my >= top_bar_h) {
                int total_entries = (int)(sizeof(g_kr_table) / sizeof(g_kr_table[0]));
                int content_h = total_entries * 25;
                int view_h = win_h - top_bar_h;
                int max_scroll = content_h - view_h;
                if (max_scroll > 0) {
                    int bar_h = (view_h * view_h) / content_h;
                    if (bar_h < 30) bar_h = 30;
                    int track_h = view_h - bar_h;
                    if (track_h > 0) {
                        float ratio = (float)(my - top_bar_h - bar_h / 2) / (float)track_h;
                        if (ratio < 0.0f) ratio = 0.0f;
                        if (ratio > 1.0f) ratio = 1.0f;
                        g_kr_scroll_y = (int)(ratio * (float)max_scroll + 0.5f);
                        g_kr_dragging = true;
                        g_kr_drag_start_y = my;
                        g_kr_drag_start_scroll = g_kr_scroll_y;
                    }
                }
                return;
            }
        }
    } else if (ev->type == SDL_KEYDOWN && ev->key.windowID == win_id) {
        if (ev->key.keysym.sym == SDLK_ESCAPE) {
            linux_keyref_destroy();
        } else if (ev->key.keysym.sym == SDLK_UP) {
            g_kr_scroll_y -= 32;
            if (g_kr_scroll_y < 0) g_kr_scroll_y = 0;
        } else if (ev->key.keysym.sym == SDLK_DOWN) {
            g_kr_scroll_y += 32;
        } else if (ev->key.keysym.sym == SDLK_PAGEUP) {
            g_kr_scroll_y -= 220;
            if (g_kr_scroll_y < 0) g_kr_scroll_y = 0;
        } else if (ev->key.keysym.sym == SDLK_PAGEDOWN) {
            g_kr_scroll_y += 220;
        }
    }
}

static void draw_kr_string(SDL_Renderer *ren, float x, float y, const char *str, uint8_t r, uint8_t g, uint8_t b) {
    if (!g_kr_atlas_tex) return;
    SDL_SetTextureColorMod(g_kr_atlas_tex, r, g, b);
    while (*str) {
        char c = *str++;
        if ((unsigned char)c < 32 || (unsigned char)c >= 128) continue;
        const stbtt_bakedchar *bc = &g_kr_cdata[(unsigned char)c - 32];
        SDL_Rect src = { bc->x0, bc->y0, bc->x1 - bc->x0, bc->y1 - bc->y0 };
        SDL_Rect dst = { (int)(x + bc->xoff), (int)(y + bc->yoff), src.w, src.h };
        SDL_RenderCopy(ren, g_kr_atlas_tex, &src, &dst);
        x += bc->xadvance;
    }
}

void linux_keyref_render(void) {
    linux_theme_colors_t th = linux_theme_current();
    if (!g_kr_win || !g_kr_ren) return;

    int win_w, win_h;
    SDL_GetWindowSize(g_kr_win, &win_w, &win_h);

    /* Sfondo principale Dark Amber Plasma */
    SDL_SetRenderDrawColor(g_kr_ren, th.bg_dark.r, th.bg_dark.g, th.bg_dark.b, 255);
    SDL_RenderClear(g_kr_ren);

    int total_entries = (int)(sizeof(g_kr_table) / sizeof(g_kr_table[0]));
    int row_h = 25;
    int top_bar_h = 72; /* Spazio per la testata Plasma fissa */
    int start_y = top_bar_h + 8 - g_kr_scroll_y;

    /* Disegno delle righe della tabella (scorrevoli sotto la barra) */
    for (int i = 0; i < total_entries; i++) {
        int y = start_y + i * row_h;
        if (y < top_bar_h - 20 || y > win_h + 30) continue;

        const keyref_entry_t *e = &g_kr_table[i];
        if (e->is_header) {
            draw_kr_string(g_kr_ren, 36.0f, (float)(y + 15), e->key, th.bright.r, th.bright.g, th.bright.b);
        } else if (e->key[0] == '-') {
            draw_kr_string(g_kr_ren, 36.0f, (float)(y + 15), e->key, th.dim.r, th.dim.g, th.dim.b);
            draw_kr_string(g_kr_ren, 230.0f, (float)(y + 15), e->shortcut, th.dim.r, th.dim.g, th.dim.b);
        } else if (e->key[0] != '\0') {
            /* Tasto PLATO in Ambra Intenso e Scorciatoia in Ambra Radiante */
            draw_kr_string(g_kr_ren, 36.0f, (float)(y + 15), e->key, th.bright.r, th.bright.g, th.bright.b);
            draw_kr_string(g_kr_ren, 230.0f, (float)(y + 15), e->shortcut, th.normal.r, th.normal.g, th.normal.b);
        }
    }

    /* BARRA SUPERIORE FISSA IN LOOK PLASMA CANONICO */
    /* 1. Sfondo testata ambra plasma intenso */
    SDL_SetRenderDrawColor(g_kr_ren, th.bg_box.r, th.bg_box.g, th.bg_box.b, 255);
    SDL_Rect bar_bg = { 0, 0, win_w, top_bar_h };
    SDL_RenderFillRect(g_kr_ren, &bar_bg);

    /* 2. Bordo inferiore radiante plasma */
    SDL_SetRenderDrawColor(g_kr_ren, th.border.r, th.border.g, th.border.b, 255);
    SDL_Rect bar_border = { 0, top_bar_h - 2, win_w, 2 };
    SDL_RenderFillRect(g_kr_ren, &bar_border);

    /* 3. Titolo principale finestra in oro plasma */
    draw_kr_string(g_kr_ren, 36.0f, 26.0f, "PLATO KEYBOARD REFERENCE", th.bright.r, th.bright.g, th.bright.b);

    /* 4. Intestazioni delle colonne fisse */
    draw_kr_string(g_kr_ren, 36.0f, 56.0f, "PLATO KEY", th.dim.r, th.dim.g, th.dim.b);
    draw_kr_string(g_kr_ren, 230.0f, 56.0f, "WINDOWS / MAC SHORTCUT", th.dim.r, th.dim.g, th.dim.b);

    /* Scrollbar laterale ambra plasma */
    int content_h = total_entries * row_h;
    if (content_h > (win_h - top_bar_h)) {
        int view_h = win_h - top_bar_h;
        int bar_h = (view_h * view_h) / content_h;
        if (bar_h < 30) bar_h = 30;
        int bar_y = top_bar_h + (g_kr_scroll_y * (view_h - bar_h)) / (content_h - view_h);
        SDL_SetRenderDrawColor(g_kr_ren, th.border.r, th.border.g, th.border.b, 230);
        SDL_Rect bar_rect = { win_w - 7, bar_y, 5, bar_h };
        SDL_RenderFillRect(g_kr_ren, &bar_rect);
    }

    SDL_RenderPresent(g_kr_ren);
}

void linux_keyref_destroy(void) {
    if (g_kr_atlas_tex) {
        SDL_DestroyTexture(g_kr_atlas_tex);
        g_kr_atlas_tex = NULL;
    }
    g_kr_dragging = false;
    if (g_kr_atlas) {
        free(g_kr_atlas);
        g_kr_atlas = NULL;
    }
    if (g_kr_ren) {
        SDL_DestroyRenderer(g_kr_ren);
        g_kr_ren = NULL;
    }
    if (g_kr_win) {
        SDL_DestroyWindow(g_kr_win);
        g_kr_win = NULL;
    }
    g_kr_font_ready = false;
}
