/*
 * PlatoLives - Profile Manager Dialog for Linux (SDL2)
 * High-Fidelity Retro Look with Dynamic Phosphor Theming
 */

#include "linux_profiles.h"
#include "linux_theme.h"
#include "plato/plato_script.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define PROF_WIN_W 980
#define PROF_WIN_H 650

#define SCRIPT_BOX_X 340
#define SCRIPT_BOX_Y 340
#define SCRIPT_BOX_W 620
#define SCRIPT_BOX_H 215
#define SCRIPT_VISIBLE_LINES 10
#define SCRIPT_LINE_HEIGHT 20

#include "plato/stb_truetype.h"
#include "fonts/roboto_font_data.h"

static SDL_Window *g_prof_win = NULL;
static SDL_Renderer *g_prof_ren = NULL;

static stbtt_bakedchar g_prof_cdata[96];
static unsigned char *g_prof_atlas = NULL;
static SDL_Texture *g_prof_atlas_tex = NULL;
static bool g_prof_font_ready = false;

static plato_profile_list_t *g_list = NULL;
static int g_current_idx = 0;
static plato_profile_t g_edit_profile;

static linux_profile_connect_cb g_connect_cb = NULL;
static void *g_cb_user_data = NULL;

typedef enum {
    FIELD_NONE = 0,
    FIELD_NAME,
    FIELD_HOST,
    FIELD_PORT,
    FIELD_SCRIPT
} prof_field_t;

static prof_field_t g_active_field = FIELD_NONE;
static int g_script_cursor = 0;
static int g_script_sel_start = 0;
static int g_script_sel_end = 0;
static bool g_script_text_dragging = false;
static int g_script_scroll_line = 0;
static bool g_sb_dragging = false;

static void profile_delete_selection(void) {
    if (g_script_sel_start == g_script_sel_end) return;
    int a = g_script_sel_start < g_script_sel_end ? g_script_sel_start : g_script_sel_end;
    int b = g_script_sel_start < g_script_sel_end ? g_script_sel_end : g_script_sel_start;
    size_t len = strlen(g_edit_profile.startup_script);
    if (b > (int)len) b = (int)len;
    if (a < 0) a = 0;
    memmove(&g_edit_profile.startup_script[a], &g_edit_profile.startup_script[b], len - b + 1);
    g_script_cursor = a;
    g_script_sel_start = g_script_sel_end = a;
}
static int g_sb_drag_start_y = 0;
static int g_sb_drag_start_scroll = 0;

static const int g_decay_ms_vals[6] = { 100, 200, 500, 1000, 2000, 5000 };
static const int g_crt_decay_ms_vals[6] = { 20, 200, 500, 1000, 2000, 5000 };
static const int g_tek_baud_vals[9] = { 0, 110, 150, 300, 600, 1200, 2400, 4800, 9600 };

typedef enum {
    PROF_BTN_NONE = 0,
    PROF_BTN_NEW,
    PROF_BTN_DELETE,
    PROF_BTN_DEFAULT,
    PROF_BTN_SAVE
} prof_flash_btn_t;

static prof_flash_btn_t g_flash_btn = PROF_BTN_NONE;
static uint32_t g_flash_end_time = 0;

static void trigger_prof_flash(prof_flash_btn_t btn) {
    g_flash_btn = btn;
    g_flash_end_time = SDL_GetTicks() + 150;
}

static bool is_prof_btn_flashing(prof_flash_btn_t btn) {
    if (g_flash_btn == btn && SDL_GetTicks() < g_flash_end_time) return true;
    return false;
}

static void init_prof_font(void) {
    if (g_prof_font_ready || !g_prof_ren) return;
    if (!g_prof_atlas) {
        g_prof_atlas = (unsigned char *)malloc(512 * 512);
    }
    stbtt_BakeFontBitmap(g_roboto_ttf, 0, 16.0f, g_prof_atlas, 512, 512, 32, 96, g_prof_cdata);

    uint32_t *rgba = (uint32_t *)malloc(512 * 512 * sizeof(uint32_t));
    if (!rgba) return;

    for (int i = 0; i < 512 * 512; i++) {
        uint8_t a = g_prof_atlas[i];
        rgba[i] = ((uint32_t)a << 24) | 0x00FFFFFF;
    }

    g_prof_atlas_tex = SDL_CreateTexture(g_prof_ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 512, 512);
    SDL_UpdateTexture(g_prof_atlas_tex, NULL, rgba, 512 * sizeof(uint32_t));
    SDL_SetTextureBlendMode(g_prof_atlas_tex, SDL_BLENDMODE_BLEND);

    free(rgba);
    g_prof_font_ready = true;
}

static void draw_prof_text(SDL_Renderer *ren, float x, float y, const char *str, uint8_t r, uint8_t g, uint8_t b) {
    if (!g_prof_atlas_tex || !str) return;
    SDL_SetTextureColorMod(g_prof_atlas_tex, r, g, b);

    while (*str) {
        char c = *str++;
        if ((unsigned char)c < 32 || (unsigned char)c >= 128) continue;
        const stbtt_bakedchar *bc = &g_prof_cdata[(unsigned char)c - 32];

        SDL_Rect src = { bc->x0, bc->y0, bc->x1 - bc->x0, bc->y1 - bc->y0 };
        SDL_Rect dst = { (int)(x + bc->xoff + 0.5f), (int)(y + bc->yoff + 0.5f), src.w, src.h };

        SDL_RenderCopy(ren, g_prof_atlas_tex, &src, &dst);
        x += bc->xadvance;
    }
}

static float get_string_width_f(const char *str, int count) {
    if (!str) return 0.0f;
    float w = 0.0f;
    for (int i = 0; i < count && str[i]; i++) {
        char c = str[i];
        if ((unsigned char)c < 32 || (unsigned char)c >= 128) { w += 8.0f; continue; }
        const stbtt_bakedchar *bc = &g_prof_cdata[(unsigned char)c - 32];
        w += bc->xadvance;
    }
    return w;
}

static void draw_prof_rect(SDL_Renderer *ren, int x, int y, int w, int h,
                           uint8_t r, uint8_t g, uint8_t b, bool outline,
                           uint8_t or, uint8_t og, uint8_t ob) {
    SDL_Rect rc = { x, y, w, h };
    SDL_SetRenderDrawColor(ren, r, g, b, 255);
    SDL_RenderFillRect(ren, &rc);
    if (outline) {
        SDL_SetRenderDrawColor(ren, or, og, ob, 255);
        SDL_RenderDrawRect(ren, &rc);
    }
}

