/*
 * PlatoLives - Native Linux Connection Profiles Manager (SDL2)
 * Full multi-line script editor, scrolling, auto-login templates & pixel-perfect UI.
 */

#include "linux_profiles.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plato/plato_script.h"
#include "plato/stb_truetype.h"
#include "fonts/roboto_font_data.h"

#define PROF_WIN_W 980
#define PROF_WIN_H 640

#define SCRIPT_BOX_X 340
#define SCRIPT_BOX_Y 338
#define SCRIPT_BOX_W 595
#define SCRIPT_BOX_H 195
#define SCRIPT_VISIBLE_LINES 9
#define SCRIPT_LINE_HEIGHT 20

static SDL_Window *g_prof_win = NULL;
static SDL_Renderer *g_prof_ren = NULL;
static stbtt_bakedchar g_prof_cdata[96];
static unsigned char *g_prof_atlas = NULL;
static SDL_Texture *g_prof_atlas_tex = NULL;
static bool g_prof_font_ready = false;

static plato_profile_list_t *g_list = NULL;
static int g_current_idx = 0;
static linux_profile_connect_cb g_connect_cb = NULL;
static void *g_cb_user_data = NULL;

typedef enum {
    FIELD_NONE = 0,
    FIELD_NAME,
    FIELD_HOST,
    FIELD_PORT,
    FIELD_SCRIPT
} active_field_t;

static active_field_t g_active_field = FIELD_NONE;
static plato_profile_t g_edit_profile;
static int g_list_scroll = 0;

typedef enum {
    PROF_BTN_NONE = 0,
    PROF_BTN_NEW,
    PROF_BTN_DELETE,
    PROF_BTN_DEFAULT,
    PROF_BTN_SAVE,
    PROF_BTN_CONNECT
} prof_btn_id_t;

static prof_btn_id_t g_prof_flash_btn = PROF_BTN_NONE;
static uint32_t g_prof_flash_until = 0;

static void trigger_prof_flash(prof_btn_id_t btn) {
    g_prof_flash_btn = btn;
    g_prof_flash_until = SDL_GetTicks() + 250;
}

static bool is_prof_btn_flashing(prof_btn_id_t btn) {
    return (g_prof_flash_btn == btn && SDL_GetTicks() < g_prof_flash_until);
}


/* Stato Editor Script Multi-linea */
static int g_script_cursor = 0;
static int g_script_scroll_line = 0;
static bool g_sb_dragging = false;
static int g_sb_drag_start_y = 0;
static int g_sb_drag_start_scroll = 0;

static const int g_decay_ms_vals[] = { 100, 200, 500, 1000, 2000, 5000 };
static const int g_crt_decay_ms_vals[] = { 20, 200, 500, 1000, 2000, 5000 };

