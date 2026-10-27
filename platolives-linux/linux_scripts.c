/*
 * PlatoLives - Native Linux Scripts Manager (SDL2)
 * Full multi-line script editor, scrolling, hotkey recorder & pixel-perfect UI.
 * Architecture mirrored 1:1 from linux_profiles.c.
 */

#include "linux_scripts.h"
#include "linux_theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "plato/stb_truetype.h"
#include "fonts/roboto_font_data.h"

#define SCR_WIN_W 980
#define SCR_WIN_H 640

#define SCRIPT_BOX_X 260
#define SCRIPT_BOX_Y 200
#define SCRIPT_BOX_W 695
#define SCRIPT_BOX_H 335
#define SCRIPT_VISIBLE_LINES 15
#define SCRIPT_LINE_HEIGHT 21

static SDL_Window *g_scr_win = NULL;
static SDL_Renderer *g_scr_ren = NULL;
static stbtt_bakedchar g_scr_cdata[96];
static unsigned char *g_scr_atlas = NULL;
static SDL_Texture *g_scr_atlas_tex = NULL;
static bool g_scr_font_ready = false;

static plato_script_list_t *g_script_list = NULL;
static int g_current_idx = 0;
static linux_script_run_cb g_run_cb = NULL;
static void *g_run_cb_user_data = NULL;

typedef enum {
    SCR_FIELD_NONE = 0,
    SCR_FIELD_NAME,
    SCR_FIELD_CHAR_DEL,
    SCR_FIELD_NEXT_DEL,
    SCR_FIELD_CMD_DEL,
    SCR_FIELD_BODY
} scr_active_field_t;

static scr_active_field_t g_active_field = SCR_FIELD_NONE;
static plato_script_t g_edit_script;
static int g_list_scroll = 0;

/* Stato Editor Script Multi-linea */
static int g_script_cursor = 0;
static int g_script_sel_start = 0;
static int g_script_sel_end = 0;
static bool g_script_text_dragging = false;
static int g_script_scroll_line = 0;
static bool g_sb_dragging = false;

static void script_delete_selection(void) {
    if (g_script_sel_start == g_script_sel_end) return;
    int a = g_script_sel_start < g_script_sel_end ? g_script_sel_start : g_script_sel_end;
    int b = g_script_sel_start < g_script_sel_end ? g_script_sel_end : g_script_sel_start;
    size_t len = strlen(g_edit_script.body);
    if (b > (int)len) b = (int)len;
    if (a < 0) a = 0;
    memmove(&g_edit_script.body[a], &g_edit_script.body[b], len - b + 1);
    g_script_cursor = a;
    g_script_sel_start = g_script_sel_end = a;
}
static int g_sb_drag_start_y = 0;
static int g_sb_drag_start_scroll = 0;

/* Stato Hotkey Recorder */
static bool g_recording_hotkey = false;

typedef enum {
    SCR_BTN_NONE = 0,
    SCR_BTN_NEW,
    SCR_BTN_CLONE,
    SCR_BTN_DELETE,
    SCR_BTN_RECORD,
    SCR_BTN_CLEAR,
    SCR_BTN_RUN,
    SCR_BTN_SAVE
} scr_btn_id_t;

static scr_btn_id_t g_flash_btn = SCR_BTN_NONE;
static uint32_t g_flash_until = 0;

static void trigger_flash_btn(scr_btn_id_t btn) {
    g_flash_btn = btn;
    g_flash_until = SDL_GetTicks() + 250;
}

static bool is_btn_flashing(scr_btn_id_t btn) {
    return (g_flash_btn == btn && SDL_GetTicks() < g_flash_until);
}


static void init_scr_font(void) {
    if (g_scr_font_ready || !g_scr_ren) return;
    if (!g_scr_atlas) {
        g_scr_atlas = (unsigned char *)malloc(512 * 512);
    }
    stbtt_BakeFontBitmap(g_roboto_ttf, 0, 16.5f, g_scr_atlas, 512, 512, 32, 96, g_scr_cdata);

    uint32_t *rgba = (uint32_t *)malloc(512 * 512 * sizeof(uint32_t));
    if (!rgba) return;

    for (int i = 0; i < 512 * 512; i++) {
        uint8_t a = g_scr_atlas[i];
        rgba[i] = ((uint32_t)a << 24) | 0x00FFFFFF;
    }

    g_scr_atlas_tex = SDL_CreateTexture(g_scr_ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 512, 512);
    SDL_UpdateTexture(g_scr_atlas_tex, NULL, rgba, 512 * sizeof(uint32_t));
    SDL_SetTextureBlendMode(g_scr_atlas_tex, SDL_BLENDMODE_BLEND);

    free(rgba);
    g_scr_font_ready = true;
}

static float get_char_width_f(char c) {
    if ((unsigned char)c < 32 || (unsigned char)c >= 128) return 6.0f;
    return g_scr_cdata[(unsigned char)c - 32].xadvance;
}

static float get_string_width_f(const char *str, int count) {
    if (!str) return 0.0f;
    float w = 0.0f;
    for (int i = 0; i < count && str[i]; i++) {
        w += get_char_width_f(str[i]);
    }
    return w;
}

static void draw_scr_text(SDL_Renderer *ren, float x, float y, const char *str, uint8_t r, uint8_t g, uint8_t b) {
    if (!g_scr_atlas_tex || !str) return;
    SDL_SetTextureColorMod(g_scr_atlas_tex, r, g, b);

    while (*str) {
        char c = *str++;
        if ((unsigned char)c < 32 || (unsigned char)c >= 128) continue;
        const stbtt_bakedchar *bc = &g_scr_cdata[(unsigned char)c - 32];

        SDL_Rect src = { bc->x0, bc->y0, bc->x1 - bc->x0, bc->y1 - bc->y0 };
        SDL_Rect dst = { (int)(x + bc->xoff + 0.5f), (int)(y + bc->yoff + 0.5f), src.w, src.h };

        SDL_RenderCopy(ren, g_scr_atlas_tex, &src, &dst);
        x += bc->xadvance;
    }
}

static void draw_scr_rect(SDL_Renderer *ren, int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, bool outline, uint8_t or, uint8_t og, uint8_t ob) {
    SDL_Rect rc = { x, y, w, h };
    SDL_SetRenderDrawColor(ren, r, g, b, 255);
    SDL_RenderFillRect(ren, &rc);
    if (outline) {
        SDL_SetRenderDrawColor(ren, or, og, ob, 255);
        SDL_RenderDrawRect(ren, &rc);
    }
}

