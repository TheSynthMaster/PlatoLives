/*
 * PlatoLives - Native Linux Live Text Buffer Window (SDL2)
 * Full 32x64 screen text inspector, selection, freeze & clipboard export.
 */

#include "linux_livetext.h"
#include "linux_theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plato/stb_truetype.h"
#include "fonts/roboto_font_data.h"

#define LT_WIN_W 640
#define LT_WIN_H 560
#define LT_BOTTOM_BAR_H 44
#define LT_LINE_H 16
#define LT_CHAR_W 9.2f

static SDL_Window *g_lt_win = NULL;
static SDL_Renderer *g_lt_ren = NULL;
static plato_terminal_t *g_lt_term = NULL;

static stbtt_bakedchar g_lt_cdata[96];
static unsigned char *g_lt_atlas = NULL;
static SDL_Texture *g_lt_atlas_tex = NULL;
static bool g_lt_font_ready = false;

static bool g_lt_frozen = false;
static bool g_lt_compact = false;
static char g_lt_buffer[PLATO_ROWS * (PLATO_COLS + 4) + 1];

/* Selezione testo */
static bool g_lt_selecting = false;
static int g_lt_sel_start_r = -1, g_lt_sel_start_c = -1;
static int g_lt_sel_end_r = -1,   g_lt_sel_end_c = -1;

/* Scorrimento & Scrollbar */
static int g_lt_scroll_y = 0;
static bool g_lt_sb_dragging = false;
static int g_lt_drag_start_y = 0;
static int g_lt_drag_start_scroll = 0;

/* Flash giallo bottoni */
typedef enum {
    LT_BTN_NONE = 0,
    LT_BTN_SELALL,
    LT_BTN_LIVE,
    LT_BTN_COPY
} lt_btn_id_t;

static lt_btn_id_t g_lt_flash_btn = LT_BTN_NONE;
static uint32_t g_lt_flash_until = 0;

static void trigger_lt_flash(lt_btn_id_t id) {
    g_lt_flash_btn = id;
    g_lt_flash_until = SDL_GetTicks() + 250;
}

static bool is_lt_btn_flashing(lt_btn_id_t id) {
    return (g_lt_flash_btn == id && SDL_GetTicks() < g_lt_flash_until);
}

static void init_lt_font(void) {
    if (g_lt_font_ready || !g_lt_ren) return;
    if (!g_lt_atlas) {
        g_lt_atlas = (unsigned char *)malloc(512 * 512);
    }
    stbtt_BakeFontBitmap(g_roboto_ttf, 0, 15.0f, g_lt_atlas, 512, 512, 32, 96, g_lt_cdata);

    uint32_t *rgba = (uint32_t *)malloc(512 * 512 * sizeof(uint32_t));
    if (!rgba) return;

    for (int i = 0; i < 512 * 512; i++) {
        uint8_t a = g_lt_atlas[i];
        rgba[i] = ((uint32_t)a << 24) | 0x00FFFFFF;
    }

    g_lt_atlas_tex = SDL_CreateTexture(g_lt_ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 512, 512);
    SDL_UpdateTexture(g_lt_atlas_tex, NULL, rgba, 512 * sizeof(uint32_t));
    SDL_SetTextureBlendMode(g_lt_atlas_tex, SDL_BLENDMODE_BLEND);

    free(rgba);
    g_lt_font_ready = true;
}

static float get_char_width_f(char c) {
    if ((unsigned char)c < 32 || (unsigned char)c >= 128) return 6.0f;
    return g_lt_cdata[(unsigned char)c - 32].xadvance;
}

static float get_string_width_f(const char *str, int count) {
    if (!str) return 0.0f;
    float w = 0.0f;
    for (int i = 0; i < count && str[i]; i++) {
        w += get_char_width_f(str[i]);
    }
    return w;
}

static void draw_lt_text(SDL_Renderer *ren, float x, float y, const char *str, uint8_t r, uint8_t g, uint8_t b) {
    if (!g_lt_atlas_tex || !str) return;
    SDL_SetTextureColorMod(g_lt_atlas_tex, r, g, b);

    while (*str) {
        char c = *str++;
        if ((unsigned char)c < 32 || (unsigned char)c >= 128) continue;
        const stbtt_bakedchar *bc = &g_lt_cdata[(unsigned char)c - 32];

        SDL_Rect src = { bc->x0, bc->y0, bc->x1 - bc->x0, bc->y1 - bc->y0 };
        SDL_Rect dst = { (int)(x + bc->xoff + 0.5f), (int)(y + bc->yoff + 0.5f), src.w, src.h };

        SDL_RenderCopy(ren, g_lt_atlas_tex, &src, &dst);
        x += bc->xadvance;
    }
}