static void draw_prof_button(SDL_Renderer *ren, int x, int y, int w, int h, const char *label, bool active) {
    linux_theme_colors_t th = linux_theme_current();
    float tw = get_string_width_f(label, (int)strlen(label));
    float tx = (w > tw) ? (float)x + ((float)w - tw) * 0.5f : (float)(x + 4);
    if (active) {
        draw_prof_rect(ren, x, y, w, h, th.hover_bg.r, th.hover_bg.g, th.hover_bg.b, true, th.bright.r, th.bright.g, th.bright.b);
        draw_prof_text(ren, tx, (float)(y + h - 8), label, th.bright.r, th.bright.g, th.bright.b);
    } else {
        draw_prof_rect(ren, x, y, w, h, th.bg_box.r, th.bg_box.g, th.bg_box.b, true, th.border.r, th.border.g, th.border.b);
        draw_prof_text(ren, tx, (float)(y + h - 8), label, th.normal.r, th.normal.g, th.normal.b);
    }
}

static void draw_prof_button_ex(SDL_Renderer *ren, int x, int y, int w, int h, const char *label, bool active, bool flashing) {
    linux_theme_colors_t th = linux_theme_current();
    if (flashing) {
        draw_prof_rect(ren, x, y, w, h, th.bright.r, th.bright.g, th.bright.b, true, 255, 255, 255);
        draw_prof_text(ren, (float)(x + 8), (float)(y + h - 8), label, 0, 0, 0);
    } else {
        draw_prof_button(ren, x, y, w, h, label, active);
    }
}

static void sync_from_profile(int idx) {
    if (!g_list || idx < 0 || (size_t)idx >= g_list->count) return;
    g_current_idx = idx;
    g_edit_profile = g_list->profiles[idx];
    g_script_cursor = (int)strlen(g_edit_profile.startup_script);
    g_script_scroll_line = 0;
}

static void sync_to_profile(void) {
    if (!g_list || g_current_idx < 0 || (size_t)g_current_idx >= g_list->count) return;
    g_list->profiles[g_current_idx] = g_edit_profile;
    plato_profiles_save(g_list);
}

void linux_profiles_show(plato_profile_list_t *list, linux_profile_connect_cb connect_cb, void *user_data) {
    g_list = list;
    g_connect_cb = connect_cb;
    g_cb_user_data = user_data;

    if (!g_prof_win) {
        g_prof_win = SDL_CreateWindow(
            "Manage Connection Profiles",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            PROF_WIN_W, PROF_WIN_H,
            SDL_WINDOW_SHOWN
        );
        if (!g_prof_win) return;

        g_prof_ren = SDL_CreateRenderer(g_prof_win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!g_prof_ren) {
            g_prof_ren = SDL_CreateRenderer(g_prof_win, -1, SDL_RENDERER_SOFTWARE);
        }
        init_prof_font();
        SDL_StartTextInput();
    } else {
        SDL_RaiseWindow(g_prof_win);
    }

    sync_from_profile(0);
}

bool linux_profiles_is_open(void) {
    return (g_prof_win != NULL);
}

void linux_profiles_destroy(void) {
    /* Manteniamo attivo TextInput per la sessione terminale */
    SDL_StartTextInput();
    if (g_prof_atlas_tex) {
        SDL_DestroyTexture(g_prof_atlas_tex);
        g_prof_atlas_tex = NULL;
    }
    if (g_prof_atlas) {
        free(g_prof_atlas);
        g_prof_atlas = NULL;
    }
    g_prof_font_ready = false;
    if (g_prof_ren) {
        SDL_DestroyRenderer(g_prof_ren);
        g_prof_ren = NULL;
    }
    if (g_prof_win) {
        SDL_DestroyWindow(g_prof_win);
        g_prof_win = NULL;
    }
}

static bool pt_in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return (x >= rx && x <= rx + rw && y >= ry && y <= ry + rh);
}

static int get_total_script_lines(const char *str) {
    if (!str || !*str) return 1;
    int lines = 1;
    while (*str) {
        if (*str == '\n') lines++;
        str++;
    }
    return lines;
}

static void get_cursor_line_col(const char *str, int cursor_pos, int *out_line, int *out_col) {
    int line = 0;
    int col = 0;
    for (int i = 0; i < cursor_pos && str[i]; i++) {
        if (str[i] == '\n') {
            line++;
            col = 0;
        } else {
            col++;
        }
    }
    *out_line = line;
    *out_col = col;
}

static int find_pos_at_line_col(const char *str, int target_line, int target_col) {
    int line = 0;
    int col = 0;
    int i = 0;
    while (str[i]) {
        if (line == target_line) {
            int line_start = i;
            while (str[i] && str[i] != '\n' && col < target_col) {
                i++;
                col++;
            }
            return i;
        }
        if (str[i] == '\n') {
            line++;
            col = 0;
        }
        i++;
    }
    return i;
}