static void draw_scr_button(SDL_Renderer *ren, int x, int y, int w, int h, const char *label, bool selected, bool flashing) {
    linux_theme_colors_t th = linux_theme_current();
    uint8_t bg_r = flashing ? th.bright.r : (selected ? th.hover_bg.r : th.bg_box.r);
    uint8_t bg_g = flashing ? th.bright.g : (selected ? th.hover_bg.g : th.bg_box.g);
    uint8_t bg_b = flashing ? th.bright.b : (selected ? th.hover_bg.b : th.bg_box.b);

    uint8_t bd_r = flashing ? 255 : (selected ? th.bright.r : th.border.r);
    uint8_t bd_g = flashing ? 255 : (selected ? th.bright.g : th.border.g);
    uint8_t bd_b = flashing ? 255 : (selected ? th.bright.b : th.border.b);

    draw_scr_rect(ren, x, y, w, h, bg_r, bg_g, bg_b, true, bd_r, bd_g, bd_b);

    float tw = get_string_width_f(label, (int)strlen(label));
    float tx = (float)x + ((float)w - tw) / 2.0f;
    float ty = (float)y + ((float)h) / 2.0f + 5.0f;

    uint8_t fg_r = flashing ? 0 : (selected ? th.bright.r : th.normal.r);
    uint8_t fg_g = flashing ? 0 : (selected ? th.bright.g : th.normal.g);
    uint8_t fg_b = flashing ? 0 : (selected ? th.bright.b : th.normal.b);

    draw_scr_text(ren, tx, ty, label, fg_r, fg_g, fg_b);
}

static int get_total_script_lines(const char *text) {
    if (!text || !*text) return 1;
    int count = 1;
    for (const char *p = text; *p; p++) {
        if (*p == '\n') count++;
    }
    return count;
}

static void get_cursor_line_col(const char *text, int cursor, int *out_line, int *out_col) {
    int line = 0, col = 0;
    if (cursor > (int)strlen(text)) cursor = (int)strlen(text);
    for (int i = 0; i < cursor && text[i]; i++) {
        if (text[i] == '\n') {
            line++;
            col = 0;
        } else {
            col++;
        }
    }
    *out_line = line;
    *out_col = col;
}

static int find_pos_at_line_col(const char *text, int target_line, int target_col) {
    int line = 0, col = 0;
    int i = 0;
    while (text[i] && line < target_line) {
        if (text[i] == '\n') line++;
        i++;
    }
    while (text[i] && text[i] != '\n' && col < target_col) {
        col++;
        i++;
    }
    return i;
}

static void ensure_cursor_visible(void) {
    int cur_line, cur_col;
    get_cursor_line_col(g_edit_script.body, g_script_cursor, &cur_line, &cur_col);
    if (cur_line < g_script_scroll_line) {
        g_script_scroll_line = cur_line;
    } else if (cur_line >= g_script_scroll_line + SCRIPT_VISIBLE_LINES) {
        g_script_scroll_line = cur_line - SCRIPT_VISIBLE_LINES + 1;
    }
}

static void sync_from_script(int idx) {
    if (!g_script_list || idx < 0 || (size_t)idx >= g_script_list->count) {
        memset(&g_edit_script, 0, sizeof(g_edit_script));
        g_current_idx = -1;
        return;
    }
    g_current_idx = idx;
    g_edit_script = g_script_list->scripts[idx];
    if (g_edit_script.char_delay_ms <= 0) g_edit_script.char_delay_ms = 250;
    if (g_edit_script.next_delay_ms <= 0) g_edit_script.next_delay_ms = 500;
    if (g_edit_script.command_delay_ms < 0) g_edit_script.command_delay_ms = 0;

    g_script_cursor = 0;
    g_script_scroll_line = 0;
}

static void sync_to_script(void) {
    if (!g_script_list || g_current_idx < 0 || (size_t)g_current_idx >= g_script_list->count) return;
    g_script_list->scripts[g_current_idx] = g_edit_script;
    plato_scripts_save(g_script_list, plato_scripts_get_default_path());
}

void linux_scripts_show(plato_script_list_t *list, linux_script_run_cb run_cb, void *user_data) {
    if (!list) return;
    g_script_list = list;
    g_run_cb = run_cb;
    g_run_cb_user_data = user_data;
    g_recording_hotkey = false;

    if (g_scr_win) {
        SDL_RaiseWindow(g_scr_win);
        return;
    }

    g_scr_win = SDL_CreateWindow(
        "Manage Scripts - PlatoLives",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCR_WIN_W, SCR_WIN_H,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!g_scr_win) return;

    g_scr_ren = SDL_CreateRenderer(g_scr_win, -1, SDL_RENDERER_SOFTWARE);
    if (!g_scr_ren) {
        g_scr_ren = SDL_CreateRenderer(g_scr_win, -1, 0);
    }

    g_scr_font_ready = false;
    init_scr_font();

    if (list->count == 0) {
        plato_script_t s = {0};
        snprintf(s.name, sizeof(s.name), "Cyber1 Auto-Login");
        s.enabled = true;
        s.char_delay_ms = 250;
        s.next_delay_ms = 500;
        s.command_delay_ms = 0;
        snprintf(s.body, sizeof(s.body), "%s", PLATO_DEFAULT_AUTOLOGIN_SCRIPT);
        plato_scripts_add(list, &s);
    }

    sync_from_script(0);
    g_active_field = SCR_FIELD_NONE;
}

bool linux_scripts_is_open(void) {
    return (g_scr_win != NULL);
}

void linux_scripts_destroy(void) {
    if (g_scr_atlas_tex) {
        SDL_DestroyTexture(g_scr_atlas_tex);
        g_scr_atlas_tex = NULL;
    }
    if (g_scr_atlas) {
        free(g_scr_atlas);
        g_scr_atlas = NULL;
    }
    if (g_scr_ren) {
        SDL_DestroyRenderer(g_scr_ren);
        g_scr_ren = NULL;
    }
    if (g_scr_win) {
        SDL_DestroyWindow(g_scr_win);
        g_scr_win = NULL;
    }
    g_scr_font_ready = false;
    g_recording_hotkey = false;
}

static bool pt_in_rect(int px, int py, int x, int y, int w, int h) {
    return (px >= x && px < x + w && py >= y && py < y + h);
}