static void init_prof_font(void) {
    if (g_prof_font_ready || !g_prof_ren) return;
    if (!g_prof_atlas) {
        g_prof_atlas = (unsigned char *)malloc(512 * 512);
    }
    stbtt_BakeFontBitmap(g_roboto_ttf, 0, 17.0f, g_prof_atlas, 512, 512, 32, 96, g_prof_cdata);

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

static float get_char_width_f(char c) {
    if ((unsigned char)c < 32 || (unsigned char)c >= 128) return 6.0f;
    return g_prof_cdata[(unsigned char)c - 32].xadvance;
}

static float get_string_width_f(const char *str, int count) {
    if (!str) return 0.0f;
    float w = 0.0f;
    for (int i = 0; i < count && str[i]; i++) {
        w += get_char_width_f(str[i]);
    }
    return w;
}

static void draw_prof_text(SDL_Renderer *ren, float x, float y, const char *str, uint8_t r, uint8_t g, uint8_t b) {
    if (!g_prof_atlas_tex || !str) return;
    SDL_SetTextureColorMod(g_prof_atlas_tex, r, g, b);

    while (*str) {
        char c = *str++;
        if ((unsigned char)c < 32 || (unsigned char)c >= 128) continue;
        const stbtt_bakedchar *bc = &g_prof_cdata[c - 32];

        SDL_Rect src = { bc->x0, bc->y0, bc->x1 - bc->x0, bc->y1 - bc->y0 };
        SDL_Rect dst = { (int)(x + bc->xoff + 0.5f), (int)(y + bc->yoff + 0.5f), src.w, src.h };

        SDL_RenderCopy(ren, g_prof_atlas_tex, &src, &dst);
        x += bc->xadvance;
    }
}

static void draw_prof_rect(SDL_Renderer *ren, int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, bool outline, uint8_t or, uint8_t og, uint8_t ob) {
    SDL_Rect rc = { x, y, w, h };
    SDL_SetRenderDrawColor(ren, r, g, b, 255);
    SDL_RenderFillRect(ren, &rc);
    if (outline) {
        SDL_SetRenderDrawColor(ren, or, og, ob, 255);
        SDL_RenderDrawRect(ren, &rc);
    }
}

static void draw_prof_button_ex(SDL_Renderer *ren, int x, int y, int w, int h, const char *label, bool selected, bool flashing) {
    uint8_t bg_r = flashing ? 255 : (selected ? 96 : 36);
    uint8_t bg_g = flashing ? 220 : (selected ? 42 : 18);
    uint8_t bg_b = flashing ? 0   : (selected ? 10 : 8);

    uint8_t bd_r = flashing ? 255 : (selected ? 255 : 120);
    uint8_t bd_g = flashing ? 245 : (selected ? 140 : 60);
    uint8_t bd_b = flashing ? 120 : (selected ? 20  : 20);

    draw_prof_rect(ren, x, y, w, h, bg_r, bg_g, bg_b, true, bd_r, bd_g, bd_b);

    float tw = get_string_width_f(label, (int)strlen(label));
    float tx = (float)x + ((float)w - tw) / 2.0f;
    float ty = (float)y + 17.0f;

    uint8_t fg_r = flashing ? 20  : (selected ? 255 : 210);
    uint8_t fg_g = flashing ? 12  : (selected ? 215 : 130);
    uint8_t fg_b = flashing ? 4   : (selected ? 60  : 20);

    draw_prof_text(ren, tx, ty, label, fg_r, fg_g, fg_b);
}

static void draw_prof_button(SDL_Renderer *ren, int x, int y, int w, int h, const char *label, bool selected) {
    draw_prof_button_ex(ren, x, y, w, h, label, selected, false);
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
    get_cursor_line_col(g_edit_profile.startup_script, g_script_cursor, &cur_line, &cur_col);
    if (cur_line < g_script_scroll_line) {
        g_script_scroll_line = cur_line;
    } else if (cur_line >= g_script_scroll_line + SCRIPT_VISIBLE_LINES) {
        g_script_scroll_line = cur_line - SCRIPT_VISIBLE_LINES + 1;
    }
}

static void sync_from_profile(int idx) {
    if (!g_list || idx < 0 || (size_t)idx >= g_list->count) return;
    g_current_idx = idx;
    g_edit_profile = g_list->profiles[idx];
    if (g_edit_profile.crt_persistence_ms == 0) g_edit_profile.crt_persistence_ms = 20;

    if (strlen(g_edit_profile.startup_script) == 0) {
        snprintf(g_edit_profile.startup_script, sizeof(g_edit_profile.startup_script), "%s", PLATO_DEFAULT_AUTOLOGIN_SCRIPT);
    }
    g_script_cursor = 0;
    g_script_scroll_line = 0;
}

static void sync_to_profile(void) {
    if (!g_list || g_current_idx < 0 || (size_t)g_current_idx >= g_list->count) return;
    g_list->profiles[g_current_idx] = g_edit_profile;
    plato_profiles_save(g_list);
}

void linux_profiles_show(plato_profile_list_t *list, linux_profile_connect_cb cb, void *user_data) {
    if (!list) return;
    g_list = list;
    g_connect_cb = cb;
    g_cb_user_data = user_data;

    if (g_prof_win) {
        SDL_RaiseWindow(g_prof_win);
        return;
    }

    g_prof_win = SDL_CreateWindow(
        "Connection Profiles - PlatoLives",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        PROF_WIN_W, PROF_WIN_H,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!g_prof_win) return;

    g_prof_ren = SDL_CreateRenderer(g_prof_win, -1, SDL_RENDERER_SOFTWARE);
    if (!g_prof_ren) {
        g_prof_ren = SDL_CreateRenderer(g_prof_win, -1, 0);
    }

    g_prof_font_ready = false;
    init_prof_font();

    int initial_idx = (list->default_index >= 0 && (size_t)list->default_index < list->count) ? list->default_index : 0;
    sync_from_profile(initial_idx);
    g_active_field = FIELD_NONE;
}

bool linux_profiles_is_open(void) {
    return (g_prof_win != NULL);
}

void linux_profiles_destroy(void) {
    if (g_prof_atlas_tex) {
        SDL_DestroyTexture(g_prof_atlas_tex);
        g_prof_atlas_tex = NULL;
    }
    if (g_prof_atlas) {
        free(g_prof_atlas);
        g_prof_atlas = NULL;
    }
    if (g_prof_ren) {
        SDL_DestroyRenderer(g_prof_ren);
        g_prof_ren = NULL;
    }
    if (g_prof_win) {
        SDL_DestroyWindow(g_prof_win);
        g_prof_win = NULL;
    }
    g_prof_font_ready = false;
}

static bool pt_in_rect(int px, int py, int x, int y, int w, int h) {
    return (px >= x && px < x + w && py >= y && py < y + h);
}

void linux_profiles_handle_event(const SDL_Event *ev) {
    if (!g_prof_win) return;
    uint32_t win_id = SDL_GetWindowID(g_prof_win);

    if (ev->type == SDL_WINDOWEVENT && ev->window.windowID == win_id) {
        if (ev->window.event == SDL_WINDOWEVENT_CLOSE) {
            linux_profiles_destroy();
            return;
        }
    } else if (ev->type == SDL_MOUSEWHEEL && ev->wheel.windowID == win_id) {
        int total = get_total_script_lines(g_edit_profile.startup_script);
        int max_scroll = total - SCRIPT_VISIBLE_LINES;
        if (max_scroll < 0) max_scroll = 0;

        g_script_scroll_line -= ev->wheel.y * 3;
        if (g_script_scroll_line < 0) g_script_scroll_line = 0;
        if (g_script_scroll_line > max_scroll) g_script_scroll_line = max_scroll;
        return;
        } else if (ev->type == SDL_MOUSEBUTTONUP && ev->button.windowID == win_id) {
        if (ev->button.button == SDL_BUTTON_LEFT) {
            g_sb_dragging = false;
        }
    } else if (ev->type == SDL_MOUSEMOTION && ev->motion.windowID == win_id) {
        if (g_sb_dragging && (ev->motion.state & SDL_BUTTON_LMASK)) {
            int total_lines = get_total_script_lines(g_edit_profile.startup_script);
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
        if (ev->key.keysym.sym == SDLK_ESCAPE) {
            linux_profiles_destroy();
            return;
        }

        SDL_Keymod mod = SDL_GetModState();
        bool ctrl = (mod & KMOD_CTRL) != 0;
        bool gui  = (mod & KMOD_GUI) != 0;

        if ((ctrl || gui) && ev->key.keysym.sym == SDLK_v && g_active_field == FIELD_SCRIPT) {
            char *cb_text = SDL_GetClipboardText();
            if (cb_text) {
                size_t cb_len = strlen(cb_text);
                size_t cur_len = strlen(g_edit_profile.startup_script);
                if (cur_len + cb_len < sizeof(g_edit_profile.startup_script) - 1) {
                    memmove(&g_edit_profile.startup_script[g_script_cursor + cb_len],
                            &g_edit_profile.startup_script[g_script_cursor],
                            cur_len - g_script_cursor + 1);
                    memcpy(&g_edit_profile.startup_script[g_script_cursor], cb_text, cb_len);
                    g_script_cursor += (int)cb_len;
                    ensure_cursor_visible();
                }
                SDL_free(cb_text);
            }
            return;
        }

        if (g_active_field == FIELD_SCRIPT) {
            int cur_line, cur_col;
            get_cursor_line_col(g_edit_profile.startup_script, g_script_cursor, &cur_line, &cur_col);
            int total = get_total_script_lines(g_edit_profile.startup_script);

            if (ev->key.keysym.sym == SDLK_UP) {
                if (cur_line > 0) {
                    g_script_cursor = find_pos_at_line_col(g_edit_profile.startup_script, cur_line - 1, cur_col);
                    ensure_cursor_visible();
                }
                return;
            } else if (ev->key.keysym.sym == SDLK_DOWN) {
                if (cur_line < total - 1) {
                    g_script_cursor = find_pos_at_line_col(g_edit_profile.startup_script, cur_line + 1, cur_col);
                    ensure_cursor_visible();
                }
                return;
            } else if (ev->key.keysym.sym == SDLK_LEFT) {
                if (g_script_cursor > 0) {
                    g_script_cursor--;
                    ensure_cursor_visible();
                }
                return;
            } else if (ev->key.keysym.sym == SDLK_RIGHT) {
                if (g_script_cursor < (int)strlen(g_edit_profile.startup_script)) {
                    g_script_cursor++;
                    ensure_cursor_visible();
                }
                return;
            } else if (ev->key.keysym.sym == SDLK_PAGEUP) {
                g_script_scroll_line -= SCRIPT_VISIBLE_LINES;
                if (g_script_scroll_line < 0) g_script_scroll_line = 0;
                int n_line = cur_line - SCRIPT_VISIBLE_LINES;
                if (n_line < 0) n_line = 0;
                g_script_cursor = find_pos_at_line_col(g_edit_profile.startup_script, n_line, cur_col);
                return;
            } else if (ev->key.keysym.sym == SDLK_PAGEDOWN) {
                int max_scroll = total - SCRIPT_VISIBLE_LINES;
                if (max_scroll < 0) max_scroll = 0;
                g_script_scroll_line += SCRIPT_VISIBLE_LINES;
                if (g_script_scroll_line > max_scroll) g_script_scroll_line = max_scroll;
                int n_line = cur_line + SCRIPT_VISIBLE_LINES;
                if (n_line >= total) n_line = total - 1;
                g_script_cursor = find_pos_at_line_col(g_edit_profile.startup_script, n_line, cur_col);
                return;
            } else if (ev->key.keysym.sym == SDLK_HOME) {
                while (g_script_cursor > 0 && g_edit_profile.startup_script[g_script_cursor - 1] != '\n') {
                    g_script_cursor--;
                }
                ensure_cursor_visible();
                return;
            } else if (ev->key.keysym.sym == SDLK_END) {
                while (g_edit_profile.startup_script[g_script_cursor] && g_edit_profile.startup_script[g_script_cursor] != '\n') {
                    g_script_cursor++;
                }
                ensure_cursor_visible();
                return;
            } else if (ev->key.keysym.sym == SDLK_DELETE) {
                size_t len = strlen(g_edit_profile.startup_script);
                if (g_script_cursor < (int)len) {
                    memmove(&g_edit_profile.startup_script[g_script_cursor],
                            &g_edit_profile.startup_script[g_script_cursor + 1],
                            len - g_script_cursor);
                }
                return;
            }
        }

        if (ev->key.keysym.sym == SDLK_BACKSPACE) {
            if (g_active_field == FIELD_NAME) {
                size_t len = strlen(g_edit_profile.name);
                if (len > 0) g_edit_profile.name[len - 1] = '\0';
            } else if (g_active_field == FIELD_HOST) {
                size_t len = strlen(g_edit_profile.host);
                if (len > 0) g_edit_profile.host[len - 1] = '\0';
            } else if (g_active_field == FIELD_PORT) {
                char buf[16]; snprintf(buf, sizeof(buf), "%d", g_edit_profile.port);
                size_t len = strlen(buf);
                if (len > 0) { buf[len - 1] = '\0'; g_edit_profile.port = atoi(buf); }
            } else if (g_active_field == FIELD_SCRIPT) {
                if (g_script_cursor > 0) {
                    size_t len = strlen(g_edit_profile.startup_script);
                    memmove(&g_edit_profile.startup_script[g_script_cursor - 1],
                            &g_edit_profile.startup_script[g_script_cursor],
                            len - g_script_cursor + 1);
                    g_script_cursor--;
                    ensure_cursor_visible();
                }
            }
        } else if (ev->key.keysym.sym == SDLK_RETURN) {
            if (g_active_field == FIELD_SCRIPT) {
                size_t len = strlen(g_edit_profile.startup_script);
                if (len + 1 < sizeof(g_edit_profile.startup_script) - 1) {
                    memmove(&g_edit_profile.startup_script[g_script_cursor + 1],
                            &g_edit_profile.startup_script[g_script_cursor],
                            len - g_script_cursor + 1);
                    g_edit_profile.startup_script[g_script_cursor] = '\n';
                    g_script_cursor++;
                    ensure_cursor_visible();
                }
            } else {
                g_active_field = FIELD_NONE;
            }
        } else if (ev->key.keysym.sym == SDLK_TAB) {
            g_active_field = (g_active_field == FIELD_NAME) ? FIELD_HOST :
                             (g_active_field == FIELD_HOST) ? FIELD_PORT :
                             (g_active_field == FIELD_PORT) ? FIELD_SCRIPT : FIELD_NAME;
        }
    } else if (ev->type == SDL_TEXTINPUT && ev->text.windowID == win_id) {
        const char *txt = ev->text.text;
        if (g_active_field == FIELD_NAME) {
            size_t len = strlen(g_edit_profile.name);
            strncat(g_edit_profile.name, txt, sizeof(g_edit_profile.name) - len - 1);
        } else if (g_active_field == FIELD_HOST) {
            size_t len = strlen(g_edit_profile.host);
            strncat(g_edit_profile.host, txt, sizeof(g_edit_profile.host) - len - 1);
        } else if (g_active_field == FIELD_PORT) {
            char buf[16]; snprintf(buf, sizeof(buf), "%d", g_edit_profile.port);
            size_t len = strlen(buf);
            if (len < 5 && txt[0] >= '0' && txt[0] <= '9') {
                strncat(buf, txt, sizeof(buf) - len - 1);
                g_edit_profile.port = atoi(buf);
            }
        } else if (g_active_field == FIELD_SCRIPT) {
            size_t t_len = strlen(txt);
            size_t cur_len = strlen(g_edit_profile.startup_script);
            if (cur_len + t_len < sizeof(g_edit_profile.startup_script) - 1) {
                memmove(&g_edit_profile.startup_script[g_script_cursor + t_len],
                        &g_edit_profile.startup_script[g_script_cursor],
                        cur_len - g_script_cursor + 1);
                memcpy(&g_edit_profile.startup_script[g_script_cursor], txt, t_len);
                g_script_cursor += (int)t_len;
                ensure_cursor_visible();
            }
        }
    } else if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.windowID == win_id) {
        int mx = ev->button.x;
        int my = ev->button.y;

        /* Click Lista Profili */
        if (pt_in_rect(mx, my, 20, 60, 220, 480)) {
            int row = (my - 60) / 28 + g_list_scroll;
            if (row >= 0 && (size_t)row < g_list->count) {
                sync_to_profile();
                sync_from_profile(row);
                g_active_field = FIELD_NONE;
            }
            return;
        }

        /* Pulsante [+ New] */
        if (pt_in_rect(mx, my, 20, 550, 100, 28)) {
            trigger_prof_flash(PROF_BTN_NEW);
            sync_to_profile();
            plato_profile_t np;
            memset(&np, 0, sizeof(np));
            snprintf(np.name, sizeof(np.name), "Profile %d", (int)g_list->count + 1);
            snprintf(np.host, sizeof(np.host), "cyberserv.org");
            np.port = 8005;
            np.display_mode = 0;
            np.persistence_ms = 100;
            np.plasma_distortion = 2;
            np.crt_persistence_ms = 20;
            np.crt_beam_level = 0;
            np.phosphor_color = 0;
            np.crt_distortion = 0;
            np.startup_script_enabled = false;
            snprintf(np.startup_script, sizeof(np.startup_script), "%s", PLATO_DEFAULT_AUTOLOGIN_SCRIPT);
            plato_profiles_add(g_list, &np);
            sync_from_profile((int)g_list->count - 1);
            return;
        }

        /* Pulsante [- Delete] */
        if (pt_in_rect(mx, my, 130, 550, 100, 28)) {
            trigger_prof_flash(PROF_BTN_DELETE);
            if (g_list->count > 1) {
                plato_profiles_delete(g_list, (size_t)g_current_idx);
                if (g_current_idx >= (int)g_list->count) g_current_idx = (int)g_list->count - 1;
                sync_from_profile(g_current_idx);
            }
            return;
        }

        /* Click & Drag Scrollbar Script */
        int sb_x = SCRIPT_BOX_X + SCRIPT_BOX_W - 14;
        int sb_y = SCRIPT_BOX_Y + 2;
        int sb_w = 12;
        int sb_h = SCRIPT_BOX_H - 4;
        if (pt_in_rect(mx, my, sb_x - 4, sb_y, sb_w + 8, sb_h)) {
            int total_lines = get_total_script_lines(g_edit_profile.startup_script);
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

        /* Click Campi di Testo Destri */
        if (pt_in_rect(mx, my, 340, 60, 320, 26)) { g_active_field = FIELD_NAME; return; }
        if (pt_in_rect(mx, my, 340, 96, 320, 26)) { g_active_field = FIELD_HOST; return; }
        if (pt_in_rect(mx, my, 340, 132, 120, 26)) { g_active_field = FIELD_PORT; return; }

        /* Phosphor Color a Y=132 (a fianco di Port) */
        if (g_edit_profile.display_mode != 3) {
            int ph_x = 570;
            for (int i = 0; i < 4; i++) {
                if (pt_in_rect(mx, my, ph_x, 132, 82, 26)) {
                    g_edit_profile.phosphor_color = i;
                    return;
                }
                ph_x += 88;
            }
        }

        /* Display Modes: 0: Real Plasma, 1: Crisp Mono, 2: Split Mono, 3: Crisp Color, 4: Real CRT */
        int mode_x = 340;
        for (int m = 0; m < 5; m++) {
            if (pt_in_rect(mx, my, mode_x, 168, 110, 26)) {
                g_edit_profile.display_mode = m;
                return;
            }
            mode_x += 116;
        }

        /* Parametri Dinamici Plasma (Y=204, 236) / CRT (Y=204, 236, 268) */
        int mode = g_edit_profile.display_mode;
        if (mode == 0 || mode == 2) {
            /* Plasma Persistence at Y=204 */
            int px = 340;
            for (int i = 0; i < 6; i++) {
                if (pt_in_rect(mx, my, px, 204, 65, 24)) {
                    g_edit_profile.persistence_ms = g_decay_ms_vals[i];
                    return;
                }
                px += 70;
            }
            /* Plasma Distortion at Y=236 */
            int dx = 340;
            for (int i = 0; i < 3; i++) {
                if (pt_in_rect(mx, my, dx, 236, 110, 24)) {
                    g_edit_profile.plasma_distortion = i;
                    return;
                }
                dx += 116;
            }
        } else if (mode == 4) {
            /* CRT Beam at Y=204 */
            int bx = 340;
            for (int i = 0; i < 3; i++) {
                if (pt_in_rect(mx, my, bx, 204, 110, 24)) {
                    g_edit_profile.crt_beam_level = i;
                    return;
                }
                bx += 116;
            }
            /* CRT Persistence at Y=236 (CORRETTO) */
            int px = 340;
            for (int i = 0; i < 6; i++) {
                if (pt_in_rect(mx, my, px, 236, 65, 24)) {
                    g_edit_profile.crt_persistence_ms = g_crt_decay_ms_vals[i];
                    return;
                }
                px += 70;
            }
            /* CRT Distortion at Y=268 (CORRETTO) */
            int dx = 340;
            for (int i = 0; i < 3; i++) {
                if (pt_in_rect(mx, my, dx, 268, 110, 24)) {
                    g_edit_profile.crt_distortion = i;
                    return;
                }
                dx += 116;
            }
        }

        /* Checkboxes */
        if (pt_in_rect(mx, my, 340, 305, 150, 24)) { g_edit_profile.full_screen = !g_edit_profile.full_screen; return; }
        if (pt_in_rect(mx, my, 510, 305, 150, 24)) {
            g_edit_profile.is_default = true;
            plato_profiles_set_default(g_list, (size_t)g_current_idx);
            return;
        }
        if (pt_in_rect(mx, my, 680, 305, 200, 24)) { g_edit_profile.startup_script_enabled = !g_edit_profile.startup_script_enabled; return; }

        /* Click Script Text Area */
        if (pt_in_rect(mx, my, SCRIPT_BOX_X, SCRIPT_BOX_Y, SCRIPT_BOX_W, SCRIPT_BOX_H)) {
            g_active_field = FIELD_SCRIPT;
            int rel_y = my - SCRIPT_BOX_Y - 4;
            int clicked_line = g_script_scroll_line + (rel_y / SCRIPT_LINE_HEIGHT);
            int rel_x = mx - SCRIPT_BOX_X - 8;
            int clicked_col = (rel_x > 0) ? (int)((float)rel_x / 8.5f) : 0;
            g_script_cursor = find_pos_at_line_col(g_edit_profile.startup_script, clicked_line, clicked_col);
            return;
        }

        /* Pulsanti Inferiori Destri */
        if (pt_in_rect(mx, my, 340, 550, 130, 32)) {
            g_edit_profile.is_default = true;
            plato_profiles_set_default(g_list, (size_t)g_current_idx);
            sync_to_profile();
            trigger_prof_flash(PROF_BTN_DEFAULT);
            return;
        }
        if (pt_in_rect(mx, my, 485, 550, 100, 32)) {
            sync_to_profile();
            trigger_prof_flash(PROF_BTN_SAVE);
            return;
        }
        if (pt_in_rect(mx, my, 600, 550, 140, 32)) {
            sync_to_profile();
            if (g_connect_cb && g_current_idx >= 0 && (size_t)g_current_idx < g_list->count) {
                g_connect_cb(&g_list->profiles[g_current_idx], g_cb_user_data);
            }
            linux_profiles_destroy();
            return;
        }
        if (pt_in_rect(mx, my, 755, 550, 90, 32)) {
            linux_profiles_destroy();
            return;
        }

        g_active_field = FIELD_NONE;
    }
}

void linux_profiles_render(void) {
    if (!g_prof_win || !g_prof_ren || !g_list) return;

    /* Sfondo Principale Dark Plasma Look */
    SDL_SetRenderDrawColor(g_prof_ren, 16, 10, 6, 255);
    SDL_RenderClear(g_prof_ren);

    /* Testata Top Bar Plasma */
    draw_prof_rect(g_prof_ren, 0, 0, PROF_WIN_W, 44, 34, 17, 6, true, 190, 85, 12);
    draw_prof_text(g_prof_ren, 20.0f, 27.0f, "CONNECTION PROFILES", 255, 215, 60);
    draw_prof_text(g_prof_ren, 260.0f, 27.0f, "CDC PLATO IV & CYBER1 TERMINAL EMULATOR", 204, 102, 0);

    /* 1. PANNELLO SINISTRO: LISTA PROFILI */
    draw_prof_rect(g_prof_ren, 20, 60, 220, 480, 24, 14, 8, true, 96, 42, 10);
    for (size_t i = 0; i < g_list->count && i < 16; i++) {
        int y = 62 + (int)i * 28;
        bool is_sel = ((int)i == g_current_idx);
        if (is_sel) {
            draw_prof_rect(g_prof_ren, 22, y, 216, 26, 96, 42, 10, true, 255, 140, 20);
        }
        char row_str[128];
        snprintf(row_str, sizeof(row_str), "%s%s", g_list->profiles[i].name, g_list->profiles[i].is_default ? " *" : "");
        draw_prof_text(g_prof_ren, 28.0f, (float)(y + 19), row_str, is_sel ? 255 : 230, is_sel ? 215 : 140, is_sel ? 60 : 20);
    }

    /* Pulsanti Lista [+ New] e [- Delete] */
    draw_prof_button_ex(g_prof_ren, 20, 550, 100, 28, "+ New", false, is_prof_btn_flashing(PROF_BTN_NEW));
    draw_prof_button_ex(g_prof_ren, 130, 550, 100, 28, "- Delete", false, is_prof_btn_flashing(PROF_BTN_DELETE));

    /* 2. PANNELLO DESTRO: IMPOSTAZIONI PROFILO */
    draw_prof_text(g_prof_ren, 260.0f, 80.0f, "Name:", 255, 140, 20);
    draw_prof_rect(g_prof_ren, 340, 60, 320, 26, 32, 18, 10, true, (g_active_field == FIELD_NAME) ? 255 : 120, (g_active_field == FIELD_NAME) ? 215 : 60, 20);
    draw_prof_text(g_prof_ren, 346.0f, 79.0f, g_edit_profile.name, 255, 215, 60);

    draw_prof_text(g_prof_ren, 260.0f, 116.0f, "Host:", 255, 140, 20);
    draw_prof_rect(g_prof_ren, 340, 96, 320, 26, 32, 18, 10, true, (g_active_field == FIELD_HOST) ? 255 : 120, (g_active_field == FIELD_HOST) ? 215 : 60, 20);
    draw_prof_text(g_prof_ren, 346.0f, 115.0f, g_edit_profile.host, 255, 215, 60);

    draw_prof_text(g_prof_ren, 260.0f, 152.0f, "Port:", 255, 140, 20);
    draw_prof_rect(g_prof_ren, 340, 132, 120, 26, 32, 18, 10, true, (g_active_field == FIELD_PORT) ? 255 : 120, (g_active_field == FIELD_PORT) ? 215 : 60, 20);
    char port_buf[16]; snprintf(port_buf, sizeof(port_buf), "%d", g_edit_profile.port);
    draw_prof_text(g_prof_ren, 346.0f, 151.0f, port_buf, 255, 215, 60);

    /* Phosphor Color a fianco di Port (Y=132) */
    bool ph_disabled = (g_edit_profile.display_mode == 3);
    draw_prof_text(g_prof_ren, 480.0f, 151.0f, "Phosphor:", ph_disabled ? 120 : 255, ph_disabled ? 60 : 140, 20);
    const char *ph_labels[] = { "Default", "Green P31", "Amber P40", "White P4" };
    int ph_x = 570;
    for (int i = 0; i < 4; i++) {
        bool is_sel = (!ph_disabled && g_edit_profile.phosphor_color == i);
        draw_prof_button(g_prof_ren, ph_x, 132, 82, 26, ph_labels[i], is_sel);
        ph_x += 88;
    }

    /* Display Mode */
    draw_prof_text(g_prof_ren, 260.0f, 188.0f, "Mode:", 255, 140, 20);
    const char *modes[] = { "Real Plasma", "Crisp Mono", "Split Mono", "Crisp Color", "Real CRT" };
    int mode_x = 340;
    for (int m = 0; m < 5; m++) {
        draw_prof_button(g_prof_ren, mode_x, 168, 110, 26, modes[m], (g_edit_profile.display_mode == m));
        mode_x += 116;
    }

    /* Parametri Condizionali Plasma / CRT */
    int mode = g_edit_profile.display_mode;
    if (mode == 0 || mode == 2) {
        draw_prof_text(g_prof_ren, 260.0f, 222.0f, "Decay:", 204, 102, 0);
        const char *dec_labels[] = { "100ms", "200ms", "500ms", "1s", "2s", "5s" };
        int px = 340;
        for (int i = 0; i < 6; i++) {
            draw_prof_button(g_prof_ren, px, 204, 65, 24, dec_labels[i], (g_edit_profile.persistence_ms == g_decay_ms_vals[i]));
            px += 70;
        }

        draw_prof_text(g_prof_ren, 260.0f, 254.0f, "Distort:", 204, 102, 0);
        const char *dist_labels[] = { "None", "Barrel", "Cylindrical" };
        int dx = 340;
        for (int i = 0; i < 3; i++) {
            draw_prof_button(g_prof_ren, dx, 236, 110, 24, dist_labels[i], (g_edit_profile.plasma_distortion == i));
            dx += 116;
        }
    } else if (mode == 4) {
        /* CRT Beam a Y = 204 */
        draw_prof_text(g_prof_ren, 260.0f, 222.0f, "Beam:", 204, 102, 0);
        const char *bm_labels[] = { "Standard", "High", "Ultra" };
        int bx = 340;
        for (int i = 0; i < 3; i++) {
            draw_prof_button(g_prof_ren, bx, 204, 110, 24, bm_labels[i], (g_edit_profile.crt_beam_level == i));
            bx += 116;
        }

        /* CRT Persistence a Y = 236 */
        draw_prof_text(g_prof_ren, 260.0f, 254.0f, "Decay:", 204, 102, 0);
        const char *dec_labels[] = { "20ms", "200ms", "500ms", "1s", "2s", "5s" };
        int px = 340;
        for (int i = 0; i < 6; i++) {
            draw_prof_button(g_prof_ren, px, 236, 65, 24, dec_labels[i], (g_edit_profile.crt_persistence_ms == g_crt_decay_ms_vals[i]));
            px += 70;
        }

        /* CRT Distortion a Y = 268 */
        draw_prof_text(g_prof_ren, 260.0f, 286.0f, "Distort:", 204, 102, 0);
        const char *dist_labels[] = { "None", "Barrel", "Cylindrical" };
        int dx = 340;
        for (int i = 0; i < 3; i++) {
            draw_prof_button(g_prof_ren, dx, 268, 110, 24, dist_labels[i], (g_edit_profile.crt_distortion == i));
            dx += 116;
        }
    }

    /* Checkboxes */
    draw_prof_rect(g_prof_ren, 340, 305, 18, 18, 32, 18, 10, true, 190, 85, 12);
    if (g_edit_profile.full_screen) draw_prof_text(g_prof_ren, 344.0f, 320.0f, "X", 255, 215, 60);
    draw_prof_text(g_prof_ren, 366.0f, 320.0f, "Full Screen", 255, 140, 20);

    draw_prof_rect(g_prof_ren, 510, 305, 18, 18, 32, 18, 10, true, 190, 85, 12);
    if (g_edit_profile.is_default) draw_prof_text(g_prof_ren, 514.0f, 320.0f, "X", 255, 215, 60);
    draw_prof_text(g_prof_ren, 536.0f, 320.0f, "Default Profile", 255, 140, 20);

    draw_prof_rect(g_prof_ren, 680, 305, 18, 18, 32, 18, 10, true, 190, 85, 12);
    if (g_edit_profile.startup_script_enabled) draw_prof_text(g_prof_ren, 684.0f, 320.0f, "X", 255, 215, 60);
    draw_prof_text(g_prof_ren, 706.0f, 320.0f, "Startup Script", 255, 140, 20);

    /* Box Editor Script Multi-linea con Scroll */
    int total_lines = get_total_script_lines(g_edit_profile.startup_script);
    char hdr_str[128];
    snprintf(hdr_str, sizeof(hdr_str), "Script (Line %d of %d):", g_script_scroll_line + 1, total_lines);
    draw_prof_text(g_prof_ren, 260.0f, 356.0f, hdr_str, 255, 140, 20);

    draw_prof_rect(g_prof_ren, SCRIPT_BOX_X, SCRIPT_BOX_Y, SCRIPT_BOX_W, SCRIPT_BOX_H, 24, 14, 8, true,
                   (g_active_field == FIELD_SCRIPT) ? 255 : 96, (g_active_field == FIELD_SCRIPT) ? 215 : 42, 20);

    /* Disegno Righe Visibili dello Script */
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
            draw_prof_text(g_prof_ren, (float)(SCRIPT_BOX_X + 8), (float)line_y, line_buf, 230, 140, 30);

            if (g_active_field == FIELD_SCRIPT && current_line_num == cur_line_pos) {
                if ((SDL_GetTicks() / 500) % 2 == 0) {
                    float cur_x = (float)(SCRIPT_BOX_X + 8) + get_string_width_f(line_start, cur_col_pos);
                    draw_prof_rect(g_prof_ren, (int)cur_x, line_y - 14, 2, 16, 255, 215, 60, false, 0, 0, 0);
                }
            }
            drawn_lines++;
        }

        if (*sp == '\n') sp++;
        current_line_num++;
    }

    /* Scrollbar Grafica */
    int sb_x = SCRIPT_BOX_X + SCRIPT_BOX_W - 12;
    int sb_y = SCRIPT_BOX_Y + 2;
    int sb_h = SCRIPT_BOX_H - 4;
    draw_prof_rect(g_prof_ren, sb_x, sb_y, 8, sb_h, 34, 17, 6, true, 96, 42, 10);

    if (total_lines > SCRIPT_VISIBLE_LINES) {
        int thumb_h = (sb_h * SCRIPT_VISIBLE_LINES) / total_lines;
        if (thumb_h < 14) thumb_h = 14;
        int max_scroll = total_lines - SCRIPT_VISIBLE_LINES;
        int thumb_y = sb_y + (g_script_scroll_line * (sb_h - thumb_h)) / max_scroll;
        draw_prof_rect(g_prof_ren, sb_x, thumb_y, 8, thumb_h, 255, 140, 20, true, 255, 215, 60);
    }

    /* Pulsanti di Azione Inferiori */
    draw_prof_button_ex(g_prof_ren, 340, 550, 130, 32, "Set as Default", false, is_prof_btn_flashing(PROF_BTN_DEFAULT));
    draw_prof_button_ex(g_prof_ren, 485, 550, 100, 32, "Save", false, is_prof_btn_flashing(PROF_BTN_SAVE));
    draw_prof_button(g_prof_ren, 600, 550, 140, 32, "Connect Now", true);
    draw_prof_button(g_prof_ren, 755, 550, 90, 32, "Close", false);



        SDL_RenderPresent(g_prof_ren);
}
