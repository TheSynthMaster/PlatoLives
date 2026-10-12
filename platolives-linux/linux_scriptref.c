/*
 * PlatoLives - Independent Scripting Reference Window (SDL2)
 * Full PLATO Scripting Language Documentation with native scrolling & Plasma Look.
 */

#include "linux_scriptref.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plato/plato_script.h"
#include "plato/stb_truetype.h"
#include "fonts/roboto_font_data.h"

#define SCRIPTREF_WIN_W 760
#define SCRIPTREF_WIN_H 580
#define SCRIPTREF_LINE_H 21
#define SCRIPTREF_TOP_BAR_H 72

static SDL_Window *g_sr_win = NULL;
static SDL_Renderer *g_sr_ren = NULL;
static int g_sr_scroll_y = 0;
static bool g_sr_dragging = false;
static int g_sr_drag_start_y = 0;
static int g_sr_drag_start_scroll = 0;

static stbtt_bakedchar g_sr_cdata[96];
static unsigned char *g_sr_atlas = NULL;
static SDL_Texture *g_sr_atlas_tex = NULL;
static bool g_sr_font_ready = false;

/* Buffer righe manuale */
static char **g_sr_lines = NULL;
static int g_sr_line_count = 0;

static void parse_manual_lines(void) {
    if (g_sr_lines) return;
    const char *raw = plato_script_get_manual_text();
    if (!raw) raw = "";

    /* Conta righe */
    int count = 1;
    for (const char *p = raw; *p; p++) {
        if (*p == '\n') count++;
    }

    g_sr_lines = (char **)malloc(count * sizeof(char *));
    if (!g_sr_lines) return;

    const char *start = raw;
    int idx = 0;
    while (*start) {
        const char *nl = strchr(start, '\n');
        size_t len = nl ? (size_t)(nl - start) : strlen(start);
        char *line = (char *)malloc(len + 1);
        if (line) {
            memcpy(line, start, len);
            line[len] = '\0';
            g_sr_lines[idx++] = line;
        }
        if (!nl) break;
        start = nl + 1;
    }
    g_sr_line_count = idx;
}

static void free_manual_lines(void) {
    if (!g_sr_lines) return;
    for (int i = 0; i < g_sr_line_count; i++) {
        if (g_sr_lines[i]) free(g_sr_lines[i]);
    }
    free(g_sr_lines);
    g_sr_lines = NULL;
    g_sr_line_count = 0;
}

static void init_sr_font(void) {
    if (g_sr_font_ready || !g_sr_ren) return;

    if (!g_sr_atlas) {
        g_sr_atlas = (unsigned char *)malloc(512 * 512);
    }
    stbtt_BakeFontBitmap(g_roboto_ttf, 0, 16.5f, g_sr_atlas, 512, 512, 32, 96, g_sr_cdata);

    uint32_t *rgba = (uint32_t *)malloc(512 * 512 * sizeof(uint32_t));
    if (!rgba) return;

    for (int i = 0; i < 512 * 512; i++) {
        uint8_t a = g_sr_atlas[i];
        rgba[i] = ((uint32_t)a << 24) | 0x00FFFFFF;
    }

    g_sr_atlas_tex = SDL_CreateTexture(g_sr_ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 512, 512);
    SDL_UpdateTexture(g_sr_atlas_tex, NULL, rgba, 512 * sizeof(uint32_t));
    SDL_SetTextureBlendMode(g_sr_atlas_tex, SDL_BLENDMODE_BLEND);

    free(rgba);
    g_sr_font_ready = true;
}

void linux_scriptref_toggle(void) {
    if (g_sr_win) {
        SDL_RaiseWindow(g_sr_win);
        return;
    }

    parse_manual_lines();

    g_sr_win = SDL_CreateWindow(
        "PLATO Scripting Language Reference",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCRIPTREF_WIN_W, SCRIPTREF_WIN_H,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!g_sr_win) return;

    g_sr_ren = SDL_CreateRenderer(g_sr_win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_sr_ren) {
        g_sr_ren = SDL_CreateRenderer(g_sr_win, -1, 0);
    }
    g_sr_scroll_y = 0;
    g_sr_dragging = false;
    g_sr_font_ready = false;
    init_sr_font();
}

bool linux_scriptref_is_open(void) {
    return (g_sr_win != NULL);
}