void linux_scripts_handle_event(const SDL_Event *ev) {
    if (!g_scr_win) return;
    uint32_t win_id = SDL_GetWindowID(g_scr_win);

    if (ev->type == SDL_WINDOWEVENT && ev->window.windowID == win_id) {
        if (ev->window.event == SDL_WINDOWEVENT_CLOSE) {
            sync_to_script();
            linux_scripts_destroy();
            return;
        }
    } else if (ev->type == SDL_MOUSEWHEEL && ev->wheel.windowID == win_id) {
        int total = get_total_script_lines(g_edit_script.body);
        int max_scroll = total - SCRIPT_VISIBLE_LINES;
        if (max_scroll < 0) max_scroll = 0;

        g_script_scroll_line -= ev->wheel.y * 3;
        if (g_script_scroll_line < 0) g_script_scroll_line = 0;
        if (g_script_scroll_line > max_scroll) g_script_scroll_line = max_scroll;
        return;
    } else if (ev->type == SDL_MOUSEBUTTONUP && ev->button.windowID == win_id) {
        if (ev->button.button == SDL_BUTTON_LEFT) {
            g_sb_dragging = false;
            g_script_text_dragging = false;
        }
    } else if (ev->type == SDL_MOUSEMOTION && ev->motion.windowID == win_id) {
        if (g_script_text_dragging && (ev->motion.state & SDL_BUTTON_LMASK)) {
            int rel_y = ev->motion.y - SCRIPT_BOX_Y - 4;
            int clicked_line = g_script_scroll_line + (rel_y / SCRIPT_LINE_HEIGHT);
            int rel_x = ev->motion.x - SCRIPT_BOX_X - 8;
            int clicked_col = (rel_x > 0) ? (int)((float)rel_x / 8.5f) : 0;
            g_script_cursor = find_pos_at_line_col(g_edit_script.body, clicked_line, clicked_col);
            g_script_sel_end = g_script_cursor;
            ensure_cursor_visible();
            return;
        }
        if (g_sb_dragging && (ev->motion.state & SDL_BUTTON_LMASK)) {
            int total_lines = get_total_script_lines(g_edit_script.body);
            int max_scroll = total_lines - SCRIPT_VISIBLE_LINES;
            if (max_scroll > 0) {
                int sb_h = SCRIPT_BOX_H - 4;
                int thumb_h = (sb_h * SCRIPT_VISIBLE_LINES) / total_lines;
                if (thumb_h < 18) thumb_h = 18;
                int track_h = sb_h - thumb_h;
                if (track_h > 0) {
                    int dy = ev->motion.y - g_sb_drag_start_y;
                    int new_scroll = g_sb_drag_start_scroll + (dy * max_scroll) / track_h;
                    if (new_scroll < 0) new_scroll = 0;
                    if (new_scroll > max_scroll) new_scroll = max_scroll;
                    g_script_scroll_line = new_scroll;
                }
            }
            return;
        }
    } else if (ev->type == SDL_KEYDOWN && ev->key.windowID == win_id) {
        /* Cattura Hotkey se in registrazione */
        if (g_recording_hotkey) {
            SDL_Keycode sym = ev->key.keysym.sym;
            if (sym == SDLK_ESCAPE) {
                g_recording_hotkey = false;
                return;
            }
            if (sym == SDLK_LCTRL || sym == SDLK_RCTRL ||
                sym == SDLK_LSHIFT || sym == SDLK_RSHIFT ||
                sym == SDLK_LALT || sym == SDLK_RALT ||
                sym == SDLK_LGUI || sym == SDLK_RGUI) {
                return; /* Aspetta tasto concreto */
            }

            SDL_Keymod mod = SDL_GetModState();
            uint32_t pmods = 0;
            if (mod & KMOD_CTRL)  pmods |= PLATO_HOTKEY_MOD_CTRL;
            if (mod & KMOD_SHIFT) pmods |= PLATO_HOTKEY_MOD_SHIFT;
            if (mod & KMOD_ALT)   pmods |= PLATO_HOTKEY_MOD_ALT;
            if (mod & KMOD_GUI)   pmods |= PLATO_HOTKEY_MOD_CMD;

            /* Controllo combinazione riservata: Ctrl+Shift+X */
            if ((pmods & (PLATO_HOTKEY_MOD_CTRL | PLATO_HOTKEY_MOD_SHIFT)) == (PLATO_HOTKEY_MOD_CTRL | PLATO_HOTKEY_MOD_SHIFT) &&
                (sym == SDLK_x)) {
                g_recording_hotkey = false;
                return;
            }

            g_edit_script.hotkey_modifiers = pmods;
            g_edit_script.hotkey_key = (uint32_t)sym;

            /* Genera testo display scorciatoia */
            char disp[PLATO_SCRIPT_MAX_HOTKEY] = {0};
            if (pmods & PLATO_HOTKEY_MOD_CTRL) strcat(disp, "Ctrl+");
            if (pmods & PLATO_HOTKEY_MOD_ALT)  strcat(disp, "Alt+");
            if (pmods & PLATO_HOTKEY_MOD_SHIFT) strcat(disp, "Shift+");
            if (pmods & PLATO_HOTKEY_MOD_CMD)  strcat(disp, "Cmd+");

            const char *kname = SDL_GetKeyName(sym);
            if (kname && *kname) {
                strncat(disp, kname, sizeof(disp) - strlen(disp) - 1);
            }
            snprintf(g_edit_script.hotkey_display, sizeof(g_edit_script.hotkey_display), "%s", disp);
            g_recording_hotkey = false;
            
            return;
        }

        if (ev->key.keysym.sym == SDLK_ESCAPE) {
            sync_to_script();
            linux_scripts_destroy();
            return;
        }

        SDL_Keymod mod = SDL_GetModState();
        bool ctrl = (mod & KMOD_CTRL) != 0;
        bool gui  = (mod & KMOD_GUI) != 0;
        bool shift = (mod & KMOD_SHIFT) != 0;

        /* Seleziona Tutto (Ctrl+A / Cmd+A) */
        if ((ctrl || gui) && ev->key.keysym.sym == SDLK_a && g_active_field == SCR_FIELD_BODY) {
            g_script_sel_start = 0;
            g_script_sel_end = (int)strlen(g_edit_script.body);
            g_script_cursor = g_script_sel_end;
            ensure_cursor_visible();
            return;
        }

        /* Copia (Ctrl+C) e Taglia (Ctrl+X) */
        if ((ctrl || gui) && (ev->key.keysym.sym == SDLK_c || ev->key.keysym.sym == SDLK_x) && g_active_field == SCR_FIELD_BODY) {
            if (g_script_sel_start != g_script_sel_end) {
                int a = g_script_sel_start < g_script_sel_end ? g_script_sel_start : g_script_sel_end;
                int b = g_script_sel_start < g_script_sel_end ? g_script_sel_end : g_script_sel_start;
                char tmp[sizeof(g_edit_script.body)];
                int clen = b - a;
                if (clen > (int)sizeof(tmp) - 1) clen = (int)sizeof(tmp) - 1;
                memcpy(tmp, &g_edit_script.body[a], clen);
                tmp[clen] = 0;
                SDL_SetClipboardText(tmp);
                if (ev->key.keysym.sym == SDLK_x) {
                    script_delete_selection();
                    ensure_cursor_visible();
                }
            } else {
                SDL_SetClipboardText(g_edit_script.body);
            }
            return;
        }

        /* Incolla (Ctrl+V / Cmd+V) */
        if ((ctrl || gui) && ev->key.keysym.sym == SDLK_v && g_active_field == SCR_FIELD_BODY) {
            char *cb_text = SDL_GetClipboardText();
            if (cb_text) {
                script_delete_selection();
                size_t cb_len = strlen(cb_text);
                size_t cur_len = strlen(g_edit_script.body);
                if (cur_len + cb_len < sizeof(g_edit_script.body) - 1) {
                    memmove(&g_edit_script.body[g_script_cursor + cb_len],
                            &g_edit_script.body[g_script_cursor],
                            cur_len - g_script_cursor + 1);
                    memcpy(&g_edit_script.body[g_script_cursor], cb_text, cb_len);
                    g_script_cursor += (int)cb_len;
                    g_script_sel_start = g_script_sel_end = g_script_cursor;
                    ensure_cursor_visible();
                }
                SDL_free(cb_text);
            }
            return;
        }

        if (g_active_field == SCR_FIELD_BODY) {
            int cur_line, cur_col;
            get_cursor_line_col(g_edit_script.body, g_script_cursor, &cur_line, &cur_col);
            int total = get_total_script_lines(g_edit_script.body);

            if (ev->key.keysym.sym == SDLK_UP) {
                if (cur_line > 0) {
                    g_script_cursor = find_pos_at_line_col(g_edit_script.body, cur_line - 1, cur_col);
                    if (shift) g_script_sel_end = g_script_cursor;
                    else g_script_sel_start = g_script_sel_end = g_script_cursor;
                    ensure_cursor_visible();
                }
                return;
            } else if (ev->key.keysym.sym == SDLK_DOWN) {
                if (cur_line < total - 1) {
                    g_script_cursor = find_pos_at_line_col(g_edit_script.body, cur_line + 1, cur_col);
                    if (shift) g_script_sel_end = g_script_cursor;
                    else g_script_sel_start = g_script_sel_end = g_script_cursor;
                    ensure_cursor_visible();
                }
                return;
            } else if (ev->key.keysym.sym == SDLK_LEFT) {
                if (g_script_cursor > 0) {
                    g_script_cursor--;
                    if (shift) g_script_sel_end = g_script_cursor;
                    else g_script_sel_start = g_script_sel_end = g_script_cursor;
                    ensure_cursor_visible();
                }
                return;
            } else if (ev->key.keysym.sym == SDLK_RIGHT) {
                if (g_script_cursor < (int)strlen(g_edit_script.body)) {
                    g_script_cursor++;
                    if (shift) g_script_sel_end = g_script_cursor;
                    else g_script_sel_start = g_script_sel_end = g_script_cursor;
                    ensure_cursor_visible();
                }
                return;
            } else if (ev->key.keysym.sym == SDLK_PAGEUP || ev->key.keysym.sym == SDLK_KP_9) {
                g_script_scroll_line -= SCRIPT_VISIBLE_LINES;
                if (g_script_scroll_line < 0) g_script_scroll_line = 0;
                int n_line = cur_line - SCRIPT_VISIBLE_LINES;
                if (n_line < 0) n_line = 0;
                g_script_cursor = find_pos_at_line_col(g_edit_script.body, n_line, cur_col);
                return;
            } else if (ev->key.keysym.sym == SDLK_PAGEDOWN || ev->key.keysym.sym == SDLK_KP_3) {
                int max_scroll = total - SCRIPT_VISIBLE_LINES;
                if (max_scroll < 0) max_scroll = 0;
                g_script_scroll_line += SCRIPT_VISIBLE_LINES;
                if (g_script_scroll_line > max_scroll) g_script_scroll_line = max_scroll;
                int n_line = cur_line + SCRIPT_VISIBLE_LINES;
                if (n_line >= total) n_line = total - 1;
                g_script_cursor = find_pos_at_line_col(g_edit_script.body, n_line, cur_col);
                return;
            } else if (ev->key.keysym.sym == SDLK_HOME || ev->key.keysym.sym == SDLK_KP_7) {
                while (g_script_cursor > 0 && g_edit_script.body[g_script_cursor - 1] != '\n') {
                    g_script_cursor--;
                }
                ensure_cursor_visible();
                return;
            } else if (ev->key.keysym.sym == SDLK_END || ev->key.keysym.sym == SDLK_KP_1) {
                while (g_edit_script.body[g_script_cursor] && g_edit_script.body[g_script_cursor] != '\n') {
                    g_script_cursor++;
                }
                ensure_cursor_visible();
                return;
            } else if (ev->key.keysym.sym == SDLK_DELETE) {
                if (g_script_sel_start != g_script_sel_end) {
                    script_delete_selection();
                } else {
                    size_t len = strlen(g_edit_script.body);
                    if (g_script_cursor < (int)len) {
                        memmove(&g_edit_script.body[g_script_cursor],
                                &g_edit_script.body[g_script_cursor + 1],
                                len - g_script_cursor);
                    }
                }
                return;
            }
        }

        if (ev->key.keysym.sym == SDLK_BACKSPACE) {
            if (g_active_field == SCR_FIELD_NAME) {
                size_t len = strlen(g_edit_script.name);
                if (len > 0) g_edit_script.name[len - 1] = '\0';
            } else if (g_active_field == SCR_FIELD_CHAR_DEL) {
                char buf[16]; snprintf(buf, sizeof(buf), "%d", g_edit_script.char_delay_ms);
                size_t len = strlen(buf);
                if (len > 0) { buf[len - 1] = '\0'; g_edit_script.char_delay_ms = atoi(buf); }
            } else if (g_active_field == SCR_FIELD_NEXT_DEL) {
                char buf[16]; snprintf(buf, sizeof(buf), "%d", g_edit_script.next_delay_ms);
                size_t len = strlen(buf);
                if (len > 0) { buf[len - 1] = '\0'; g_edit_script.next_delay_ms = atoi(buf); }
            } else if (g_active_field == SCR_FIELD_CMD_DEL) {
                char buf[16]; snprintf(buf, sizeof(buf), "%d", g_edit_script.command_delay_ms);
                size_t len = strlen(buf);
                if (len > 0) { buf[len - 1] = '\0'; g_edit_script.command_delay_ms = atoi(buf); }
            } else if (g_active_field == SCR_FIELD_BODY) {
                if (g_script_sel_start != g_script_sel_end) {
                    script_delete_selection();
                    ensure_cursor_visible();
                } else if (g_script_cursor > 0) {
                    size_t len = strlen(g_edit_script.body);
                    memmove(&g_edit_script.body[g_script_cursor - 1],
                            &g_edit_script.body[g_script_cursor],
                            len - g_script_cursor + 1);
                    g_script_cursor--;
                    g_script_sel_start = g_script_sel_end = g_script_cursor;
                    ensure_cursor_visible();
                }
            }
        } else if (ev->key.keysym.sym == SDLK_RETURN) {
            if (g_active_field == SCR_FIELD_BODY) {
                size_t len = strlen(g_edit_script.body);
                if (len + 1 < sizeof(g_edit_script.body) - 1) {
                    memmove(&g_edit_script.body[g_script_cursor + 1],
                            &g_edit_script.body[g_script_cursor],
                            len - g_script_cursor + 1);
                    g_edit_script.body[g_script_cursor] = '\n';
                    g_script_cursor++;
                    ensure_cursor_visible();
                }
            } else {
                g_active_field = SCR_FIELD_NONE;
            }
        } else if (ev->key.keysym.sym == SDLK_TAB) {
            g_active_field = (g_active_field == SCR_FIELD_NAME) ? SCR_FIELD_CHAR_DEL :
                             (g_active_field == SCR_FIELD_CHAR_DEL) ? SCR_FIELD_NEXT_DEL :
                             (g_active_field == SCR_FIELD_NEXT_DEL) ? SCR_FIELD_CMD_DEL :
                             (g_active_field == SCR_FIELD_CMD_DEL) ? SCR_FIELD_BODY : SCR_FIELD_NAME;
        }
    } else if (ev->type == SDL_TEXTINPUT && ev->text.windowID == win_id) {
        SDL_Keymod mod = SDL_GetModState();
        if ((mod & KMOD_CTRL) || (mod & KMOD_GUI)) return;
        const char *txt = ev->text.text;
        if (g_active_field == SCR_FIELD_NAME) {
            size_t len = strlen(g_edit_script.name);
            strncat(g_edit_script.name, txt, sizeof(g_edit_script.name) - len - 1);
        } else if (g_active_field == SCR_FIELD_CHAR_DEL) {
            char buf[16]; snprintf(buf, sizeof(buf), "%d", g_edit_script.char_delay_ms);
            size_t len = strlen(buf);
            if (len < 5 && txt[0] >= '0' && txt[0] <= '9') {
                strncat(buf, txt, sizeof(buf) - len - 1);
                g_edit_script.char_delay_ms = atoi(buf);
            }
        } else if (g_active_field == SCR_FIELD_NEXT_DEL) {
            char buf[16]; snprintf(buf, sizeof(buf), "%d", g_edit_script.next_delay_ms);
            size_t len = strlen(buf);
            if (len < 5 && txt[0] >= '0' && txt[0] <= '9') {
                strncat(buf, txt, sizeof(buf) - len - 1);
                g_edit_script.next_delay_ms = atoi(buf);
            }
        } else if (g_active_field == SCR_FIELD_CMD_DEL) {
            char buf[16]; snprintf(buf, sizeof(buf), "%d", g_edit_script.command_delay_ms);
            size_t len = strlen(buf);
            if (len < 5 && txt[0] >= '0' && txt[0] <= '9') {
                strncat(buf, txt, sizeof(buf) - len - 1);
                g_edit_script.command_delay_ms = atoi(buf);
            }
        } else if (g_active_field == SCR_FIELD_BODY) {
            size_t t_len = strlen(txt);
            size_t cur_len = strlen(g_edit_script.body);
            if (cur_len + t_len < sizeof(g_edit_script.body) - 1) {
                memmove(&g_edit_script.body[g_script_cursor + t_len],
                        &g_edit_script.body[g_script_cursor],
                        cur_len - g_script_cursor + 1);
                memcpy(&g_edit_script.body[g_script_cursor], txt, t_len);
                g_script_cursor += (int)t_len;
                ensure_cursor_visible();
            }
        }
    } else if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.windowID == win_id) {
        int mx = ev->button.x;
        int my = ev->button.y;

        /* Click Lista Scripts */
        if (pt_in_rect(mx, my, 20, 60, 220, 480)) {
            int row = (my - 60) / 28 + g_list_scroll;
            if (row >= 0 && (size_t)row < g_script_list->count) {
                sync_to_script();
                sync_from_script(row);
                g_active_field = SCR_FIELD_NONE;
                g_recording_hotkey = false;
            }
            return;
        }

        /* Pulsante [+ New] */
        if (pt_in_rect(mx, my, 20, 550, 68, 28)) {
            trigger_flash_btn(SCR_BTN_NEW);
            sync_to_script();
            plato_script_t ns = {0};
            snprintf(ns.name, sizeof(ns.name), "Script %d", (int)g_script_list->count + 1);
            ns.enabled = true;
            ns.char_delay_ms = 250;
            ns.next_delay_ms = 500;
            ns.command_delay_ms = 0;
            snprintf(ns.body, sizeof(ns.body), "%s", PLATO_DEFAULT_AUTOLOGIN_SCRIPT);
            plato_scripts_add(g_script_list, &ns);
            sync_from_script((int)g_script_list->count - 1);
            trigger_flash_btn(SCR_BTN_NEW);
            return;
        }

        /* Pulsante [Clone] */
        if (pt_in_rect(mx, my, 94, 550, 68, 28)) {
            trigger_flash_btn(SCR_BTN_CLONE);
            if (g_script_list->count > 0 && g_current_idx >= 0) {
                sync_to_script();
                int new_idx = plato_scripts_clone(g_script_list, (size_t)g_current_idx);
                if (new_idx >= 0) {
                    sync_from_script(new_idx);
                    trigger_flash_btn(SCR_BTN_CLONE);
                }
            }
            return;
        }

        /* Pulsante [- Delete] */
        if (pt_in_rect(mx, my, 168, 550, 72, 28)) {
            trigger_flash_btn(SCR_BTN_DELETE);
            if (g_script_list->count > 1 && g_current_idx >= 0) {
                plato_scripts_delete(g_script_list, (size_t)g_current_idx);
                if (g_current_idx >= (int)g_script_list->count) g_current_idx = (int)g_script_list->count - 1;
                sync_from_script(g_current_idx);
                trigger_flash_btn(SCR_BTN_DELETE);
            }
            return;
        }

        /* Click & Drag Scrollbar Script */
        int sb_x = SCRIPT_BOX_X + SCRIPT_BOX_W - 14;
        int sb_y = SCRIPT_BOX_Y + 2;
        int sb_w = 12;
        int sb_h = SCRIPT_BOX_H - 4;
        if (pt_in_rect(mx, my, sb_x - 4, sb_y, sb_w + 8, sb_h)) {
            int total_lines = get_total_script_lines(g_edit_script.body);
            int max_scroll = total_lines - SCRIPT_VISIBLE_LINES;
            if (max_scroll > 0) {
                int thumb_h = (sb_h * SCRIPT_VISIBLE_LINES) / total_lines;
                if (thumb_h < 18) thumb_h = 18;
                int track_h = sb_h - thumb_h;
                if (track_h > 0) {
                    float ratio = (float)(my - sb_y - thumb_h / 2) / (float)track_h;
                    if (ratio < 0.0f) ratio = 0.0f;
                    if (ratio > 1.0f) ratio = 1.0f;
                    g_script_scroll_line = (int)(ratio * (float)max_scroll + 0.5f);
                    g_sb_dragging = true;
                    g_sb_drag_start_y = my;
                    g_sb_drag_start_scroll = g_script_scroll_line;
                }
            }
            return;
        }

        /* Click Campi Destri */
        if (pt_in_rect(mx, my, 340, 60, 320, 26)) { g_active_field = SCR_FIELD_NAME; return; }
        if (pt_in_rect(mx, my, 680, 60, 130, 26)) { g_edit_script.enabled = !g_edit_script.enabled; return; }

        /* Delays */
        if (pt_in_rect(mx, my, 340, 98, 80, 26)) { g_active_field = SCR_FIELD_CHAR_DEL; return; }
        if (pt_in_rect(mx, my, 480, 98, 80, 26)) { g_active_field = SCR_FIELD_NEXT_DEL; return; }
        if (pt_in_rect(mx, my, 620, 98, 80, 26)) { g_active_field = SCR_FIELD_CMD_DEL; return; }

        /* Pulsante Record Hotkey */
        if (pt_in_rect(mx, my, 570, 136, 90, 26)) {
            trigger_flash_btn(SCR_BTN_RECORD);
            g_recording_hotkey = true;
            return;
        }

        /* Pulsante Clear Hotkey */
        if (pt_in_rect(mx, my, 670, 136, 80, 26)) {
            trigger_flash_btn(SCR_BTN_CLEAR);
            g_edit_script.hotkey_modifiers = 0;
            g_edit_script.hotkey_key = 0;
            g_edit_script.hotkey_display[0] = '\0';
            g_recording_hotkey = false;
            trigger_flash_btn(SCR_BTN_CLEAR);
            return;
        }

        /* Click Script Text Area */
        if (pt_in_rect(mx, my, SCRIPT_BOX_X, SCRIPT_BOX_Y, SCRIPT_BOX_W, SCRIPT_BOX_H)) {
            g_active_field = SCR_FIELD_BODY;
            int rel_y = my - SCRIPT_BOX_Y - 4;
            int clicked_line = g_script_scroll_line + (rel_y / SCRIPT_LINE_HEIGHT);
            int rel_x = mx - SCRIPT_BOX_X - 8;
            int clicked_col = (rel_x > 0) ? (int)((float)rel_x / 8.5f) : 0;
            g_script_cursor = find_pos_at_line_col(g_edit_script.body, clicked_line, clicked_col);
            return;
        }

        /* Pulsanti Inferiori Destri */
        if (pt_in_rect(mx, my, 450, 550, 130, 32)) {
            /* Run Now */
            sync_to_script();
            if (g_run_cb && g_current_idx >= 0 && (size_t)g_current_idx < g_script_list->count) {
                g_run_cb(&g_script_list->scripts[g_current_idx], g_run_cb_user_data);
            }
            linux_scripts_destroy();
            return;
        }
        if (pt_in_rect(mx, my, 600, 550, 110, 32)) {
            /* Save */
            trigger_flash_btn(SCR_BTN_SAVE);
            sync_to_script();
            trigger_flash_btn(SCR_BTN_SAVE);
            return;
        }
        if (pt_in_rect(mx, my, 730, 550, 100, 32)) {
            /* Close */
            sync_to_script();
            linux_scripts_destroy();
            return;
        }

        g_active_field = SCR_FIELD_NONE;
    }
}