void linux_profiles_handle_event(const SDL_Event *ev) {
    if (!ev || !g_prof_win) return;
    uint32_t win_id = SDL_GetWindowID(g_prof_win);

    if (ev->type == SDL_WINDOWEVENT && ev->window.windowID == win_id) {
        if (ev->window.event == SDL_WINDOWEVENT_CLOSE) {
            linux_profiles_destroy();
            return;
        }
    }

    /* Consumo esclusivo: ignora eventi non destinati a questa finestra modale */
    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.windowID != win_id) return;
    if (ev->type == SDL_MOUSEBUTTONUP && ev->button.windowID != win_id) return;
    if (ev->type == SDL_MOUSEMOTION && ev->motion.windowID != win_id) return;
    if (ev->type == SDL_MOUSEWHEEL && ev->wheel.windowID != win_id) return;
    if (ev->type == SDL_KEYDOWN && ev->key.windowID != win_id) return;
    if (ev->type == SDL_KEYUP && ev->key.windowID != win_id) return;
    if (ev->type == SDL_TEXTINPUT && ev->text.windowID != win_id) return;

    if (ev->type == SDL_MOUSEBUTTONUP) {
        if (ev->button.button == SDL_BUTTON_LEFT) {
            g_sb_dragging = false;
            g_script_text_dragging = false;
        }
    }

    if (ev->type == SDL_MOUSEMOTION) {
        if (g_script_text_dragging && (ev->motion.state & SDL_BUTTON_LMASK)) {
            int rel_y = ev->motion.y - SCRIPT_BOX_Y - 4;
            int clicked_line = g_script_scroll_line + (rel_y / SCRIPT_LINE_HEIGHT);
            int rel_x = ev->motion.x - SCRIPT_BOX_X - 8;
            int clicked_col = (rel_x > 0) ? (int)((float)rel_x / 8.5f) : 0;
            g_script_cursor = find_pos_at_line_col(g_edit_profile.startup_script, clicked_line, clicked_col);
            g_script_sel_end = g_script_cursor;
            return;
        }
        if (g_sb_dragging) {
            int total_lines = get_total_script_lines(g_edit_profile.startup_script);
            int max_scroll = total_lines - SCRIPT_VISIBLE_LINES;
            if (max_scroll > 0) {
                int sb_h = SCRIPT_BOX_H - 4;
                int thumb_h = (sb_h * SCRIPT_VISIBLE_LINES) / total_lines;
                if (thumb_h < 18) thumb_h = 18;
                int track_h = sb_h - thumb_h;
                if (track_h > 0) {
                    int dy = ev->motion.y - g_sb_drag_start_y;
                    float lines_per_pixel = (float)max_scroll / (float)track_h;
                    int new_scroll = g_sb_drag_start_scroll + (int)(dy * lines_per_pixel + 0.5f);
                    if (new_scroll < 0) new_scroll = 0;
                    if (new_scroll > max_scroll) new_scroll = max_scroll;
                    g_script_scroll_line = new_scroll;
                }
            }
        }
    }

    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        int mx = ev->button.x;
        int my = ev->button.y;

        /* Click Lista Profili Sinistra */
        if (pt_in_rect(mx, my, 20, 60, 220, 500)) {
            int clicked_idx = (my - 62) / 28;
            if (clicked_idx >= 0 && (size_t)clicked_idx < g_list->count) {
                sync_to_profile();
                sync_from_profile(clicked_idx);
            }
            return;
        }

        /* Pulsante [+ New] */
        if (pt_in_rect(mx, my, 20, 575, 100, 28)) {
            trigger_prof_flash(PROF_BTN_NEW);
            sync_to_profile();
            plato_profile_t np;
            memset(&np, 0, sizeof(np));
            snprintf(np.name, sizeof(np.name), "New Profile %zu", g_list->count + 1);
            snprintf(np.host, sizeof(np.host), "cyber1.org");
            np.port = 8005;
            np.display_mode = 0;
            np.persistence_ms = 500;
            np.plasma_distortion = 2;
            np.crt_persistence_ms = 20;
            np.phosphor_color = 0;
            np.crt_distortion = 0;
            np.pdp1_font = 0;
            np.pdp1_refresh_rate = 45;
            np.pdp1_preset = 0;
            np.tek_baud_rate = 2400;
            np.tek_erase_flash = true;
            np.tek_accurate_bg = true;
            np.startup_script_enabled = false;
            snprintf(np.startup_script, sizeof(np.startup_script), "%s", PLATO_DEFAULT_AUTOLOGIN_SCRIPT);
            plato_profiles_add(g_list, &np);
            sync_from_profile((int)g_list->count - 1);
            return;
        }

        /* Pulsante [- Delete] */
        if (pt_in_rect(mx, my, 130, 575, 100, 28)) {
            trigger_prof_flash(PROF_BTN_DELETE);
            if (g_list->count > 1) {
                plato_profiles_delete(g_list, (size_t)g_current_idx);
                if (g_current_idx >= (int)g_list->count) g_current_idx = (int)g_list->count - 1;
                sync_from_profile(g_current_idx);
            }
            return;
        }

        /* Click Campi Destri */
        if (pt_in_rect(mx, my, 340, 60, 320, 26)) { g_active_field = FIELD_NAME; return; }
        if (pt_in_rect(mx, my, 340, 94, 320, 26)) { g_active_field = FIELD_HOST; return; }
        if (pt_in_rect(mx, my, 340, 128, 110, 26)) { g_active_field = FIELD_PORT; return; }

        /* Phosphor Color a X=540, Y=128 */
        if (g_edit_profile.display_mode != 5) {
            int ph_x = 540;
            for (int i = 0; i < 4; i++) {
                if (pt_in_rect(mx, my, ph_x, 128, 85, 26)) {
                    g_edit_profile.phosphor_color = i;
                    return;
                }
                ph_x += 90;
            }
        }

        /* 7 Display Modes: width 84, step 89, X=340..958 */
        int mode_x = 340;
        for (int m = 0; m < 7; m++) {
            if (pt_in_rect(mx, my, mode_x, 162, 84, 26)) {
                g_edit_profile.display_mode = m;
                if (m == 5 && g_edit_profile.tek_baud_rate <= 0) {
                    g_edit_profile.tek_baud_rate = 2400; /* Auto-proposta 2400 Baud */
                }
                if (m == 6 && g_edit_profile.pdp1_refresh_rate <= 0) {
                    g_edit_profile.pdp1_refresh_rate = 45; /* Auto-proposta 45 Hz */
                }
                return;
            }
            mode_x += 89;
        }

        int mode = g_edit_profile.display_mode;
        if (mode == 0 || mode == 2) {
            /* Plasma Persistence at Y=196 */
            int px = 340;
            for (int i = 0; i < 6; i++) {
                if (pt_in_rect(mx, my, px, 196, 66, 24)) {
                    g_edit_profile.persistence_ms = g_decay_ms_vals[i];
                    return;
                }
                px += 72;
            }
            /* Plasma Distortion at Y=226 */
            int dx = 340;
            for (int i = 0; i < 3; i++) {
                if (pt_in_rect(mx, my, dx, 226, 110, 24)) {
                    g_edit_profile.plasma_distortion = i;
                    return;
                }
                dx += 116;
            }
        } else if (mode == 4) {
            /* CRT Beam at Y=196 */
            int bx = 340;
            for (int i = 0; i < 3; i++) {
                if (pt_in_rect(mx, my, bx, 196, 110, 24)) {
                    g_edit_profile.crt_beam_level = i;
                    return;
                }
                bx += 116;
            }
            /* CRT Persistence at Y=226 */
            int px = 340;
            for (int i = 0; i < 6; i++) {
                if (pt_in_rect(mx, my, px, 226, 66, 24)) {
                    g_edit_profile.crt_persistence_ms = g_crt_decay_ms_vals[i];
                    return;
                }
                px += 72;
            }
            /* CRT Distortion at Y=256 */
            int dx = 340;
            for (int i = 0; i < 3; i++) {
                if (pt_in_rect(mx, my, dx, 256, 110, 24)) {
                    g_edit_profile.crt_distortion = i;
                    return;
                }
                dx += 116;
            }
        } else if (mode == 5) {
            /* Tektronix Speed at Y=196 (9 tasti: Instant, 110..9600) */
            int tx = 340;
            for (int i = 0; i < 9; i++) {
                if (pt_in_rect(mx, my, tx, 196, 64, 24)) {
                    g_edit_profile.tek_baud_rate = g_tek_baud_vals[i];
                    return;
                }
                tx += 69;
            }
            /* Tektronix Erase Flash & Accurate BG at Y=234 */
            if (pt_in_rect(mx, my, 340, 234, 150, 22)) {
                g_edit_profile.tek_erase_flash = !g_edit_profile.tek_erase_flash;
                return;
            }
            if (pt_in_rect(mx, my, 510, 234, 210, 22)) {
                g_edit_profile.tek_accurate_bg = !g_edit_profile.tek_accurate_bg;
                return;
            }
        } else if (mode == 6) {
            /* PDP-1 Font at Y=196 */
            if (pt_in_rect(mx, my, 340, 196, 150, 24)) {
                g_edit_profile.pdp1_font = 0;
                return;
            }
            if (pt_in_rect(mx, my, 500, 196, 150, 24)) {
                g_edit_profile.pdp1_font = 1;
                return;
            }

            /* PDP-1 Refresh Rate at Y=226 */
            const int pdp1_hz_vals[] = { 60, 45, 30, 15, 10, 5 };
            int rx = 340;
            for (int i = 0; i < 6; i++) {
                if (pt_in_rect(mx, my, rx, 226, 66, 24)) {
                    g_edit_profile.pdp1_refresh_rate = pdp1_hz_vals[i];
                    return;
                }
                rx += 72;
            }

            /* PDP-1 Preset at Y=256 */
            int pr_x = 340;
            for (int i = 0; i < 3; i++) {
                if (pt_in_rect(mx, my, pr_x, 256, 140, 24)) {
                    g_edit_profile.pdp1_preset = i;
                    return;
                }
                pr_x += 150;
            }
        }

        /* Checkboxes a Y=292 */
        if (pt_in_rect(mx, my, 340, 292, 140, 22)) { g_edit_profile.full_screen = !g_edit_profile.full_screen; return; }
        if (pt_in_rect(mx, my, 500, 292, 150, 22)) {
            g_edit_profile.is_default = true;
            plato_profiles_set_default(g_list, (size_t)g_current_idx);
            return;
        }
        if (pt_in_rect(mx, my, 670, 292, 180, 22)) { g_edit_profile.startup_script_enabled = !g_edit_profile.startup_script_enabled; return; }

        /* Click Script Text Area */
        if (pt_in_rect(mx, my, SCRIPT_BOX_X, SCRIPT_BOX_Y, SCRIPT_BOX_W, SCRIPT_BOX_H)) {
            g_active_field = FIELD_SCRIPT;
            int rel_y = my - SCRIPT_BOX_Y - 4;
            int clicked_line = g_script_scroll_line + (rel_y / SCRIPT_LINE_HEIGHT);
            int rel_x = mx - SCRIPT_BOX_X - 8;
            int clicked_col = (rel_x > 0) ? (int)((float)rel_x / 8.5f) : 0;
            g_script_cursor = find_pos_at_line_col(g_edit_profile.startup_script, clicked_line, clicked_col);
            SDL_Keymod mod = SDL_GetModState();
            if (!(mod & KMOD_SHIFT)) g_script_sel_start = g_script_cursor;
            g_script_sel_end = g_script_cursor;
            g_script_text_dragging = true;
            return;
        }

        /* Pulsanti Inferiori a Y=575 */
        if (pt_in_rect(mx, my, 340, 575, 130, 32)) {
            g_edit_profile.is_default = true;
            plato_profiles_set_default(g_list, (size_t)g_current_idx);
            trigger_prof_flash(PROF_BTN_DEFAULT);
            return;
        }
        if (pt_in_rect(mx, my, 485, 575, 100, 32)) {
            sync_to_profile();
            trigger_prof_flash(PROF_BTN_SAVE);
            return;
        }
        if (pt_in_rect(mx, my, 600, 575, 140, 32)) {
            sync_to_profile();
            if (g_connect_cb && g_current_idx >= 0 && (size_t)g_current_idx < g_list->count) {
                g_connect_cb(&g_list->profiles[g_current_idx], g_cb_user_data);
            }
            linux_profiles_destroy();
            return;
        }
        if (pt_in_rect(mx, my, 755, 575, 100, 32)) {
            linux_profiles_destroy();
            return;
        }

        g_active_field = FIELD_NONE;
    }

    if (ev->type == SDL_TEXTINPUT) {
        SDL_Keymod mod = SDL_GetModState();
        if ((mod & KMOD_CTRL) || (mod & KMOD_GUI)) return;
        if (g_active_field == FIELD_NAME) {
            strncat(g_edit_profile.name, ev->text.text, sizeof(g_edit_profile.name) - strlen(g_edit_profile.name) - 1);
        } else if (g_active_field == FIELD_HOST) {
            strncat(g_edit_profile.host, ev->text.text, sizeof(g_edit_profile.host) - strlen(g_edit_profile.host) - 1);
        } else if (g_active_field == FIELD_PORT) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d%s", g_edit_profile.port, ev->text.text);
            g_edit_profile.port = atoi(buf);
        } else if (g_active_field == FIELD_SCRIPT) {
            size_t len = strlen(g_edit_profile.startup_script);
            size_t ins_len = strlen(ev->text.text);
            if (len + ins_len < sizeof(g_edit_profile.startup_script) - 1) {
                memmove(&g_edit_profile.startup_script[g_script_cursor + ins_len],
                        &g_edit_profile.startup_script[g_script_cursor],
                        len - g_script_cursor + 1);
                memcpy(&g_edit_profile.startup_script[g_script_cursor], ev->text.text, ins_len);
                g_script_cursor += (int)ins_len;
            }
        }
    }

    if (ev->type == SDL_KEYDOWN) {
        SDL_Keycode k = ev->key.keysym.sym;
        if (k == SDLK_ESCAPE) {
            linux_profiles_destroy();
            return;
        }

        SDL_Keymod mod = SDL_GetModState();
        bool ctrl = (mod & KMOD_CTRL) != 0;
        bool gui  = (mod & KMOD_GUI) != 0;

        bool shift = (mod & KMOD_SHIFT) != 0;

        /* Seleziona Tutto (Ctrl+A / Cmd+A) */
        if ((ctrl || gui) && k == SDLK_a && g_active_field == FIELD_SCRIPT) {
            g_script_sel_start = 0;
            g_script_sel_end = (int)strlen(g_edit_profile.startup_script);
            g_script_cursor = g_script_sel_end;
            return;
        }

        /* Copia (Ctrl+C) e Taglia (Ctrl+X) */
        if ((ctrl || gui) && (k == SDLK_c || k == SDLK_x)) {
            if (g_active_field == FIELD_SCRIPT) {
                if (g_script_sel_start != g_script_sel_end) {
                    int a = g_script_sel_start < g_script_sel_end ? g_script_sel_start : g_script_sel_end;
                    int b = g_script_sel_start < g_script_sel_end ? g_script_sel_end : g_script_sel_start;
                    char tmp[sizeof(g_edit_profile.startup_script)];
                    int clen = b - a;
                    if (clen > (int)sizeof(tmp) - 1) clen = (int)sizeof(tmp) - 1;
                    memcpy(tmp, &g_edit_profile.startup_script[a], clen);
                    tmp[clen] = 0;
                    SDL_SetClipboardText(tmp);
                    if (k == SDLK_x) profile_delete_selection();
                } else {
                    SDL_SetClipboardText(g_edit_profile.startup_script);
                }
                return;
            } else if (g_active_field == FIELD_NAME) {
                SDL_SetClipboardText(g_edit_profile.name);
                return;
            } else if (g_active_field == FIELD_HOST) {
                SDL_SetClipboardText(g_edit_profile.host);
                return;
            }
        }

        /* Incolla (Ctrl+V / Cmd+V) */
        if ((ctrl || gui) && k == SDLK_v) {
            if (g_active_field == FIELD_SCRIPT) {
                char *cb_text = SDL_GetClipboardText();
                if (cb_text) {
                    profile_delete_selection();
                    size_t cb_len = strlen(cb_text);
                    size_t cur_len = strlen(g_edit_profile.startup_script);
                    if (cur_len + cb_len < sizeof(g_edit_profile.startup_script) - 1) {
                        memmove(&g_edit_profile.startup_script[g_script_cursor + cb_len],
                                &g_edit_profile.startup_script[g_script_cursor],
                                cur_len - g_script_cursor + 1);
                        memcpy(&g_edit_profile.startup_script[g_script_cursor], cb_text, cb_len);
                        g_script_cursor += (int)cb_len;
                        g_script_sel_start = g_script_sel_end = g_script_cursor;
                    }
                    SDL_free(cb_text);
                }
                return;
            } else if (g_active_field == FIELD_NAME) {
                char *cb = SDL_GetClipboardText();
                if (cb) {
                    strncat(g_edit_profile.name, cb, sizeof(g_edit_profile.name) - strlen(g_edit_profile.name) - 1);
                    SDL_free(cb);
                }
                return;
            } else if (g_active_field == FIELD_HOST) {
                char *cb = SDL_GetClipboardText();
                if (cb) {
                    strncat(g_edit_profile.host, cb, sizeof(g_edit_profile.host) - strlen(g_edit_profile.host) - 1);
                    SDL_free(cb);
                }
                return;
            }
        }

        if (g_active_field == FIELD_NAME && k == SDLK_BACKSPACE) {
            size_t l = strlen(g_edit_profile.name);
            if (l > 0) g_edit_profile.name[l - 1] = '\0';
        } else if (g_active_field == FIELD_HOST && k == SDLK_BACKSPACE) {
            size_t l = strlen(g_edit_profile.host);
            if (l > 0) g_edit_profile.host[l - 1] = '\0';
        } else if (g_active_field == FIELD_PORT && k == SDLK_BACKSPACE) {
            g_edit_profile.port /= 10;
        } else if (g_active_field == FIELD_SCRIPT) {
            int cur_line = 0, cur_col = 0;
            get_cursor_line_col(g_edit_profile.startup_script, g_script_cursor, &cur_line, &cur_col);
            int total = get_total_script_lines(g_edit_profile.startup_script);

            if (k == SDLK_BACKSPACE) {
                if (g_script_sel_start != g_script_sel_end) {
                    profile_delete_selection();
                } else if (g_script_cursor > 0) {
                    size_t l = strlen(g_edit_profile.startup_script);
                    memmove(&g_edit_profile.startup_script[g_script_cursor - 1],
                            &g_edit_profile.startup_script[g_script_cursor],
                            l - g_script_cursor + 1);
                    g_script_cursor--;
                    g_script_sel_start = g_script_sel_end = g_script_cursor;
                }
            } else if (k == SDLK_DELETE) {
                if (g_script_sel_start != g_script_sel_end) {
                    profile_delete_selection();
                } else {
                    size_t l = strlen(g_edit_profile.startup_script);
                    if (g_script_cursor < (int)l) {
                        memmove(&g_edit_profile.startup_script[g_script_cursor],
                                &g_edit_profile.startup_script[g_script_cursor + 1],
                                l - g_script_cursor);
                    }
                }
            } else if (k == SDLK_RETURN) {
                profile_delete_selection();
                size_t l = strlen(g_edit_profile.startup_script);
                if (l + 1 < sizeof(g_edit_profile.startup_script) - 1) {
                    memmove(&g_edit_profile.startup_script[g_script_cursor + 1],
                            &g_edit_profile.startup_script[g_script_cursor],
                            l - g_script_cursor + 1);
                    g_edit_profile.startup_script[g_script_cursor] = '\n';
                    g_script_cursor++;
                    g_script_sel_start = g_script_sel_end = g_script_cursor;
                }
            } else if (k == SDLK_LEFT && g_script_cursor > 0) {
                g_script_cursor--;
                if (shift) g_script_sel_end = g_script_cursor;
                else g_script_sel_start = g_script_sel_end = g_script_cursor;
            } else if (k == SDLK_RIGHT && (size_t)g_script_cursor < strlen(g_edit_profile.startup_script)) {
                g_script_cursor++;
                if (shift) g_script_sel_end = g_script_cursor;
                else g_script_sel_start = g_script_sel_end = g_script_cursor;
            } else if (k == SDLK_UP) {
                if (cur_line > 0) {
                    g_script_cursor = find_pos_at_line_col(g_edit_profile.startup_script, cur_line - 1, cur_col);
                    if (shift) g_script_sel_end = g_script_cursor;
                    else g_script_sel_start = g_script_sel_end = g_script_cursor;
                }
            } else if (k == SDLK_DOWN) {
                if (cur_line < total - 1) {
                    g_script_cursor = find_pos_at_line_col(g_edit_profile.startup_script, cur_line + 1, cur_col);
                    if (shift) g_script_sel_end = g_script_cursor;
                    else g_script_sel_start = g_script_sel_end = g_script_cursor;
                }
            } else if (k == SDLK_PAGEUP || k == SDLK_KP_9) {
                g_script_scroll_line -= SCRIPT_VISIBLE_LINES;
                if (g_script_scroll_line < 0) g_script_scroll_line = 0;
                int n_line = cur_line - SCRIPT_VISIBLE_LINES;
                if (n_line < 0) n_line = 0;
                g_script_cursor = find_pos_at_line_col(g_edit_profile.startup_script, n_line, cur_col);
                if (shift) g_script_sel_end = g_script_cursor;
                else g_script_sel_start = g_script_sel_end = g_script_cursor;
            } else if (k == SDLK_PAGEDOWN || k == SDLK_KP_3) {
                int max_scroll = total - SCRIPT_VISIBLE_LINES;
                if (max_scroll < 0) max_scroll = 0;
                g_script_scroll_line += SCRIPT_VISIBLE_LINES;
                if (g_script_scroll_line > max_scroll) g_script_scroll_line = max_scroll;
                int n_line = cur_line + SCRIPT_VISIBLE_LINES;
                if (n_line >= total) n_line = total - 1;
                g_script_cursor = find_pos_at_line_col(g_edit_profile.startup_script, n_line, cur_col);
                if (shift) g_script_sel_end = g_script_cursor;
                else g_script_sel_start = g_script_sel_end = g_script_cursor;
            } else if (k == SDLK_HOME || k == SDLK_KP_7) {
                while (g_script_cursor > 0 && g_edit_profile.startup_script[g_script_cursor - 1] != '\n') {
                    g_script_cursor--;
                }
                if (shift) g_script_sel_end = g_script_cursor;
                else g_script_sel_start = g_script_sel_end = g_script_cursor;
            } else if (k == SDLK_END || k == SDLK_KP_1) {
                while (g_edit_profile.startup_script[g_script_cursor] && g_edit_profile.startup_script[g_script_cursor] != '\n') {
                    g_script_cursor++;
                }
                if (shift) g_script_sel_end = g_script_cursor;
                else g_script_sel_start = g_script_sel_end = g_script_cursor;
            }

            /* Mantiene il cursore visibile nello scroll dell'editor del profilo */
            get_cursor_line_col(g_edit_profile.startup_script, g_script_cursor, &cur_line, &cur_col);
            if (cur_line < g_script_scroll_line) {
                g_script_scroll_line = cur_line;
            } else if (cur_line >= g_script_scroll_line + SCRIPT_VISIBLE_LINES) {
                g_script_scroll_line = cur_line - SCRIPT_VISIBLE_LINES + 1;
            }
        }
    }
}