static void draw_lt_rect(SDL_Renderer *ren, int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, bool outline, uint8_t or, uint8_t og, uint8_t ob) {
    SDL_Rect rc = { x, y, w, h };
    SDL_SetRenderDrawColor(ren, r, g, b, 255);
    SDL_RenderFillRect(ren, &rc);
    if (outline) {
        SDL_SetRenderDrawColor(ren, or, og, ob, 255);
        SDL_RenderDrawRect(ren, &rc);
    }
}

static void draw_lt_button(SDL_Renderer *ren, int x, int y, int w, int h, const char *label, bool selected, bool flashing) {
    linux_theme_colors_t th = linux_theme_current();
    uint8_t bg_r = flashing ? th.bright.r : (selected ? th.hover_bg.r : th.bg_box.r);
    uint8_t bg_g = flashing ? th.bright.g : (selected ? th.hover_bg.g : th.bg_box.g);
    uint8_t bg_b = flashing ? th.bright.b : (selected ? th.hover_bg.b : th.bg_box.b);

    uint8_t bd_r = flashing ? 255 : (selected ? th.bright.r : th.border.r);
    uint8_t bd_g = flashing ? 255 : (selected ? th.bright.g : th.border.g);
    uint8_t bd_b = flashing ? 255 : (selected ? th.bright.b : th.border.b);

    draw_lt_rect(ren, x, y, w, h, bg_r, bg_g, bg_b, true, bd_r, bd_g, bd_b);

    float tw = get_string_width_f(label, (int)strlen(label));
    float tx = (float)x + ((float)w - tw) / 2.0f;
    float ty = (float)y + 17.0f;

    uint8_t fg_r = flashing ? 0 : (selected ? th.bright.r : th.normal.r);
    uint8_t fg_g = flashing ? 0 : (selected ? th.bright.g : th.normal.g);
    uint8_t fg_b = flashing ? 0 : (selected ? th.bright.b : th.normal.b);

    draw_lt_text(ren, tx, ty, label, fg_r, fg_g, fg_b);
}

static void refresh_buffer(void) {
    if (!g_lt_term || g_lt_frozen) return;
    plato_terminal_get_text_area(g_lt_term, 0, 0, PLATO_COLS - 1, PLATO_ROWS - 1, g_lt_compact, g_lt_buffer, sizeof(g_lt_buffer));
}

void linux_livetext_show(plato_terminal_t *term) {
    if (!term) return;
    g_lt_term = term;

    if (g_lt_win) {
        SDL_RaiseWindow(g_lt_win);
        return;
    }

    g_lt_win = SDL_CreateWindow(
        "PLATO Live Text Buffer [● LIVE]",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        LT_WIN_W, LT_WIN_H,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!g_lt_win) return;

    g_lt_ren = SDL_CreateRenderer(g_lt_win, -1, SDL_RENDERER_SOFTWARE);
    if (!g_lt_ren) {
        g_lt_ren = SDL_CreateRenderer(g_lt_win, -1, 0);
    }

    g_lt_font_ready = false;
    init_lt_font();
    g_lt_frozen = false;
    g_lt_selecting = false;
    g_lt_sel_start_r = g_lt_sel_start_c = -1;
    g_lt_sel_end_r = g_lt_sel_end_c = -1;
    g_lt_scroll_y = 0;
    refresh_buffer();
}

void linux_livetext_toggle(plato_terminal_t *term) {
    if (g_lt_win) {
        linux_livetext_destroy();
    } else {
        linux_livetext_show(term);
    }
}

bool linux_livetext_is_open(void) {
    return (g_lt_win != NULL);
}

void linux_livetext_destroy(void) {
    if (g_lt_atlas_tex) {
        SDL_DestroyTexture(g_lt_atlas_tex);
        g_lt_atlas_tex = NULL;
    }
    if (g_lt_atlas) {
        free(g_lt_atlas);
        g_lt_atlas = NULL;
    }
    if (g_lt_ren) {
        SDL_DestroyRenderer(g_lt_ren);
        g_lt_ren = NULL;
    }
    if (g_lt_win) {
        SDL_DestroyWindow(g_lt_win);
        g_lt_win = NULL;
    }
    g_lt_font_ready = false;
}