void linux_scripts_render(void) {
    linux_theme_colors_t th = linux_theme_current();
    if (!g_scr_win || !g_scr_ren) return;

    /* Sfondo finestra Dark Amber Plasma */
    SDL_SetRenderDrawColor(g_scr_ren, th.bg_dark.r, th.bg_dark.g, th.bg_dark.b, 255);
    SDL_RenderClear(g_scr_ren);

    /* Testata Top Bar Plasma 1:1 con Connection Profiles */
    draw_scr_rect(g_scr_ren, 0, 0, SCR_WIN_W, 44, th.bg_box.r, th.bg_box.g, th.bg_box.b, true, th.border.r, th.border.g, th.border.b);
    draw_scr_text(g_scr_ren, 20.0f, 27.0f, "MANAGE SCRIPTS", th.bright.r, th.bright.g, th.bright.b);
    draw_scr_text(g_scr_ren, 260.0f, 27.0f, "PLATO AUTOMATION & SCRIPT ENGINE", th.dim.r, th.dim.g, th.dim.b);

    /* Sfondo Riquadro Lista Sinistra */
    draw_scr_rect(g_scr_ren, 20, 60, 220, 480, th.bg_box.r, th.bg_box.g, th.bg_box.b, true, th.border.r, th.border.g, th.border.b);

    /* Render Elementi Lista Script */
    if (g_script_list) {
        for (size_t i = 0; i < g_script_list->count && i < 17; i++) {
            int y = 60 + (int)i * 28;
            bool is_sel = ((int)i == g_current_idx);

            if (is_sel) {
                draw_scr_rect(g_scr_ren, 22, y + 2, 216, 24, th.hover_bg.r, th.hover_bg.g, th.hover_bg.b, true, th.bright.r, th.bright.g, th.bright.b);
            }

            const plato_script_t *s = &g_script_list->scripts[i];
            char item_lbl[80];
            snprintf(item_lbl, sizeof(item_lbl), "%s %s", s->enabled ? "[✓]" : "[  ]", s->name);

            uint8_t fg_r = is_sel ? th.bright.r : (s->enabled ? th.normal.r : th.dim.r);
            uint8_t fg_g = is_sel ? th.bright.g : (s->enabled ? th.normal.g : th.dim.g);
            uint8_t fg_b = is_sel ? th.bright.b : (s->enabled ? th.normal.b : th.dim.b);

            draw_scr_text(g_scr_ren, 28.0f, (float)(y + 19), item_lbl, fg_r, fg_g, fg_b);

            if (s->hotkey_display[0]) {
                float kw = get_string_width_f(s->hotkey_display, (int)strlen(s->hotkey_display));
                draw_scr_text(g_scr_ren, 228.0f - kw, (float)(y + 19), s->hotkey_display, th.dim.r, th.dim.g, th.dim.b);
            }
        }
    }

    /* Pulsanti Sotto Lista */
    draw_scr_button(g_scr_ren, 20, 550, 68, 28, "+ New", false, is_btn_flashing(SCR_BTN_NEW));
    draw_scr_button(g_scr_ren, 94, 550, 68, 28, "Clone", false, is_btn_flashing(SCR_BTN_CLONE));
    draw_scr_button(g_scr_ren, 168, 550, 72, 28, "- Delete", false, is_btn_flashing(SCR_BTN_DELETE));

    /* DETTAGLI DELLO SCRIPT SELEZIONATO (COLONNA DESTRA) */
    if (g_current_idx >= 0 && g_script_list && (size_t)g_current_idx < g_script_list->count) {
        /* Riga 1: Name & Enabled */
        draw_scr_text(g_scr_ren, 260.0f, 78.0f, "Name:", th.dim.r, th.dim.g, th.dim.b);
        bool act_name = (g_active_field == SCR_FIELD_NAME);
        draw_scr_rect(g_scr_ren, 340, 60, 320, 26, th.bg_box.r, th.bg_box.g, th.bg_box.b, true, act_name ? th.bright.r : th.border.r, act_name ? th.bright.g : th.border.g, act_name ? th.bright.b : th.border.b);
        draw_scr_text(g_scr_ren, 348.0f, 78.0f, g_edit_script.name, th.bright.r, th.bright.g, th.bright.b);

        /* Checkbox Enabled */
        bool en = g_edit_script.enabled;
        draw_scr_rect(g_scr_ren, 680, 64, 18, 18, th.bg_box.r, th.bg_box.g, th.bg_box.b, true, th.border.r, th.border.g, th.border.b);
        if (en) {
            draw_scr_rect(g_scr_ren, 684, 68, 10, 10, th.bright.r, th.bright.g, th.bright.b, false, 0, 0, 0);
        }
        draw_scr_text(g_scr_ren, 706.0f, 78.0f, "Enabled in Menu", th.normal.r, th.normal.g, th.normal.b);

        /* Riga 2: Delays */
        draw_scr_text(g_scr_ren, 260.0f, 116.0f, "Delays:", th.dim.r, th.dim.g, th.dim.b);

        char num_buf[16];
        /* Char Delay */
        draw_scr_text(g_scr_ren, 340.0f, 116.0f, "Char:", th.dim.r, th.dim.g, th.dim.b);
        snprintf(num_buf, sizeof(num_buf), "%d", g_edit_script.char_delay_ms);
        bool act_c = (g_active_field == SCR_FIELD_CHAR_DEL);
        draw_scr_rect(g_scr_ren, 382, 98, 70, 26, th.bg_box.r, th.bg_box.g, th.bg_box.b, true, act_c ? th.bright.r : th.border.r, act_c ? th.bright.g : th.border.g, act_c ? th.bright.b : th.border.b);
        draw_scr_text(g_scr_ren, 390.0f, 116.0f, num_buf, th.bright.r, th.bright.g, th.bright.b);

        /* Next Delay */
        draw_scr_text(g_scr_ren, 470.0f, 116.0f, "Next:", th.dim.r, th.dim.g, th.dim.b);
        snprintf(num_buf, sizeof(num_buf), "%d", g_edit_script.next_delay_ms);
        bool act_n = (g_active_field == SCR_FIELD_NEXT_DEL);
        draw_scr_rect(g_scr_ren, 510, 98, 70, 26, th.bg_box.r, th.bg_box.g, th.bg_box.b, true, act_n ? th.bright.r : th.border.r, act_n ? th.bright.g : th.border.g, act_n ? th.bright.b : th.border.b);
        draw_scr_text(g_scr_ren, 518.0f, 116.0f, num_buf, th.bright.r, th.bright.g, th.bright.b);

        /* Command Delay */
        draw_scr_text(g_scr_ren, 600.0f, 116.0f, "Cmd:", th.dim.r, th.dim.g, th.dim.b);
        snprintf(num_buf, sizeof(num_buf), "%d", g_edit_script.command_delay_ms);
        bool act_m = (g_active_field == SCR_FIELD_CMD_DEL);
        draw_scr_rect(g_scr_ren, 638, 98, 70, 26, th.bg_box.r, th.bg_box.g, th.bg_box.b, true, act_m ? th.bright.r : th.border.r, act_m ? th.bright.g : th.border.g, act_m ? th.bright.b : th.border.b);
        draw_scr_text(g_scr_ren, 646.0f, 116.0f, num_buf, th.bright.r, th.bright.g, th.bright.b);

        /* Riga 3: Hotkey */
        draw_scr_text(g_scr_ren, 260.0f, 154.0f, "Hotkey:", th.dim.r, th.dim.g, th.dim.b);
        const char *hk_lbl = g_recording_hotkey ? ">> Press any key... <<" :
                             (g_edit_script.hotkey_display[0] ? g_edit_script.hotkey_display : "None");
        uint8_t hk_r = g_recording_hotkey ? th.bright.r : (g_edit_script.hotkey_display[0] ? th.bright.r : th.dim.r);
        uint8_t hk_g = g_recording_hotkey ? th.bright.g : (g_edit_script.hotkey_display[0] ? th.bright.g : th.dim.g);
        uint8_t hk_b = g_recording_hotkey ? th.bright.b : (g_edit_script.hotkey_display[0] ? th.bright.b : th.dim.b);
        draw_scr_rect(g_scr_ren, 340, 136, 210, 26, th.bg_box.r, th.bg_box.g, th.bg_box.b, true, g_recording_hotkey ? th.bright.r : th.border.r, g_recording_hotkey ? th.bright.g : th.border.g, g_recording_hotkey ? th.bright.b : th.border.b);
        draw_scr_text(g_scr_ren, 348.0f, 154.0f, hk_lbl, hk_r, hk_g, hk_b);

        draw_scr_button(g_scr_ren, 570, 136, 90, 26, g_recording_hotkey ? "Cancel" : "Record", g_recording_hotkey, is_btn_flashing(SCR_BTN_RECORD));
        draw_scr_button(g_scr_ren, 670, 136, 80, 26, "Clear", false, is_btn_flashing(SCR_BTN_CLEAR));

        /* Riga 4: Script Body Label */
        draw_scr_text(g_scr_ren, 260.0f, 192.0f, "Script Body:", th.bright.r, th.bright.g, th.bright.b);
        draw_scr_text(g_scr_ren, 360.0f, 192.0f, "Commands: wait_text, send, key, let, find_text, if..goto", th.dim.r, th.dim.g, th.dim.b);

        /* EDITOR MULTI-LINEA */
        bool act_script = (g_active_field == SCR_FIELD_BODY);
        draw_scr_rect(g_scr_ren, SCRIPT_BOX_X, SCRIPT_BOX_Y, SCRIPT_BOX_W, SCRIPT_BOX_H,
                      th.bg_box.r, th.bg_box.g, th.bg_box.b, true,
                      act_script ? th.bright.r : th.border.r,
                      act_script ? th.bright.g : th.border.g,
                      act_script ? th.bright.b : th.border.b);

        /* Render Linee di Testo */
        const char *text = g_edit_script.body;
        int line_idx = 0;
        int visible_row = 0;
        const char *line_start = text;

        int cur_line, cur_col;
        get_cursor_line_col(text, g_script_cursor, &cur_line, &cur_col);

        while (*line_start && visible_row < SCRIPT_VISIBLE_LINES) {
            const char *line_end = strchr(line_start, '\n');
            int len = line_end ? (int)(line_end - line_start) : (int)strlen(line_start);

            if (line_idx >= g_script_scroll_line) {
                float ly = (float)(SCRIPT_BOX_Y + 18 + visible_row * SCRIPT_LINE_HEIGHT);

                /* Estrai riga per il draw */
                char buf[512];
                int copy_len = len < 510 ? len : 510;
                memcpy(buf, line_start, copy_len);
                buf[copy_len] = '\0';

                /* Sintassi evidenziata */
                uint8_t cr = 240, cg = 180, cb = 60;
                if (buf[0] == '#') { cr = 140; cg = 100; cb = 30; }
                else if (buf[0] == '@') { cr = 255; cg = 220; cb = 60; }

                /* Disegna evidenziazione visiva della selezione */
                int line_start_idx = (int)(line_start - text);
                int line_end_idx = line_start_idx + len;
                int s_min = g_script_sel_start < g_script_sel_end ? g_script_sel_start : g_script_sel_end;
                int s_max = g_script_sel_start < g_script_sel_end ? g_script_sel_end : g_script_sel_start;
                if (s_min != s_max && s_min < line_end_idx && s_max > line_start_idx) {
                    int o_start = s_min > line_start_idx ? s_min : line_start_idx;
                    int o_end = s_max < line_end_idx ? s_max : line_end_idx;
                    float sel_x1 = (float)(SCRIPT_BOX_X + 10) + get_string_width_f(line_start, o_start - line_start_idx);
                    float sel_w = get_string_width_f(line_start + (o_start - line_start_idx), o_end - o_start);
                    draw_scr_rect(g_scr_ren, (int)sel_x1, (int)(ly - 14), (int)sel_w + 1, 17, th.hover_bg.r, th.hover_bg.g, th.hover_bg.b, false, 0, 0, 0);
                }
                draw_scr_text(g_scr_ren, (float)(SCRIPT_BOX_X + 10), ly, buf, th.normal.r, th.normal.g, th.normal.b);

                /* Cursore lampeggiante / visibile */
                if (act_script && line_idx == cur_line) {
                    float cx = (float)(SCRIPT_BOX_X + 10) + get_string_width_f(line_start, cur_col);
                    SDL_Rect cr_rc = { (int)cx, (int)(ly - 14), 2, 17 };
                    SDL_SetRenderDrawColor(g_scr_ren, th.bright.r, th.bright.g, th.bright.b, 255);
                    SDL_RenderFillRect(g_scr_ren, &cr_rc);
                }
                visible_row++;
            }

            line_idx++;
            if (!line_end) break;
            line_start = line_end + 1;
        }

        /* Se il cursore è su una nuova linea vuota in fondo */
        if (act_script && cur_line >= line_idx && visible_row < SCRIPT_VISIBLE_LINES) {
            float ly = (float)(SCRIPT_BOX_Y + 18 + visible_row * SCRIPT_LINE_HEIGHT);
            SDL_Rect cr_rc = { SCRIPT_BOX_X + 10, (int)(ly - 14), 2, 17 };
            SDL_SetRenderDrawColor(g_scr_ren, th.bright.r, th.bright.g, th.bright.b, 255);
            SDL_RenderFillRect(g_scr_ren, &cr_rc);
        }

        /* SCROLLBAR DRAG-AND-DROP PER L'EDITOR */
        int total_lines = get_total_script_lines(text);
        if (total_lines > SCRIPT_VISIBLE_LINES) {
            int sb_x = SCRIPT_BOX_X + SCRIPT_BOX_W - 14;
            int sb_y = SCRIPT_BOX_Y + 2;
            int sb_h = SCRIPT_BOX_H - 4;

            draw_scr_rect(g_scr_ren, sb_x, sb_y, 12, sb_h, th.bg_dark.r, th.bg_dark.g, th.bg_dark.b, true, th.border.r, th.border.g, th.border.b);

            int thumb_h = (sb_h * SCRIPT_VISIBLE_LINES) / total_lines;
            if (thumb_h < 18) thumb_h = 18;

            int max_scroll = total_lines - SCRIPT_VISIBLE_LINES;
            int thumb_y = sb_y + (g_script_scroll_line * (sb_h - thumb_h)) / max_scroll;

            draw_scr_rect(g_scr_ren, sb_x + 1, thumb_y, 10, thumb_h, th.dim.r, th.dim.g, th.dim.b, true, th.bright.r, th.bright.g, th.bright.b);
        }

        

        /* Pulsanti Inferiori Destri */
        draw_scr_button(g_scr_ren, 450, 550, 130, 32, "Run Now", false, is_btn_flashing(SCR_BTN_RUN));
        draw_scr_button(g_scr_ren, 600, 550, 110, 32, "Save", false, is_btn_flashing(SCR_BTN_SAVE));
        draw_scr_button(g_scr_ren, 730, 550, 100, 32, "Close", false, false);
    }

    SDL_RenderPresent(g_scr_ren);
}