void linux_profiles_render(void) {
    if (!g_prof_win || !g_prof_ren || !g_list) return;

    linux_theme_colors_t th = linux_theme_current();

    SDL_SetRenderDrawColor(g_prof_ren, th.bg_dark.r, th.bg_dark.g, th.bg_dark.b, 255);
    SDL_RenderClear(g_prof_ren);

    /* Testata Top Bar */
    draw_prof_rect(g_prof_ren, 0, 0, PROF_WIN_W, 44, th.bg_box.r, th.bg_box.g, th.bg_box.b, true, th.border.r, th.border.g, th.border.b);
    draw_prof_text(g_prof_ren, 20.0f, 27.0f, "CONNECTION PROFILES", th.bright.r, th.bright.g, th.bright.b);
    draw_prof_text(g_prof_ren, 260.0f, 27.0f, "CDC PLATO IV & CYBER1 TERMINAL EMULATOR", th.dim.r, th.dim.g, th.dim.b);

    /* 1. PANNELLO SINISTRO: LISTA PROFILI */
    draw_prof_rect(g_prof_ren, 20, 60, 220, 500, th.bg_box.r, th.bg_box.g, th.bg_box.b, true, th.border.r, th.border.g, th.border.b);
    for (size_t i = 0; i < g_list->count && i < 17; i++) {
        int y = 62 + (int)i * 28;
        bool is_sel = ((int)i == g_current_idx);
        if (is_sel) {
            draw_prof_rect(g_prof_ren, 22, y, 216, 26, th.hover_bg.r, th.hover_bg.g, th.hover_bg.b, true, th.bright.r, th.bright.g, th.bright.b);
        }
        char row_str[128];
        snprintf(row_str, sizeof(row_str), "%s%s", g_list->profiles[i].name, g_list->profiles[i].is_default ? " *" : "");
        draw_prof_text(g_prof_ren, 28.0f, (float)(y + 19), row_str, is_sel ? th.bright.r : th.normal.r, is_sel ? th.bright.g : th.normal.g, is_sel ? th.bright.b : th.normal.b);
    }

    draw_prof_button_ex(g_prof_ren, 20, 575, 100, 28, "+ New", false, is_prof_btn_flashing(PROF_BTN_NEW));
    draw_prof_button_ex(g_prof_ren, 130, 575, 100, 28, "- Delete", false, is_prof_btn_flashing(PROF_BTN_DELETE));

    /* 2. PANNELLO DESTRO: IMPOSTAZIONI PROFILO */
    draw_prof_text(g_prof_ren, 260.0f, 79.0f, "Name:", th.dim.r, th.dim.g, th.dim.b);
    draw_prof_rect(g_prof_ren, 340, 60, 320, 26, th.bg_box.r, th.bg_box.g, th.bg_box.b, true, (g_active_field == FIELD_NAME) ? th.bright.r : th.border.r, (g_active_field == FIELD_NAME) ? th.bright.g : th.border.g, (g_active_field == FIELD_NAME) ? th.bright.b : th.border.b);
    draw_prof_text(g_prof_ren, 346.0f, 78.0f, g_edit_profile.name, th.bright.r, th.bright.g, th.bright.b);

    draw_prof_text(g_prof_ren, 260.0f, 113.0f, "Host:", th.dim.r, th.dim.g, th.dim.b);
    draw_prof_rect(g_prof_ren, 340, 94, 320, 26, th.bg_box.r, th.bg_box.g, th.bg_box.b, true, (g_active_field == FIELD_HOST) ? th.bright.r : th.border.r, (g_active_field == FIELD_HOST) ? th.bright.g : th.border.g, (g_active_field == FIELD_HOST) ? th.bright.b : th.border.b);
    draw_prof_text(g_prof_ren, 346.0f, 112.0f, g_edit_profile.host, th.bright.r, th.bright.g, th.bright.b);

    draw_prof_text(g_prof_ren, 260.0f, 147.0f, "Port:", th.dim.r, th.dim.g, th.dim.b);
    draw_prof_rect(g_prof_ren, 340, 128, 110, 26, th.bg_box.r, th.bg_box.g, th.bg_box.b, true, (g_active_field == FIELD_PORT) ? th.bright.r : th.border.r, (g_active_field == FIELD_PORT) ? th.bright.g : th.border.g, (g_active_field == FIELD_PORT) ? th.bright.b : th.border.b);
    char port_buf[16]; snprintf(port_buf, sizeof(port_buf), "%d", g_edit_profile.port);
    draw_prof_text(g_prof_ren, 346.0f, 146.0f, port_buf, th.bright.r, th.bright.g, th.bright.b);

    /* Phosphor Color a fianco di Port (Y=128, X=540) - nascosto su Tektronix 4014 P31 */
    if (g_edit_profile.display_mode != 5) {
        draw_prof_text(g_prof_ren, 465.0f, 147.0f, "Phosphor:", th.dim.r, th.dim.g, th.dim.b);
        const char *ph_labels[] = { "Default", "Green P31", "Amber P40", "White P4" };
        int ph_x = 540;
        for (int i = 0; i < 4; i++) {
            bool is_sel = (g_edit_profile.phosphor_color == i);
            draw_prof_button(g_prof_ren, ph_x, 128, 85, 26, ph_labels[i], is_sel);
            ph_x += 90;
        }
    }

    /* 7 Display Modes: width 84, gap 5 (step 89) -> 340..958 */
    draw_prof_text(g_prof_ren, 260.0f, 181.0f, "Mode:", th.dim.r, th.dim.g, th.dim.b);
    const char *modes[] = { "Real Plasma", "Crisp Mono", "Split Mono", "Crisp Color", "Real CRT", "Tek 4014", "DEC PDP-1" };
    int mode_x = 340;
    for (int m = 0; m < 7; m++) {
        draw_prof_button(g_prof_ren, mode_x, 162, 84, 26, modes[m], (g_edit_profile.display_mode == m));
        mode_x += 89;
    }

    /* Parametri Condizionali Plasma / CRT / Tektronix */
    int mode = g_edit_profile.display_mode;
    if (mode == 0 || mode == 2) {
        draw_prof_text(g_prof_ren, 260.0f, 214.0f, "Decay:", th.dim.r, th.dim.g, th.dim.b);
        const char *dec_labels[] = { "100ms", "200ms", "500ms", "1s", "2s", "5s" };
        int px = 340;
        for (int i = 0; i < 6; i++) {
            draw_prof_button(g_prof_ren, px, 196, 66, 24, dec_labels[i], (g_edit_profile.persistence_ms == g_decay_ms_vals[i]));
            px += 72;
        }

        draw_prof_text(g_prof_ren, 260.0f, 244.0f, "Distort:", th.dim.r, th.dim.g, th.dim.b);
        const char *dist_labels[] = { "None", "Barrel", "Cylindrical" };
        int dx = 340;
        for (int i = 0; i < 3; i++) {
            draw_prof_button(g_prof_ren, dx, 226, 110, 24, dist_labels[i], (g_edit_profile.plasma_distortion == i));
            dx += 116;
        }
    } else if (mode == 4) {
        draw_prof_text(g_prof_ren, 260.0f, 214.0f, "Beam:", th.dim.r, th.dim.g, th.dim.b);
        const char *bm_labels[] = { "Standard", "High", "Ultra" };
        int bx = 340;
        for (int i = 0; i < 3; i++) {
            draw_prof_button(g_prof_ren, bx, 196, 110, 24, bm_labels[i], (g_edit_profile.crt_beam_level == i));
            bx += 116;
        }

        draw_prof_text(g_prof_ren, 260.0f, 244.0f, "Decay:", th.dim.r, th.dim.g, th.dim.b);
        const char *dec_labels[] = { "20ms", "200ms", "500ms", "1s", "2s", "5s" };
        int px = 340;
        for (int i = 0; i < 6; i++) {
            draw_prof_button(g_prof_ren, px, 226, 66, 24, dec_labels[i], (g_edit_profile.crt_persistence_ms == g_crt_decay_ms_vals[i]));
            px += 72;
        }

        draw_prof_text(g_prof_ren, 260.0f, 274.0f, "Distort:", th.dim.r, th.dim.g, th.dim.b);
        const char *dist_labels[] = { "None", "Barrel", "Cylindrical" };
        int dx = 340;
        for (int i = 0; i < 3; i++) {
            draw_prof_button(g_prof_ren, dx, 256, 110, 24, dist_labels[i], (g_edit_profile.crt_distortion == i));
            dx += 116;
        }
    } else if (mode == 5) {
        draw_prof_text(g_prof_ren, 260.0f, 214.0f, "Speed:", th.dim.r, th.dim.g, th.dim.b);
        const char *tek_labels[] = { "Instant", "110", "150", "300", "600", "1200", "2400", "4800", "9600" };
        int tx = 340;
        for (int i = 0; i < 9; i++) {
            draw_prof_button(g_prof_ren, tx, 196, 64, 24, tek_labels[i], (g_edit_profile.tek_baud_rate == g_tek_baud_vals[i]));
            tx += 69;
        }

        /* Checkboxes Tektronix a Y=234 */
        draw_prof_rect(g_prof_ren, 340, 234, 18, 18, th.bg_box.r, th.bg_box.g, th.bg_box.b, true, th.border.r, th.border.g, th.border.b);
        if (g_edit_profile.tek_erase_flash) draw_prof_text(g_prof_ren, 344.0f, 249.0f, "X", th.bright.r, th.bright.g, th.bright.b);
        draw_prof_text(g_prof_ren, 366.0f, 249.0f, "Erase Flash", th.normal.r, th.normal.g, th.normal.b);

        draw_prof_rect(g_prof_ren, 510, 234, 18, 18, th.bg_box.r, th.bg_box.g, th.bg_box.b, true, th.border.r, th.border.g, th.border.b);
        if (g_edit_profile.tek_accurate_bg) draw_prof_text(g_prof_ren, 514.0f, 249.0f, "X", th.bright.r, th.bright.g, th.bright.b);
        draw_prof_text(g_prof_ren, 536.0f, 249.0f, "Accurate Background", th.normal.r, th.normal.g, th.normal.b);
    } else if (mode == 6) {
        draw_prof_text(g_prof_ren, 260.0f, 214.0f, "Font:", th.dim.r, th.dim.g, th.dim.b);
        draw_prof_button(g_prof_ren, 340, 196, 150, 24, "1961 Codeword", (g_edit_profile.pdp1_font == 0));
        draw_prof_button(g_prof_ren, 500, 196, 150, 24, "1964 Character", (g_edit_profile.pdp1_font == 1));

        draw_prof_text(g_prof_ren, 260.0f, 244.0f, "Refresh:", th.dim.r, th.dim.g, th.dim.b);
        const int pdp1_hz_vals[] = { 60, 45, 30, 15, 10, 5 };
        const char *hz_labels[] = { "60 Hz", "45 Hz", "30 Hz", "15 Hz", "10 Hz", "5 Hz" };
        int cur_hz = (g_edit_profile.pdp1_refresh_rate > 0) ? g_edit_profile.pdp1_refresh_rate : 45;
        int rx = 340;
        for (int i = 0; i < 6; i++) {
            draw_prof_button(g_prof_ren, rx, 226, 66, 24, hz_labels[i], (cur_hz == pdp1_hz_vals[i]));
            rx += 72;
        }

        draw_prof_text(g_prof_ren, 260.0f, 274.0f, "Preset:", th.dim.r, th.dim.g, th.dim.b);
        const char *pr_labels[] = { "Historic (15s)", "Fast Decay (5s)", "Modern (2s)" };
        int pr_x = 340;
        for (int i = 0; i < 3; i++) {
            draw_prof_button(g_prof_ren, pr_x, 256, 140, 24, pr_labels[i], (g_edit_profile.pdp1_preset == i));
            pr_x += 150;
        }
    }

    /* Checkboxes a Y=292 */
    draw_prof_rect(g_prof_ren, 340, 292, 18, 18, th.bg_box.r, th.bg_box.g, th.bg_box.b, true, th.border.r, th.border.g, th.border.b);
    if (g_edit_profile.full_screen) draw_prof_text(g_prof_ren, 344.0f, 307.0f, "X", th.bright.r, th.bright.g, th.bright.b);
    draw_prof_text(g_prof_ren, 366.0f, 307.0f, "Full Screen", th.normal.r, th.normal.g, th.normal.b);

    draw_prof_rect(g_prof_ren, 500, 292, 18, 18, th.bg_box.r, th.bg_box.g, th.bg_box.b, true, th.border.r, th.border.g, th.border.b);
    if (g_edit_profile.is_default) draw_prof_text(g_prof_ren, 504.0f, 307.0f, "X", th.bright.r, th.bright.g, th.bright.b);
    draw_prof_text(g_prof_ren, 526.0f, 307.0f, "Default Profile", th.normal.r, th.normal.g, th.normal.b);

    draw_prof_rect(g_prof_ren, 670, 292, 18, 18, th.bg_box.r, th.bg_box.g, th.bg_box.b, true, th.border.r, th.border.g, th.border.b);
    if (g_edit_profile.startup_script_enabled) draw_prof_text(g_prof_ren, 674.0f, 307.0f, "X", th.bright.r, th.bright.g, th.bright.b);
    draw_prof_text(g_prof_ren, 696.0f, 307.0f, "Startup Script", th.normal.r, th.normal.g, th.normal.b);

    /* Script Editor Box */
    int total_lines = get_total_script_lines(g_edit_profile.startup_script);
    char hdr_str[128];
    snprintf(hdr_str, sizeof(hdr_str), "Startup Script (Line %d of %d):", g_script_scroll_line + 1, total_lines);
    draw_prof_text(g_prof_ren, 260.0f, 332.0f, hdr_str, th.dim.r, th.dim.g, th.dim.b);

    draw_prof_rect(g_prof_ren, SCRIPT_BOX_X, SCRIPT_BOX_Y, SCRIPT_BOX_W, SCRIPT_BOX_H, th.bg_box.r, th.bg_box.g, th.bg_box.b, true,
                   (g_active_field == FIELD_SCRIPT) ? th.bright.r : th.border.r, (g_active_field == FIELD_SCRIPT) ? th.bright.g : th.border.g, (g_active_field == FIELD_SCRIPT) ? th.bright.b : th.border.b);

    const char *sp = g_edit_profile.startup_script;
    int current_line_num = 0;
    int cur_line_pos, cur_col_pos;
    get_cursor_line_col(g_edit_profile.startup_script, g_script_cursor, &cur_line_pos, &cur_col_pos);

    int drawn_lines = 0;
    while (*sp && drawn_lines < SCRIPT_VISIBLE_LINES) {
        const char *line_start = sp;
        while (*sp && *sp != '\n') sp++;
        int line_len = (int)(sp - line_start);

        if (current_line_num >= g_script_scroll_line) {
            char line_buf[256];
            int copy_len = (line_len < (int)sizeof(line_buf) - 1) ? line_len : (int)sizeof(line_buf) - 1;
            strncpy(line_buf, line_start, copy_len);
            line_buf[copy_len] = '\0';

            int line_y = SCRIPT_BOX_Y + 18 + drawn_lines * SCRIPT_LINE_HEIGHT;
            int line_start_idx = (int)(line_start - g_edit_profile.startup_script);
            int line_end_idx = line_start_idx + line_len;
            int s_min = g_script_sel_start < g_script_sel_end ? g_script_sel_start : g_script_sel_end;
            int s_max = g_script_sel_start < g_script_sel_end ? g_script_sel_end : g_script_sel_start;
            if (s_min != s_max && s_min < line_end_idx && s_max > line_start_idx) {
                int o_start = s_min > line_start_idx ? s_min : line_start_idx;
                int o_end = s_max < line_end_idx ? s_max : line_end_idx;
                float sel_x1 = (float)(SCRIPT_BOX_X + 8) + get_string_width_f(line_start, o_start - line_start_idx);
                float sel_w = get_string_width_f(line_start + (o_start - line_start_idx), o_end - o_start);
                draw_prof_rect(g_prof_ren, (int)sel_x1, line_y - 14, (int)sel_w + 1, 16, th.hover_bg.r, th.hover_bg.g, th.hover_bg.b, false, 0, 0, 0);
            }
            draw_prof_text(g_prof_ren, (float)(SCRIPT_BOX_X + 8), (float)line_y, line_buf, th.normal.r, th.normal.g, th.normal.b);

            if (g_active_field == FIELD_SCRIPT && current_line_num == cur_line_pos) {
                if ((SDL_GetTicks() / 500) % 2 == 0) {
                    float cur_x = (float)(SCRIPT_BOX_X + 8) + get_string_width_f(line_start, cur_col_pos);
                    draw_prof_rect(g_prof_ren, (int)cur_x, line_y - 14, 2, 16, th.bright.r, th.bright.g, th.bright.b, false, 0, 0, 0);
                }
            }
            drawn_lines++;
        }

        if (*sp == '\n') sp++;
        current_line_num++;
    }

    /* Scrollbar */
    int sb_x = SCRIPT_BOX_X + SCRIPT_BOX_W - 12;
    int sb_y = SCRIPT_BOX_Y + 2;
    int sb_h = SCRIPT_BOX_H - 4;
    draw_prof_rect(g_prof_ren, sb_x, sb_y, 8, sb_h, th.bg_dark.r, th.bg_dark.g, th.bg_dark.b, true, th.border.r, th.border.g, th.border.b);

    if (total_lines > SCRIPT_VISIBLE_LINES) {
        int thumb_h = (sb_h * SCRIPT_VISIBLE_LINES) / total_lines;
        if (thumb_h < 14) thumb_h = 14;
        int max_scroll = total_lines - SCRIPT_VISIBLE_LINES;
        int thumb_y = sb_y + (g_script_scroll_line * (sb_h - thumb_h)) / max_scroll;
        draw_prof_rect(g_prof_ren, sb_x, thumb_y, 8, thumb_h, th.dim.r, th.dim.g, th.dim.b, true, th.bright.r, th.bright.g, th.bright.b);
    }

    /* Pulsanti Inferiori a Y=575 */
    draw_prof_button_ex(g_prof_ren, 340, 575, 130, 32, "Set as Default", false, is_prof_btn_flashing(PROF_BTN_DEFAULT));
    draw_prof_button_ex(g_prof_ren, 485, 575, 100, 32, "Save", false, is_prof_btn_flashing(PROF_BTN_SAVE));
    draw_prof_button(g_prof_ren, 600, 575, 140, 32, "Connect Now", true);
    draw_prof_button(g_prof_ren, 755, 575, 100, 32, "Close", false);

    SDL_RenderPresent(g_prof_ren);
}