static bool is_pos_selected(int r, int c) {
    if (g_lt_sel_start_r < 0 || g_lt_sel_end_r < 0) return false;
    int sr = g_lt_sel_start_r, sc = g_lt_sel_start_c;
    int er = g_lt_sel_end_r,   ec = g_lt_sel_end_c;

    if (sr > er || (sr == er && sc > ec)) {
        int tr = sr; sr = er; er = tr;
        int tc = sc; sc = ec; ec = tc;
    }

    if (r < sr || r > er) return false;
    if (r == sr && r == er) return (c >= sc && c <= ec);
    if (r == sr) return (c >= sc);
    if (r == er) return (c <= ec);
    return true;
}

static void copy_selection_or_all(void) {
    if (g_lt_sel_start_r >= 0 && g_lt_sel_end_r >= 0) {
        /* Copia testo selezionato */
        int sr = g_lt_sel_start_r, sc = g_lt_sel_start_c;
        int er = g_lt_sel_end_r,   ec = g_lt_sel_end_c;
        if (sr > er || (sr == er && sc > ec)) {
            int tr = sr; sr = er; er = tr;
            int tc = sc; sc = ec; ec = tc;
        }

        char out[4096] = {0};
        size_t oidx = 0;
        const char *p = g_lt_buffer;
        int cur_r = 0, cur_c = 0;

        while (*p && oidx < sizeof(out) - 2) {
            char ch = *p++;
            if (ch == '\n') {
                if (cur_r >= sr && cur_r <= er) {
                    out[oidx++] = '\n';
                }
                cur_r++;
                cur_c = 0;
            } else {
                if (is_pos_selected(cur_r, cur_c)) {
                    out[oidx++] = ch;
                }
                cur_c++;
            }
        }
        out[oidx] = '\0';
        if (oidx > 0) SDL_SetClipboardText(out);
    } else {
        /* Copia tutto il buffer */
        if (g_lt_buffer[0]) {
            SDL_SetClipboardText(g_lt_buffer);
        }
    }
}

static bool pt_in_rect(int px, int py, int x, int y, int w, int h) {
    return (px >= x && px < x + w && py >= y && py < y + h);
}