void linux_scriptref_handle_event(const SDL_Event *ev) {
    if (!g_sr_win) return;
    uint32_t win_id = SDL_GetWindowID(g_sr_win);

    if (ev->type == SDL_WINDOWEVENT && ev->window.windowID == win_id) {
        if (ev->window.event == SDL_WINDOWEVENT_CLOSE) {
            linux_scriptref_destroy();
            return;
        }
    } else if (ev->type == SDL_MOUSEWHEEL && ev->wheel.windowID == win_id) {
        g_sr_scroll_y -= ev->wheel.y * 36;
        if (g_sr_scroll_y < 0) g_sr_scroll_y = 0;
        int win_w, win_h;
        SDL_GetWindowSize(g_sr_win, &win_w, &win_h);
        int max_scroll = g_sr_line_count * SCRIPTREF_LINE_H - (win_h - SCRIPTREF_TOP_BAR_H - 20);
        if (max_scroll < 0) max_scroll = 0;
        if (g_sr_scroll_y > max_scroll) g_sr_scroll_y = max_scroll;
    } else if (ev->type == SDL_MOUSEBUTTONUP && ev->window.windowID == win_id) {
        if (ev->button.button == SDL_BUTTON_LEFT) {
            g_sr_dragging = false;
        }
    } else if (ev->type == SDL_MOUSEMOTION && ev->window.windowID == win_id) {
        if (g_sr_dragging && (ev->motion.state & SDL_BUTTON_LMASK)) {
            int win_w, win_h;
            SDL_GetWindowSize(g_sr_win, &win_w, &win_h);
            int content_h = g_sr_line_count * SCRIPTREF_LINE_H;
            int view_h = win_h - SCRIPTREF_TOP_BAR_H;
            int max_scroll = content_h - view_h;
            if (max_scroll > 0) {
                int bar_h = (view_h * view_h) / content_h;
                if (bar_h < 30) bar_h = 30;
                int track_h = view_h - bar_h;
                if (track_h > 0) {
                    int dy = ev->motion.y - g_sr_drag_start_y;
                    int new_scroll = g_sr_drag_start_scroll + (dy * max_scroll) / track_h;
                    if (new_scroll < 0) new_scroll = 0;
                    if (new_scroll > max_scroll) new_scroll = max_scroll;
                    g_sr_scroll_y = new_scroll;
                }
            }
            return;
        }
    } else if (ev->type == SDL_MOUSEBUTTONDOWN && ev->window.windowID == win_id) {
        if (ev->button.button == SDL_BUTTON_LEFT) {
            int win_w, win_h;
            SDL_GetWindowSize(g_sr_win, &win_w, &win_h);
            int mx = ev->button.x, my = ev->button.y;
            if (mx >= win_w - 20 && my >= SCRIPTREF_TOP_BAR_H) {
                int content_h = g_sr_line_count * SCRIPTREF_LINE_H;
                int view_h = win_h - SCRIPTREF_TOP_BAR_H;
                int max_scroll = content_h - view_h;
                if (max_scroll > 0) {
                    int bar_h = (view_h * view_h) / content_h;
                    if (bar_h < 30) bar_h = 30;
                    int track_h = view_h - bar_h;
                    if (track_h > 0) {
                        float ratio = (float)(my - SCRIPTREF_TOP_BAR_H - bar_h / 2) / (float)track_h;
                        if (ratio < 0.0f) ratio = 0.0f;
                        if (ratio > 1.0f) ratio = 1.0f;
                        g_sr_scroll_y = (int)(ratio * (float)max_scroll + 0.5f);
                        g_sr_dragging = true;
                        g_sr_drag_start_y = my;
                        g_sr_drag_start_scroll = g_sr_scroll_y;
                    }
                }
                return;
            }
        }
    } else if (ev->type == SDL_KEYDOWN && ev->key.windowID == win_id) {
        if (ev->key.keysym.sym == SDLK_ESCAPE) {
            linux_scriptref_destroy();
        } else if (ev->key.keysym.sym == SDLK_UP) {
            g_sr_scroll_y -= 36;
            if (g_sr_scroll_y < 0) g_sr_scroll_y = 0;
        } else if (ev->key.keysym.sym == SDLK_DOWN) {
            g_sr_scroll_y += 36;
        } else if (ev->key.keysym.sym == SDLK_PAGEUP) {
            g_sr_scroll_y -= 250;
            if (g_sr_scroll_y < 0) g_sr_scroll_y = 0;
        } else if (ev->key.keysym.sym == SDLK_PAGEDOWN) {
            g_sr_scroll_y += 250;
        }
    }
}

static void draw_sr_string(SDL_Renderer *ren, float x, float y, const char *str, uint8_t r, uint8_t g, uint8_t b) {
    if (!g_sr_atlas_tex || !str) return;
    SDL_SetTextureColorMod(g_sr_atlas_tex, r, g, b);
    while (*str) {
        char c = *str++;
        if ((unsigned char)c < 32 || (unsigned char)c >= 128) continue;
        const stbtt_bakedchar *bc = &g_sr_cdata[(unsigned char)c - 32];
        SDL_Rect src = { bc->x0, bc->y0, bc->x1 - bc->x0, bc->y1 - bc->y0 };
        SDL_Rect dst = { (int)(x + bc->xoff), (int)(y + bc->yoff), src.w, src.h };
        SDL_RenderCopy(ren, g_sr_atlas_tex, &src, &dst);
        x += bc->xadvance;
    }
}