void linux_livetext_handle_event(const SDL_Event *ev) {
    if (!g_lt_win) return;
    uint32_t win_id = SDL_GetWindowID(g_lt_win);

    if (ev->type == SDL_WINDOWEVENT && ev->window.windowID == win_id) {
        if (ev->window.event == SDL_WINDOWEVENT_CLOSE) {
            linux_livetext_destroy();
            return;
        }
    } else if (ev->type == SDL_MOUSEWHEEL && ev->wheel.windowID == win_id) {
        g_lt_scroll_y -= ev->wheel.y * 24;
        if (g_lt_scroll_y < 0) g_lt_scroll_y = 0;
        int win_w, win_h;
        SDL_GetWindowSize(g_lt_win, &win_w, &win_h);
        int max_scroll = (PLATO_ROWS * LT_LINE_H) - (win_h - LT_BOTTOM_BAR_H - 20);
        if (max_scroll < 0) max_scroll = 0;
        if (g_lt_scroll_y > max_scroll) g_lt_scroll_y = max_scroll;
    } else if (ev->type == SDL_MOUSEBUTTONUP && ev->button.windowID == win_id) {
        if (ev->button.button == SDL_BUTTON_LEFT) {
            g_lt_sb_dragging = false;
            g_lt_selecting = false;
        }
    } else if (ev->type == SDL_MOUSEMOTION && ev->motion.windowID == win_id) {
        if (g_lt_sb_dragging && (ev->motion.state & SDL_BUTTON_LMASK)) {
            int win_w, win_h;
            SDL_GetWindowSize(g_lt_win, &win_w, &win_h);
            int content_h = PLATO_ROWS * LT_LINE_H;
            int view_h = win_h - LT_BOTTOM_BAR_H;
            int max_scroll = content_h - view_h;
            if (max_scroll > 0) {
                int track_h = view_h - 30;
                if (track_h > 0) {
                    int dy = ev->motion.y - g_lt_drag_start_y;
                    int new_scroll = g_lt_drag_start_scroll + (dy * max_scroll) / track_h;
                    if (new_scroll < 0) new_scroll = 0;
                    if (new_scroll > max_scroll) new_scroll = max_scroll;
                    g_lt_scroll_y = new_scroll;
                }
            }
            return;
        }

        if (g_lt_selecting && (ev->motion.state & SDL_BUTTON_LMASK)) {
            int mx = ev->motion.x, my = ev->motion.y;
            int r = (my - 10 + g_lt_scroll_y) / LT_LINE_H;
            int c = (int)((float)(mx - 14) / LT_CHAR_W);
            if (r < 0) r = 0; if (r >= PLATO_ROWS) r = PLATO_ROWS - 1;
            if (c < 0) c = 0; if (c >= PLATO_COLS) c = PLATO_COLS - 1;
            g_lt_sel_end_r = r;
            g_lt_sel_end_c = c;
            g_lt_frozen = true;
            SDL_SetWindowTitle(g_lt_win, "PLATO Live Text Buffer [⏸ FROZEN]");
        }
    } else if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.windowID == win_id) {
        int mx = ev->button.x, my = ev->button.y;
        int win_w, win_h;
        SDL_GetWindowSize(g_lt_win, &win_w, &win_h);

        /* Barra Inferiore */
        if (my >= win_h - LT_BOTTOM_BAR_H) {
            /* Pulsante Select All */
            if (pt_in_rect(mx, my, 12, win_h - 35, 85, 26)) {
                trigger_lt_flash(LT_BTN_SELALL);
                g_lt_sel_start_r = 0; g_lt_sel_start_c = 0;
                g_lt_sel_end_r = PLATO_ROWS - 1; g_lt_sel_end_c = PLATO_COLS - 1;
                g_lt_frozen = true;
                SDL_SetWindowTitle(g_lt_win, "PLATO Live Text Buffer [⏸ FROZEN]");
                return;
            }

            /* Pulsante Deselect / Live */
            if (pt_in_rect(mx, my, 105, win_h - 35, 115, 26)) {
                trigger_lt_flash(LT_BTN_LIVE);
                g_lt_sel_start_r = g_lt_sel_start_c = -1;
                g_lt_sel_end_r = g_lt_sel_end_c = -1;
                g_lt_frozen = false;
                SDL_SetWindowTitle(g_lt_win, "PLATO Live Text Buffer [● LIVE]");
                refresh_buffer();
                return;
            }

            /* Checkbox Compact */
            if (pt_in_rect(mx, my, 230, win_h - 35, 85, 26)) {
                g_lt_compact = !g_lt_compact;
                refresh_buffer();
                return;
            }

            /* Pulsante Copy to Clipboard */
            if (pt_in_rect(mx, my, 325, win_h - 35, 140, 26)) {
                trigger_lt_flash(LT_BTN_COPY);
                copy_selection_or_all();
                return;
            }
            return;
        }

        /* Area Testo: Inizio Selezione Mouse */
        if (mx < win_w - 20) {
            int r = (my - 10 + g_lt_scroll_y) / LT_LINE_H;
            int c = (int)((float)(mx - 14) / LT_CHAR_W);
            if (r < 0) r = 0; if (r >= PLATO_ROWS) r = PLATO_ROWS - 1;
            if (c < 0) c = 0; if (c >= PLATO_COLS) c = PLATO_COLS - 1;
            g_lt_sel_start_r = g_lt_sel_end_r = r;
            g_lt_sel_start_c = g_lt_sel_end_c = c;
            g_lt_selecting = true;
            return;
        }

        /* Scrollbar click */
        if (mx >= win_w - 20) {
            int content_h = PLATO_ROWS * LT_LINE_H;
            int view_h = win_h - LT_BOTTOM_BAR_H;
            int max_scroll = content_h - view_h;
            if (max_scroll > 0) {
                g_lt_sb_dragging = true;
                g_lt_drag_start_y = my;
                g_lt_drag_start_scroll = g_lt_scroll_y;
            }
        }
    } else if (ev->type == SDL_KEYDOWN && ev->key.windowID == win_id) {
        if (ev->key.keysym.sym == SDLK_ESCAPE) {
            linux_livetext_destroy();
        } else if (ev->key.keysym.sym == SDLK_c && (SDL_GetModState() & (KMOD_CTRL | KMOD_GUI))) {
            trigger_lt_flash(LT_BTN_COPY);
            copy_selection_or_all();
        }
    }
}