void linux_scriptref_render(void) {
    if (!g_sr_win || !g_sr_ren) return;

    int win_w, win_h;
    SDL_GetWindowSize(g_sr_win, &win_w, &win_h);

    /* Sfondo principale Dark Amber Plasma */
    SDL_SetRenderDrawColor(g_sr_ren, 14, 9, 5, 255);
    SDL_RenderClear(g_sr_ren);

    int start_y = SCRIPTREF_TOP_BAR_H + 8 - g_sr_scroll_y;

    /* Render linee di documentazione */
    for (int i = 0; i < g_sr_line_count; i++) {
        int y = start_y + i * SCRIPTREF_LINE_H;
        if (y < SCRIPTREF_TOP_BAR_H - 20 || y > win_h + 30) continue;

        const char *line = g_sr_lines[i];
        if (!line) continue;

        if (line[0] == '#' && line[1] == ' ' && line[2] == '#') {
            /* Separatore o banner */
            draw_sr_string(g_sr_ren, 28.0f, (float)(y + 14), line, 140, 70, 20);
        } else if (line[0] == '#') {
            /* Commento */
            draw_sr_string(g_sr_ren, 28.0f, (float)(y + 14), line, 160, 110, 45);
        } else if (line[0] == '@' || (strncmp(line, "===", 3) == 0)) {
            /* Label o Sezione principale */
            draw_sr_string(g_sr_ren, 28.0f, (float)(y + 14), line, 255, 225, 75);
        } else if (strncmp(line, "wait_", 5) == 0 || strncmp(line, "send", 4) == 0 ||
                   strncmp(line, "key", 3) == 0 || strncmp(line, "let", 3) == 0 ||
                   strncmp(line, "for", 3) == 0 || strncmp(line, "if", 2) == 0 ||
                   strncmp(line, "find_text", 9) == 0 || strncmp(line, "exit", 4) == 0) {
            /* Istruzione evidenziata */
            draw_sr_string(g_sr_ren, 28.0f, (float)(y + 14), line, 255, 185, 40);
        } else {
            /* Testo standard ambra radiante */
            draw_sr_string(g_sr_ren, 28.0f, (float)(y + 14), line, 235, 150, 30);
        }
    }

    /* BARRA SUPERIORE FISSA IN LOOK PLASMA */
    SDL_SetRenderDrawColor(g_sr_ren, 36, 18, 6, 255);
    SDL_Rect bar_bg = { 0, 0, win_w, SCRIPTREF_TOP_BAR_H };
    SDL_RenderFillRect(g_sr_ren, &bar_bg);

    SDL_SetRenderDrawColor(g_sr_ren, 190, 85, 12, 255);
    SDL_Rect bar_border = { 0, SCRIPTREF_TOP_BAR_H - 2, win_w, 2 };
    SDL_RenderFillRect(g_sr_ren, &bar_border);

    /* Titolo in oro plasma */
    draw_sr_string(g_sr_ren, 28.0f, 26.0f, "PLATO SCRIPTING LANGUAGE REFERENCE", 255, 220, 65);
    draw_sr_string(g_sr_ren, 28.0f, 52.0f, "Commands, Syntax & Examples — Press Esc to close", 210, 140, 35);

    /* Scrollbar laterale ambra plasma */
    int content_h = g_sr_line_count * SCRIPTREF_LINE_H;
    int view_h = win_h - SCRIPTREF_TOP_BAR_H;
    if (content_h > view_h) {
        int bar_h = (view_h * view_h) / content_h;
        if (bar_h < 30) bar_h = 30;
        int bar_y = SCRIPTREF_TOP_BAR_H + (g_sr_scroll_y * (view_h - bar_h)) / (content_h - view_h);
        SDL_SetRenderDrawColor(g_sr_ren, 190, 85, 12, 230);
        SDL_Rect bar_rect = { win_w - 7, bar_y, 5, bar_h };
        SDL_RenderFillRect(g_sr_ren, &bar_rect);
    }

    SDL_RenderPresent(g_sr_ren);
}

void linux_scriptref_destroy(void) {
    if (g_sr_atlas_tex) {
        SDL_DestroyTexture(g_sr_atlas_tex);
        g_sr_atlas_tex = NULL;
    }
    g_sr_dragging = false;
    if (g_sr_atlas) {
        free(g_sr_atlas);
        g_sr_atlas = NULL;
    }
    if (g_sr_ren) {
        SDL_DestroyRenderer(g_sr_ren);
        g_sr_ren = NULL;
    }
    if (g_sr_win) {
        SDL_DestroyWindow(g_sr_win);
        g_sr_win = NULL;
    }
    free_manual_lines();
    g_sr_font_ready = false;
}