void linux_livetext_render(void) {
    linux_theme_colors_t th = linux_theme_current();
    if (!g_lt_win || !g_lt_ren) return;

    refresh_buffer();

    int win_w, win_h;
    SDL_GetWindowSize(g_lt_win, &win_w, &win_h);

    /* Sfondo Principale Dark Amber */
    SDL_SetRenderDrawColor(g_lt_ren, th.bg_dark.r, th.bg_dark.g, th.bg_dark.b, 255);
    SDL_RenderClear(g_lt_ren);

    /* Render Righe di Testo 32x64 */
    const char *p = g_lt_buffer;
    int r = 0, c = 0;
    int start_y = 10 - g_lt_scroll_y;

    while (*p) {
        char ch = *p++;
        if (ch == '\n') {
            r++;
            c = 0;
            continue;
        }

        int px = (int)(14.0f + (float)c * LT_CHAR_W);
        int py = start_y + r * LT_LINE_H;

        if (py >= -16 && py < win_h - LT_BOTTOM_BAR_H + 16) {
            bool sel = is_pos_selected(r, c);
            if (sel) {
                draw_lt_rect(g_lt_ren, px - 1, py - 1, (int)LT_CHAR_W + 1, LT_LINE_H, th.hover_bg.r, th.hover_bg.g, th.hover_bg.b, false, 0, 0, 0);
            }
            char s[2] = { ch, '\0' };
            uint8_t cr = sel ? th.bright.r : th.normal.r;
            uint8_t cg = sel ? th.bright.g : th.normal.g;
            uint8_t cb = sel ? th.bright.b : th.normal.b;
            draw_lt_text(g_lt_ren, (float)px, (float)(py + 13), s, cr, cg, cb);
        }
        c++;
    }

    /* BARRA INFERIORE A 44px */
    draw_lt_rect(g_lt_ren, 0, win_h - LT_BOTTOM_BAR_H, win_w, LT_BOTTOM_BAR_H, th.bg_box.r, th.bg_box.g, th.bg_box.b, true, th.border.r, th.border.g, th.border.b);

    /* Pulsanti Inferiori con Flash Giallo */
    draw_lt_button(g_lt_ren, 12, win_h - 35, 85, 26, "Select All", false, is_lt_btn_flashing(LT_BTN_SELALL));
    draw_lt_button(g_lt_ren, 105, win_h - 35, 115, 26, "Deselect / Live", false, is_lt_btn_flashing(LT_BTN_LIVE));

    /* Checkbox Compact */
    draw_lt_rect(g_lt_ren, 230, win_h - 31, 16, 16, th.bg_box.r, th.bg_box.g, th.bg_box.b, true, th.border.r, th.border.g, th.border.b);
    if (g_lt_compact) {
        draw_lt_rect(g_lt_ren, 234, win_h - 27, 8, 8, th.bright.r, th.bright.g, th.bright.b, false, 0, 0, 0);
    }
    draw_lt_text(g_lt_ren, 252.0f, (float)(win_h - 18), "Compact", th.normal.r, th.normal.g, th.normal.b);

    /* Pulsante Copy to Clipboard */
    draw_lt_button(g_lt_ren, 325, win_h - 35, 140, 26, "Copy to Clipboard", false, is_lt_btn_flashing(LT_BTN_COPY));

    /* Status Indicator Badge a Destra */
    if (g_lt_frozen) {
        draw_lt_text(g_lt_ren, (float)(win_w - 110), (float)(win_h - 17), "[⏸ FROZEN]", th.dim.r, th.dim.g, th.dim.b);
    } else {
        draw_lt_text(g_lt_ren, (float)(win_w - 95), (float)(win_h - 17), "[● LIVE]", th.bright.r, th.bright.g, th.bright.b);
    }

    /* Scrollbar laterale */
    int content_h = PLATO_ROWS * LT_LINE_H;
    int view_h = win_h - LT_BOTTOM_BAR_H;
    if (content_h > view_h) {
        int bar_h = (view_h * view_h) / content_h;
        if (bar_h < 30) bar_h = 30;
        int bar_y = (g_lt_scroll_y * (view_h - bar_h)) / (content_h - view_h);
        draw_lt_rect(g_lt_ren, win_w - 7, bar_y, 5, bar_h, th.border.r, th.border.g, th.border.b, false, 0, 0, 0);
    }

    SDL_RenderPresent(g_lt_ren);
}
