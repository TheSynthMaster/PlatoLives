#include "linux_keymap.h"
#include "plato/plato_keyboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plato/stb_truetype.h"
#include "fonts/roboto_font_data.h"

#define K_WIN_W 960
#define K_WIN_H 620

static SDL_Window *g_kwin = NULL;
static SDL_Renderer *g_kren = NULL;
static stbtt_bakedchar g_kcdata[96];
static unsigned char *g_katlas = NULL;
static SDL_Texture *g_katlas_tex = NULL;
static bool g_kfont_ready = false;

static plato_keymap_manager_t *g_kmgr = NULL;
static int g_sel_prof = 0;
static int g_sel_bind = -1;
static int g_bind_scroll = 0;

typedef enum {
    KSTATE_NORMAL = 0,
    KSTATE_RECORD_KEY,
    KSTATE_PICK_ACTION,
    KSTATE_RENAME
} k_state_t;

static k_state_t g_kstate = KSTATE_NORMAL;
static plato_key_binding_t g_tmp_bind;
static char g_rename_name[64] = {0};
static char g_rename_desc[128] = {0};
static int  g_rename_field = 0; /* 0 = Name, 1 = Description */
static char g_char_buf[8] = {0};
static int g_pick_mode = 0; /* 0 = Character, 1 = PLATO Key */
static int g_pick_plato_sel = 0;
static char g_label_buf[32] = {0};
static bool g_is_editing_bind = false;
static int  g_pick_field = 0; /* 0 = Char/Key, 1 = Description/Label */

static void k_init_font(void) {
    if (g_kfont_ready || !g_kren) return;
    if (!g_katlas) g_katlas = (unsigned char *)malloc(512 * 512);
    stbtt_BakeFontBitmap(g_roboto_ttf, 0, 16.5f, g_katlas, 512, 512, 32, 96, g_kcdata);

    uint32_t *rgba = (uint32_t *)malloc(512 * 512 * 4);
    for (int i = 0; i < 512 * 512; i++) {
        rgba[i] = ((uint32_t)g_katlas[i] << 24) | 0x00FFFFFF;
    }
    g_katlas_tex = SDL_CreateTexture(g_kren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 512, 512);
    SDL_UpdateTexture(g_katlas_tex, NULL, rgba, 512 * 4);
    SDL_SetTextureBlendMode(g_katlas_tex, SDL_BLENDMODE_BLEND);
    free(rgba);
    g_kfont_ready = true;
}

static float k_get_str_w(const char *str) {
    if (!str) return 0;
    float w = 0;
    while (*str) {
        char c = *str++;
        if ((unsigned char)c >= 32 && (unsigned char)c < 128) w += g_kcdata[c - 32].xadvance;
    }
    return w;
}

static void k_draw_text(float x, float y, const char *str, uint8_t r, uint8_t g, uint8_t b) {
    if (!g_katlas_tex || !str) return;
    SDL_SetTextureColorMod(g_katlas_tex, r, g, b);
    while (*str) {
        char c = *str++;
        if ((unsigned char)c < 32 || (unsigned char)c >= 128) continue;
        const stbtt_bakedchar *bc = &g_kcdata[c - 32];
        SDL_Rect src = { bc->x0, bc->y0, bc->x1 - bc->x0, bc->y1 - bc->y0 };
        SDL_Rect dst = { (int)(x + bc->xoff + 0.5f), (int)(y + bc->yoff + 0.5f), src.w, src.h };
        SDL_RenderCopy(g_kren, g_katlas_tex, &src, &dst);
        x += bc->xadvance;
    }
}

static void k_draw_rect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, bool out, uint8_t or, uint8_t og, uint8_t ob) {
    SDL_Rect rc = { x, y, w, h };
    SDL_SetRenderDrawColor(g_kren, r, g, b, 255);
    SDL_RenderFillRect(g_kren, &rc);
    if (out) {
        SDL_SetRenderDrawColor(g_kren, or, og, ob, 255);
        SDL_RenderDrawRect(g_kren, &rc);
    }
}

static void k_draw_btn(int x, int y, int w, int h, const char *lbl, bool sel) {
    uint8_t bg_r = sel ? 96 : 32, bg_g = sel ? 42 : 20, bg_b = sel ? 10 : 12;
    uint8_t bd_r = sel ? 255 : 90, bd_g = sel ? 140 : 50, bd_b = sel ? 20 : 25;
    k_draw_rect(x, y, w, h, bg_r, bg_g, bg_b, true, bd_r, bd_g, bd_b);
    float tw = k_get_str_w(lbl);
    uint8_t fg_r = sel ? 255 : 210, fg_g = sel ? 215 : 190, fg_b = sel ? 60 : 170;
    k_draw_text(x + (w - tw)/2.0f, y + 18.0f, lbl, fg_r, fg_g, fg_b);
}

static void format_key_str(const plato_key_binding_t *b, char *out, size_t max_len) {
    char mods[64] = {0};
    if (b->modifiers & PLATO_KEY_MOD_CTRL)  strcat(mods, "Ctrl + ");
    if (b->modifiers & PLATO_KEY_MOD_ALT)   strcat(mods, "Alt + ");
    if (b->modifiers & PLATO_KEY_MOD_SHIFT) strcat(mods, "Shift + ");
    if (b->modifiers & PLATO_KEY_MOD_GUI)   strcat(mods, "Cmd + ");

    const char *k = "";
    char single[16] = {0};
    if (b->vkey != 0) {
        switch (b->vkey) {
            case PLATO_VKEY_UP:        k = "Up Arrow"; break;
            case PLATO_VKEY_DOWN:      k = "Down Arrow"; break;
            case PLATO_VKEY_LEFT:      k = "Left Arrow"; break;
            case PLATO_VKEY_RIGHT:     k = "Right Arrow"; break;
            case PLATO_VKEY_PAGEUP:    k = "Page Up"; break;
            case PLATO_VKEY_PAGEDOWN:  k = "Page Down"; break;
            case PLATO_VKEY_HOME:      k = "Home"; break;
            case PLATO_VKEY_END:       k = "End"; break;
            case PLATO_VKEY_RETURN:    k = "Return / Enter"; break;
            case PLATO_VKEY_BACKSPACE: k = "Backspace"; break;
            case PLATO_VKEY_ESCAPE:    k = "Escape"; break;
            case PLATO_VKEY_TAB:       k = "Tab"; break;
            case PLATO_VKEY_F1:        k = "F1"; break;
            case PLATO_VKEY_F2:        k = "F2"; break;
            case PLATO_VKEY_F3:        k = "F3"; break;
            case PLATO_VKEY_F4:        k = "F4"; break;
            case PLATO_VKEY_F5:        k = "F5"; break;
            case PLATO_VKEY_F6:        k = "F6"; break;
            case PLATO_VKEY_F7:        k = "F7"; break;
            case PLATO_VKEY_F8:        k = "F8"; break;
            case PLATO_VKEY_F9:        k = "F9"; break;
            case PLATO_VKEY_F10:       k = "F10"; break;
            case PLATO_VKEY_F11:       k = "F11"; break;
            case PLATO_VKEY_F12:       k = "F12"; break;
            case PLATO_VKEY_NUMPAD0:   k = "Numpad 0"; break;
            case PLATO_VKEY_NUMPAD1:   k = "Numpad 1"; break;
            case PLATO_VKEY_NUMPAD2:   k = "Numpad 2"; break;
            case PLATO_VKEY_NUMPAD3:   k = "Numpad 3"; break;
            case PLATO_VKEY_NUMPAD4:   k = "Numpad 4"; break;
            case PLATO_VKEY_NUMPAD5:   k = "Numpad 5"; break;
            case PLATO_VKEY_NUMPAD6:   k = "Numpad 6"; break;
            case PLATO_VKEY_NUMPAD7:   k = "Numpad 7"; break;
            case PLATO_VKEY_NUMPAD8:   k = "Numpad 8"; break;
            case PLATO_VKEY_NUMPAD9:   k = "Numpad 9"; break;
            default: snprintf(single, sizeof(single), "VKey %u", b->vkey); k = single; break;
        }
    } else if (b->codepoint != 0) {
        if (b->codepoint == ' ') k = "Space";
        else { snprintf(single, sizeof(single), "'%c'", (char)b->codepoint); k = single; }
    } else {
        k = "(None)";
    }
    snprintf(out, max_len, "%s%s", mods, k);
}

static void format_action_str(const plato_key_binding_t *b, char *out, size_t max_len) {
    if (b->type == PLATO_BIND_ACTION_RAW_BYTE) {
        char ch = (char)b->target.raw_byte;
        if (ch >= 32 && ch <= 126) snprintf(out, max_len, "Type '%c'", ch);
        else if (ch == 9) snprintf(out, max_len, "Type Tab (0x09)");
        else if (ch == 10 || ch == 13) snprintf(out, max_len, "Type Return");
        else if (ch == 27) snprintf(out, max_len, "Type Esc (0x1B)");
        else snprintf(out, max_len, "Raw Byte 0x%02X", b->target.raw_byte);
    } else if (b->type == PLATO_BIND_ACTION_PLATO_KEY) {
        const char *pk = "";
        switch ((plato_special_key_t)b->target.plato_key) {
            case PLATO_KEY_NEXT:   pk = "PLATO: NEXT"; break;
            case PLATO_KEY_BACK:   pk = "PLATO: BACK"; break;
            case PLATO_KEY_HELP:   pk = "PLATO: HELP"; break;
            case PLATO_KEY_LAB:    pk = "PLATO: LAB"; break;
            case PLATO_KEY_DATA:   pk = "PLATO: DATA"; break;
            case PLATO_KEY_STOP:   pk = "PLATO: STOP"; break;
            case PLATO_KEY_EDIT:   pk = "PLATO: EDIT"; break;
            case PLATO_KEY_COPY:   pk = "PLATO: COPY"; break;
            case PLATO_KEY_MICRO:  pk = "PLATO: MICRO"; break;
            case PLATO_KEY_FONT:   pk = "PLATO: FONT"; break;
            case PLATO_KEY_SUPER:  pk = "PLATO: SUPER (Up)"; break;
            case PLATO_KEY_SUB:    pk = "PLATO: SUB (Down)"; break;
            case PLATO_KEY_ACCESS: pk = "PLATO: ACCESS / TAB"; break;
            case PLATO_KEY_ERASE:  pk = "PLATO: ERASE (Left)"; break;
            case PLATO_KEY_ANS:    pk = "PLATO: ANS"; break;
            case PLATO_KEY_TERM:   pk = "PLATO: TERM"; break;
            case PLATO_KEY_SQUARE: pk = "PLATO: SQUARE"; break;
            default: pk = "PLATO Key"; break;
        }
        snprintf(out, max_len, "%s", pk);
    } else {
        snprintf(out, max_len, "(None)");
    }
}

void linux_keymap_show(plato_keymap_manager_t *mgr) {
    if (!mgr) return;
    g_kmgr = mgr;
    if (g_kwin) { SDL_RaiseWindow(g_kwin); return; }
    g_kwin = SDL_CreateWindow("Manage Keyboard Profiles", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, K_WIN_W, K_WIN_H, SDL_WINDOW_SHOWN);
    g_kren = SDL_CreateRenderer(g_kwin, -1, SDL_RENDERER_SOFTWARE);
    if (!g_kren) g_kren = SDL_CreateRenderer(g_kwin, -1, 0);
    g_kfont_ready = false;
    k_init_font();
    g_sel_prof = mgr->active_profile_index;
    g_sel_bind = -1;
    g_kstate = KSTATE_NORMAL;
}

bool linux_keymap_is_open(void) { return g_kwin != NULL; }

void linux_keymap_destroy(void) {
    if (g_katlas_tex) { SDL_DestroyTexture(g_katlas_tex); g_katlas_tex = NULL; }
    if (g_katlas) { free(g_katlas); g_katlas = NULL; }
    if (g_kren) { SDL_DestroyRenderer(g_kren); g_kren = NULL; }
    if (g_kwin) { SDL_DestroyWindow(g_kwin); g_kwin = NULL; }
}

static bool k_pt_in(int px, int py, int x, int y, int w, int h) { return (px >= x && px < x + w && py >= y && py < y + h); }

static void do_add_prof(void) {
    plato_keymap_profile_t p = {0};
    snprintf(p.name, sizeof(p.name), "Custom Profile %d", g_kmgr->profile_count);
    snprintf(p.description, sizeof(p.description), "User-defined key mappings");
    int idx = plato_keymap_add_profile(g_kmgr, &p);
    if (idx >= 0) { g_sel_prof = idx; plato_keymap_save_profile(&g_kmgr->profiles[idx]); }
}

static void do_dup_prof(void) {
    if (g_sel_prof < 0) return;
    char nn[64]; snprintf(nn, sizeof(nn), "%s Copy", g_kmgr->profiles[g_sel_prof].name);
    if (plato_keymap_duplicate_profile(g_kmgr, g_sel_prof, nn)) {
        g_sel_prof = g_kmgr->profile_count - 1;
        plato_keymap_save_profile(&g_kmgr->profiles[g_sel_prof]);
    }
}

void linux_keymap_handle_event(const SDL_Event *ev) {
    if (!g_kwin) return;
    uint32_t wid = SDL_GetWindowID(g_kwin);

    if (ev->type == SDL_WINDOWEVENT && ev->window.windowID == wid) {
        if (ev->window.event == SDL_WINDOWEVENT_CLOSE) linux_keymap_destroy();
    } else if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.windowID == wid) {
        int x = ev->button.x, y = ev->button.y;

        if (g_kstate == KSTATE_RENAME) {
            int dw = 440, dh = 188;
            int dx = K_WIN_W/2 - dw/2;
            int dy = K_WIN_H/2 - dh/2;

            if (k_pt_in(x, y, dx + 20, dy + 48, 400, 28)) {
                g_rename_field = 0;
                return;
            }
            if (k_pt_in(x, y, dx + 20, dy + 101, 400, 28)) {
                g_rename_field = 1;
                return;
            }
            if (k_pt_in(x, y, dx + 220, dy + 146, 95, 28)) {
                if (strlen(g_rename_name) > 0) {
                    plato_keymap_update_profile_info(g_kmgr, g_sel_prof, g_rename_name, g_rename_desc);
                }
                g_kstate = KSTATE_NORMAL;
                return;
            }
            if (k_pt_in(x, y, dx + 325, dy + 146, 95, 28)) {
                g_kstate = KSTATE_NORMAL;
                return;
            }
            return;
        } else if (g_kstate == KSTATE_PICK_ACTION) {
            int mx = K_WIN_W/2 - 260;
            int my = K_WIN_H/2 - 190;

            // Modal Tabs: [ Custom Character ] vs [ PLATO Function Key ]
            if (k_pt_in(x, y, mx + 60, my + 40, 195, 26)) { g_pick_mode = 0; g_pick_field = 0; }
            if (k_pt_in(x, y, mx + 265, my + 40, 195, 26)) { g_pick_mode = 1; }

            if (g_pick_mode == 1) {
                // Selezione Tasto PLATO
                for (int i = 0; i < 17; i++) {
                    int bx = mx + 47 + (i % 3) * 144;
                    int by = my + 75 + (i / 3) * 28;
                    if (k_pt_in(x, y, bx, by, 138, 24)) {
                        g_pick_plato_sel = i;
                        static const char* pnames[] = {
                            "NEXT", "BACK", "HELP", "LAB", "DATA", "STOP", "EDIT", "COPY",
                            "MICRO", "FONT", "SUPER", "SUB", "ERASE", "ACCESS/TAB", "ANS", "TERM", "SQUARE"
                        };
                        if (strlen(g_label_buf) == 0) {
                            snprintf(g_label_buf, sizeof(g_label_buf), "PLATO: %s", pnames[i]);
                        }
                    }
                }
                // Click su box Descrizione
                if (k_pt_in(x, y, mx + 47, my + 265, 426, 28)) g_pick_field = 1;

                // Click su Save / Assign
                if (k_pt_in(x, y, mx + 150, my + 310, 100, 30)) {
                    static const plato_special_key_t pvals[] = {
                        PLATO_KEY_NEXT, PLATO_KEY_BACK, PLATO_KEY_HELP, PLATO_KEY_LAB, PLATO_KEY_DATA, PLATO_KEY_STOP,
                        PLATO_KEY_EDIT, PLATO_KEY_COPY, PLATO_KEY_MICRO, PLATO_KEY_FONT, PLATO_KEY_SUPER, PLATO_KEY_SUB,
                        PLATO_KEY_ERASE, PLATO_KEY_ACCESS, PLATO_KEY_ANS, PLATO_KEY_TERM, PLATO_KEY_SQUARE
                    };
                    static const char* pnames[] = {
                        "NEXT", "BACK", "HELP", "LAB", "DATA", "STOP", "EDIT", "COPY",
                        "MICRO", "FONT", "SUPER", "SUB", "ERASE", "ACCESS/TAB", "ANS", "TERM", "SQUARE"
                    };
                    g_tmp_bind.type = PLATO_BIND_ACTION_PLATO_KEY;
                    g_tmp_bind.target.plato_key = pvals[g_pick_plato_sel];
                    if (strlen(g_label_buf) > 0) {
                        snprintf(g_tmp_bind.label, sizeof(g_tmp_bind.label), "%s", g_label_buf);
                    } else {
                        snprintf(g_tmp_bind.label, sizeof(g_tmp_bind.label), "PLATO: %s", pnames[g_pick_plato_sel]);
                    }
                    if (g_is_editing_bind && g_sel_bind >= 0) {
                        plato_keymap_profile_set_binding(&g_kmgr->profiles[g_sel_prof], g_sel_bind, &g_tmp_bind);
                    } else {
                        plato_keymap_profile_add_binding(&g_kmgr->profiles[g_sel_prof], &g_tmp_bind);
                    }
                    plato_keymap_save_profile(&g_kmgr->profiles[g_sel_prof]);
                    g_kstate = KSTATE_NORMAL;
                    return;
                }
            } else {
                // Click su box Carattere
                if (k_pt_in(x, y, mx + 230, my + 120, 60, 30)) g_pick_field = 0;
                // Click su box Descrizione
                if (k_pt_in(x, y, mx + 60, my + 185, 400, 28)) g_pick_field = 1;

                // Click su Save / Assign
                if (k_pt_in(x, y, mx + 150, my + 235, 100, 30) && strlen(g_char_buf) > 0) {
                    g_tmp_bind.type = PLATO_BIND_ACTION_RAW_BYTE;
                    g_tmp_bind.target.raw_byte = g_char_buf[0];
                    if (strlen(g_label_buf) > 0) {
                        snprintf(g_tmp_bind.label, sizeof(g_tmp_bind.label), "%s", g_label_buf);
                    } else {
                        snprintf(g_tmp_bind.label, sizeof(g_tmp_bind.label), "Type '%c'", g_char_buf[0]);
                    }
                    if (g_is_editing_bind && g_sel_bind >= 0) {
                        plato_keymap_profile_set_binding(&g_kmgr->profiles[g_sel_prof], g_sel_bind, &g_tmp_bind);
                    } else {
                        plato_keymap_profile_add_binding(&g_kmgr->profiles[g_sel_prof], &g_tmp_bind);
                    }
                    plato_keymap_save_profile(&g_kmgr->profiles[g_sel_prof]);
                    g_kstate = KSTATE_NORMAL;
                    return;
                }
            }

            // Pulsante Cancel (comune ad entrambi i mode)
            int cancel_y = (g_pick_mode == 1) ? (my + 310) : (my + 235);
            if (k_pt_in(x, y, mx + 270, cancel_y, 100, 30)) g_kstate = KSTATE_NORMAL;
        } else if (g_kstate == KSTATE_NORMAL) {
            // Selezione Profilo a sinistra (y: 55..550)
            if (k_pt_in(x, y, 20, 55, 250, 495)) {
                int idx = (y - 58) / 26;
                if (idx >= 0 && idx < g_kmgr->profile_count) {
                    g_sel_prof = idx;
                    g_sel_bind = -1;
                    g_bind_scroll = 0;
                }
            }

            // Selezione Binding nella tabella (y: 104..515)
            if (k_pt_in(x, y, 290, 104, 650, 410)) {
                int r = (y - 104) / 26 + g_bind_scroll;
                if (r >= 0 && r < g_kmgr->profiles[g_sel_prof].binding_count) g_sel_bind = r;
            }

            // Pulsanti Sotto Lista Profili (y: 568..596)
            if (k_pt_in(x, y, 20, 568, 55, 28)) do_add_prof();
            if (k_pt_in(x, y, 79, 568, 76, 28)) do_dup_prof();
            if (k_pt_in(x, y, 159, 568, 62, 28)) {
                if (!g_kmgr->profiles[g_sel_prof].is_builtin) {
                    snprintf(g_rename_name, sizeof(g_rename_name), "%s", g_kmgr->profiles[g_sel_prof].name);
                    snprintf(g_rename_desc, sizeof(g_rename_desc), "%s", g_kmgr->profiles[g_sel_prof].description);
                    g_rename_field = 0;
                    g_kstate = KSTATE_RENAME;
                }
            }
            if (k_pt_in(x, y, 225, 568, 45, 28)) {
                if (!g_kmgr->profiles[g_sel_prof].is_builtin) {
                    plato_keymap_delete_profile(g_kmgr, g_sel_prof);
                    g_sel_prof = g_kmgr->active_profile_index;
                }
            }

            // Pulsanti Sotto Tabella Mapping (y: 528..558)
            if (k_pt_in(x, y, 290, 528, 185, 30) && !g_kmgr->profiles[g_sel_prof].is_builtin) {
                g_is_editing_bind = false;
                g_label_buf[0] = 0;
                g_char_buf[0] = 0;
                g_pick_field = 0;
                g_kstate = KSTATE_RECORD_KEY;
            }
            if (k_pt_in(x, y, 485, 528, 120, 30)) {
                if (!g_kmgr->profiles[g_sel_prof].is_builtin && g_sel_bind >= 0 && g_sel_bind < g_kmgr->profiles[g_sel_prof].binding_count) {
                    g_is_editing_bind = true;
                    g_tmp_bind = g_kmgr->profiles[g_sel_prof].bindings[g_sel_bind];
                    snprintf(g_label_buf, sizeof(g_label_buf), "%s", g_tmp_bind.label);
                    if (g_tmp_bind.type == PLATO_BIND_ACTION_RAW_BYTE) {
                        g_pick_mode = 0;
                        g_char_buf[0] = (char)g_tmp_bind.target.raw_byte;
                        g_char_buf[1] = 0;
                    } else {
                        g_pick_mode = 1;
                        static const plato_special_key_t pvals_find[] = {
                            PLATO_KEY_NEXT, PLATO_KEY_BACK, PLATO_KEY_HELP, PLATO_KEY_LAB, PLATO_KEY_DATA, PLATO_KEY_STOP,
                            PLATO_KEY_EDIT, PLATO_KEY_COPY, PLATO_KEY_MICRO, PLATO_KEY_FONT, PLATO_KEY_SUPER, PLATO_KEY_SUB,
                            PLATO_KEY_ERASE, PLATO_KEY_ACCESS, PLATO_KEY_ANS, PLATO_KEY_TERM, PLATO_KEY_SQUARE
                        };
                        g_pick_plato_sel = 0;
                        for (int pi = 0; pi < 17; pi++) {
                            if (pvals_find[pi] == g_tmp_bind.target.plato_key) { g_pick_plato_sel = pi; break; }
                        }
                    }
                    g_pick_field = 0;
                    g_kstate = KSTATE_PICK_ACTION;
                }
            }
            if (k_pt_in(x, y, 615, 528, 140, 30) && g_sel_bind >= 0 && !g_kmgr->profiles[g_sel_prof].is_builtin) {
                plato_keymap_profile_remove_binding(&g_kmgr->profiles[g_sel_prof], g_sel_bind);
                plato_keymap_save_profile(&g_kmgr->profiles[g_sel_prof]);
                g_sel_bind = -1;
            }

            // Pulsanti Bottom Bar Destra (y: 568..598)
            if (k_pt_in(x, y, 680, 568, 155, 30)) {
                plato_keymap_set_active_by_index(g_kmgr, g_sel_prof);
                plato_keymap_save_all(g_kmgr);
            }
            if (k_pt_in(x, y, 850, 568, 90, 30)) linux_keymap_destroy();
        }
    } else if (ev->type == SDL_KEYDOWN && ev->key.windowID == wid) {
        if (ev->key.keysym.sym == SDLK_ESCAPE) {
            if (g_kstate != KSTATE_NORMAL) g_kstate = KSTATE_NORMAL;
            else linux_keymap_destroy();
            return;
        }

        if (g_kstate == KSTATE_RENAME) {
            if (ev->key.keysym.sym == SDLK_TAB) {
                g_rename_field = 1 - g_rename_field;
            } else if (ev->key.keysym.sym == SDLK_RETURN) {
                if (strlen(g_rename_name) > 0) {
                    plato_keymap_update_profile_info(g_kmgr, g_sel_prof, g_rename_name, g_rename_desc);
                }
                g_kstate = KSTATE_NORMAL;
            } else if (ev->key.keysym.sym == SDLK_BACKSPACE) {
                char *buf = (g_rename_field == 0) ? g_rename_name : g_rename_desc;
                size_t l = strlen(buf);
                if (l > 0) buf[l-1] = '\0';
            }
        } else if (g_kstate == KSTATE_PICK_ACTION) {
            if (ev->key.keysym.sym == SDLK_TAB) {
                g_pick_field = 1 - g_pick_field;
            } else if (ev->key.keysym.sym == SDLK_BACKSPACE) {
                if (g_pick_field == 1) {
                    size_t l = strlen(g_label_buf);
                    if (l > 0) g_label_buf[l-1] = '\0';
                } else if (g_pick_mode == 0) {
                    g_char_buf[0] = '\0';
                }
            } else if (ev->key.keysym.sym == SDLK_RETURN) {
                if (g_pick_mode == 0 && strlen(g_char_buf) > 0) {
                    g_tmp_bind.type = PLATO_BIND_ACTION_RAW_BYTE;
                    g_tmp_bind.target.raw_byte = g_char_buf[0];
                    if (strlen(g_label_buf) > 0) {
                        snprintf(g_tmp_bind.label, sizeof(g_tmp_bind.label), "%s", g_label_buf);
                    } else {
                        snprintf(g_tmp_bind.label, sizeof(g_tmp_bind.label), "Type '%c'", g_char_buf[0]);
                    }
                    if (g_is_editing_bind && g_sel_bind >= 0) {
                        plato_keymap_profile_set_binding(&g_kmgr->profiles[g_sel_prof], g_sel_bind, &g_tmp_bind);
                    } else {
                        plato_keymap_profile_add_binding(&g_kmgr->profiles[g_sel_prof], &g_tmp_bind);
                    }
                    plato_keymap_save_profile(&g_kmgr->profiles[g_sel_prof]);
                    g_kstate = KSTATE_NORMAL;
                } else if (g_pick_mode == 1) {
                    static const plato_special_key_t pvals[] = {
                        PLATO_KEY_NEXT, PLATO_KEY_BACK, PLATO_KEY_HELP, PLATO_KEY_LAB, PLATO_KEY_DATA, PLATO_KEY_STOP,
                        PLATO_KEY_EDIT, PLATO_KEY_COPY, PLATO_KEY_MICRO, PLATO_KEY_FONT, PLATO_KEY_SUPER, PLATO_KEY_SUB,
                        PLATO_KEY_ERASE, PLATO_KEY_ACCESS, PLATO_KEY_ANS, PLATO_KEY_TERM, PLATO_KEY_SQUARE
                    };
                    static const char* pnames[] = {
                        "NEXT", "BACK", "HELP", "LAB", "DATA", "STOP", "EDIT", "COPY",
                        "MICRO", "FONT", "SUPER", "SUB", "ERASE", "ACCESS/TAB", "ANS", "TERM", "SQUARE"
                    };
                    g_tmp_bind.type = PLATO_BIND_ACTION_PLATO_KEY;
                    g_tmp_bind.target.plato_key = pvals[g_pick_plato_sel];
                    if (strlen(g_label_buf) > 0) {
                        snprintf(g_tmp_bind.label, sizeof(g_tmp_bind.label), "%s", g_label_buf);
                    } else {
                        snprintf(g_tmp_bind.label, sizeof(g_tmp_bind.label), "PLATO: %s", pnames[g_pick_plato_sel]);
                    }
                    if (g_is_editing_bind && g_sel_bind >= 0) {
                        plato_keymap_profile_set_binding(&g_kmgr->profiles[g_sel_prof], g_sel_bind, &g_tmp_bind);
                    } else {
                        plato_keymap_profile_add_binding(&g_kmgr->profiles[g_sel_prof], &g_tmp_bind);
                    }
                    plato_keymap_save_profile(&g_kmgr->profiles[g_sel_prof]);
                    g_kstate = KSTATE_NORMAL;
                }
            }
        } else if (g_kstate == KSTATE_RECORD_KEY) {
            memset(&g_tmp_bind, 0, sizeof(g_tmp_bind));
            SDL_Keymod mod = SDL_GetModState();
            if (mod & KMOD_SHIFT) g_tmp_bind.modifiers |= PLATO_KEY_MOD_SHIFT;
            if (mod & KMOD_CTRL)  g_tmp_bind.modifiers |= PLATO_KEY_MOD_CTRL;
            if (mod & KMOD_ALT)   g_tmp_bind.modifiers |= PLATO_KEY_MOD_ALT;
            if (mod & KMOD_GUI)   g_tmp_bind.modifiers |= PLATO_KEY_MOD_GUI;

            uint16_t vk = 0; uint32_t cp = 0;
            switch (ev->key.keysym.sym) {
                case SDLK_UP: vk = PLATO_VKEY_UP; break;
                case SDLK_DOWN: vk = PLATO_VKEY_DOWN; break;
                case SDLK_LEFT: vk = PLATO_VKEY_LEFT; break;
                case SDLK_RIGHT: vk = PLATO_VKEY_RIGHT; break;
                case SDLK_RETURN: vk = PLATO_VKEY_RETURN; break;
                case SDLK_TAB: vk = PLATO_VKEY_TAB; break;
                case SDLK_BACKSPACE: vk = PLATO_VKEY_BACKSPACE; break;
                case SDLK_PAGEUP: vk = PLATO_VKEY_PAGEUP; break;
                case SDLK_PAGEDOWN: vk = PLATO_VKEY_PAGEDOWN; break;
                case SDLK_HOME: vk = PLATO_VKEY_HOME; break;
                case SDLK_END: vk = PLATO_VKEY_END; break;
                case SDLK_KP_8: vk = PLATO_VKEY_NUMPAD8; break;
                case SDLK_KP_2: vk = PLATO_VKEY_NUMPAD2; break;
                case SDLK_KP_4: vk = PLATO_VKEY_NUMPAD4; break;
                case SDLK_KP_6: vk = PLATO_VKEY_NUMPAD6; break;
                case SDLK_KP_7: vk = PLATO_VKEY_NUMPAD7; break;
                case SDLK_KP_9: vk = PLATO_VKEY_NUMPAD9; break;
                case SDLK_KP_1: vk = PLATO_VKEY_NUMPAD1; break;
                case SDLK_KP_3: vk = PLATO_VKEY_NUMPAD3; break;
                case SDLK_KP_5: vk = PLATO_VKEY_NUMPAD5; break;
                default:
                    if (ev->key.keysym.sym >= SDLK_F1 && ev->key.keysym.sym <= SDLK_F12) {
                        vk = PLATO_VKEY_F1 + (ev->key.keysym.sym - SDLK_F1);
                    } else if (ev->key.keysym.sym >= 32 && ev->key.keysym.sym <= 126) {
                        cp = ev->key.keysym.sym;
                    }
                    break;
            }
            g_tmp_bind.vkey = vk;
            g_tmp_bind.codepoint = cp;
            g_kstate = KSTATE_PICK_ACTION;
            g_is_editing_bind = false;
            g_label_buf[0] = '\0';
            g_pick_field = 0;
            g_pick_mode = (vk != 0 && (vk < PLATO_VKEY_NUMPAD0 || vk > PLATO_VKEY_NUMPAD9)) ? 1 : 0;
            if (cp >= 32 && cp <= 126) {
                g_char_buf[0] = (char)cp;
                g_char_buf[1] = '\0';
            } else {
                g_char_buf[0] = '\0';
            }
        }
    } else if (ev->type == SDL_TEXTINPUT && ev->text.windowID == wid) {
        if (g_kstate == KSTATE_RENAME) {
            char *buf = (g_rename_field == 0) ? g_rename_name : g_rename_desc;
            size_t max_len = (g_rename_field == 0) ? sizeof(g_rename_name) : sizeof(g_rename_desc);
            if (strlen(buf) + strlen(ev->text.text) < max_len) {
                strcat(buf, ev->text.text);
            }
        } else if (g_kstate == KSTATE_PICK_ACTION) {
            if (g_pick_field == 1) {
                if (strlen(g_label_buf) + strlen(ev->text.text) < sizeof(g_label_buf)) {
                    strcat(g_label_buf, ev->text.text);
                }
            } else if (g_pick_mode == 0) {
                snprintf(g_char_buf, sizeof(g_char_buf), "%s", ev->text.text);
            }
        }
    } else if (ev->type == SDL_MOUSEWHEEL && ev->wheel.windowID == wid) {
        if (g_kstate == KSTATE_NORMAL) {
            g_bind_scroll -= ev->wheel.y * 2;
            if (g_bind_scroll < 0) g_bind_scroll = 0;
        }
    }
}

void linux_keymap_render(void) {
    if (!g_kwin || !g_kren) return;
    SDL_SetRenderDrawColor(g_kren, 16, 12, 10, 255);
    SDL_RenderClear(g_kren);

    // =========================================================
    // 1. HEADER SINISTRO & DESTRO
    // =========================================================
    k_draw_text(20, 36, "PROFILES", 255, 110, 0);

    plato_keymap_profile_t *p = &g_kmgr->profiles[g_sel_prof];
    char ptitle[128];
    snprintf(ptitle, sizeof(ptitle), "%s %s", p->name, p->is_builtin ? "[Built-in / Read-Only]" : "[User Defined]");
    k_draw_text(290, 35, ptitle, 255, 110, 0);
    k_draw_text(290, 58, p->description, 160, 160, 160);

    // =========================================================
    // 2. PANNELLO SINISTRO: LISTA PROFILI
    // =========================================================
    k_draw_rect(20, 55, 250, 500, 24, 18, 14, true, 56, 26, 0);

    for (int i = 0; i < g_kmgr->profile_count; i++) {
        int ry = 58 + i * 26;
        if (ry + 26 > 550) break;

        bool is_sel = (i == g_sel_prof);
        if (is_sel) {
            /* Highlight perfettamente coincidente con la riga */
            k_draw_rect(21, ry, 248, 25, 96, 42, 10, true, 255, 140, 20);
        }

        char plabel[128];
        snprintf(plabel, sizeof(plabel), "%s%s", g_kmgr->profiles[i].name, g_kmgr->profiles[i].is_builtin ? " (Built-in)" : "");

        uint8_t tr = is_sel ? 255 : (i == g_kmgr->active_profile_index ? 255 : 210);
        uint8_t tg = is_sel ? 215 : (i == g_kmgr->active_profile_index ? 140 : 190);
        uint8_t tb = is_sel ? 60  : (i == g_kmgr->active_profile_index ? 20  : 170);

        /* Baseline corretta a ry + 18.0f */
        k_draw_text(26, ry + 18.0f, plabel, tr, tg, tb);
    }

    // Pulsanti Sotto Lista Profili (Identici a macOS: New, Duplicate, Rename, Del)
    k_draw_btn(20, 568, 55, 28, "New", false);
    k_draw_btn(79, 568, 76, 28, "Duplicate", false);
    k_draw_btn(159, 568, 62, 28, "Rename", false);
    k_draw_btn(225, 568, 45, 28, "Del", false);

    // =========================================================
    // 3. PANNELLO DESTRO: TABELLA MAPPINGS 3 COLONNE
    // =========================================================
    k_draw_rect(290, 75, 650, 440, 24, 18, 14, true, 56, 26, 0);

    // Header Tabella con separatore (y: 75..101)
    k_draw_rect(291, 76, 648, 26, 34, 22, 14, false, 0, 0, 0);
    k_draw_text(298, 94.0f, "Physical Key / Combo", 210, 190, 170);
    k_draw_text(485, 94.0f, "Target Action", 210, 190, 170);
    k_draw_text(675, 94.0f, "Description", 210, 190, 170);
    k_draw_rect(291, 102, 648, 1, 56, 26, 0, false, 0, 0, 0);

    // Righe Bindings (y: 104..510)
    for (int i = 0; i < p->binding_count; i++) {
        int r_idx = i - g_bind_scroll;
        if (r_idx < 0 || r_idx >= 15) continue;
        int ry = 104 + r_idx * 26;

        bool is_sel_b = (i == g_sel_bind);
        if (is_sel_b) {
            k_draw_rect(291, ry, 648, 25, 96, 42, 10, true, 255, 140, 20);
        }

        char kstr[64];
        char actstr[64];
        format_key_str(&p->bindings[i], kstr, sizeof(kstr));
        format_action_str(&p->bindings[i], actstr, sizeof(actstr));

        /* Colonna 1: Physical Key (Ambra acceso) */
        k_draw_text(298, ry + 18.0f, kstr, 255, 160, 50);

        /* Colonna 2: Target Action (Bianco/Grigio chiaro) */
        k_draw_text(485, ry + 18.0f, actstr, 225, 220, 215);

        /* Colonna 3: Description (Grigio tenue) */
        k_draw_text(675, ry + 18.0f, p->bindings[i].label, 150, 145, 140);
    }

    // Pulsanti Sotto Tabella Mapping (Identici a macOS)
    k_draw_btn(290, 528, 185, 30, "◉ Press Key to Bind...", false);
    k_draw_btn(485, 528, 120, 30, "✎ Edit Binding", false);
    k_draw_btn(615, 528, 140, 30, "- Remove Binding", false);

    // =========================================================
    // 4. BOTTOM BAR: STATO & CONTROLLI ATTIVAZIONE
    // =========================================================
    if (p->is_builtin) {
        k_draw_text(290, 588.0f, "Built-in profile is read-only. Click 'Duplicate' to customize.", 255, 140, 20);
    } else {
        char count_msg[64];
        snprintf(count_msg, sizeof(count_msg), "%d custom key bindings configured.", p->binding_count);
        k_draw_text(290, 588.0f, count_msg, 255, 140, 20);
    }

    bool is_act = (g_sel_prof == g_kmgr->active_profile_index);
    k_draw_btn(680, 568, 155, 30, is_act ? "✓ Active Profile" : "Set as Active Profile", is_act);
    k_draw_btn(850, 568, 90, 30, "Close", false);

    // =========================================================
    // 5. MODALI OVERLAY (RENAME, RECORDING, UNIVERSAL PICKER)
    // =========================================================
    if (g_kstate == KSTATE_RENAME) {
        int dw = 440, dh = 188;
        int dx = K_WIN_W/2 - dw/2;
        int dy = K_WIN_H/2 - dh/2;

        k_draw_rect(dx, dy, dw, dh, 34, 17, 6, true, 255, 140, 20);
        k_draw_text(dx + 20, dy + 25, "Edit Profile", 255, 110, 0);

        // Campo 1: Nome
        k_draw_text(dx + 20, dy + 42, "Profile Name:", 210, 200, 190);
        bool sel_name = (g_rename_field == 0);
        k_draw_rect(dx + 20, dy + 48, 400, 28, 18, 12, 8, true, sel_name ? 255 : 80, sel_name ? 140 : 40, sel_name ? 20 : 10);
        char dname[80]; snprintf(dname, sizeof(dname), "%s%s", g_rename_name, sel_name ? "_" : "");
        k_draw_text(dx + 26, dy + 68, dname, 255, 255, 255);

        // Campo 2: Descrizione
        k_draw_text(dx + 20, dy + 95, "Description:", 210, 200, 190);
        bool sel_desc = (g_rename_field == 1);
        k_draw_rect(dx + 20, dy + 101, 400, 28, 18, 12, 8, true, sel_desc ? 255 : 80, sel_desc ? 140 : 40, sel_desc ? 20 : 10);
        char ddesc[150]; snprintf(ddesc, sizeof(ddesc), "%s%s", g_rename_desc, sel_desc ? "_" : "");
        k_draw_text(dx + 26, dy + 121, ddesc, 255, 255, 255);

        // Pulsanti e scorciatoie
        k_draw_text(dx + 20, dy + 165, "(Tab: Switch, Enter: Save)", 140, 135, 130);
        k_draw_btn(dx + 220, dy + 146, 95, 28, "Save", true);
        k_draw_btn(dx + 325, dy + 146, 95, 28, "Cancel", false);
    } else if (g_kstate == KSTATE_RECORD_KEY) {
        k_draw_rect(K_WIN_W/2 - 250, K_WIN_H/2 - 50, 500, 100, 180, 45, 0, true, 255, 120, 0);
        k_draw_text(K_WIN_W/2 - 215, K_WIN_H/2 - 10, "◉ PRESS ANY KEY OR COMBINATION", 255, 255, 255);
        k_draw_text(K_WIN_W/2 - 90,  K_WIN_H/2 + 20, "(Press Esc to Cancel)", 255, 215, 180);
    } else if (g_kstate == KSTATE_PICK_ACTION) {
        int mx = K_WIN_W/2 - 260;
        int my = K_WIN_H/2 - 190;
        int mh = (g_pick_mode == 1) ? 360 : 285;

        k_draw_rect(mx, my, 520, mh, 28, 16, 8, true, 255, 140, 20);
        k_draw_text(mx + 20, my + 25, g_is_editing_bind ? "EDIT TARGET ACTION" : "ASSIGN TARGET ACTION", 255, 110, 0);

        // Tabs
        k_draw_btn(mx + 60, my + 40, 195, 26, "Custom Character", g_pick_mode == 0);
        k_draw_btn(mx + 265, my + 40, 195, 26, "PLATO Function Key", g_pick_mode == 1);

        if (g_pick_mode == 1) {
            const char* pkeys[] = {
                "NEXT", "BACK", "HELP", "LAB", "DATA", "STOP", "EDIT", "COPY",
                "MICRO", "FONT", "SUPER", "SUB", "ERASE", "TAB", "ANS", "TERM", "SQUARE"
            };
            for (int i = 0; i < 17; i++) {
                int bx = mx + 47 + (i % 3) * 144;
                int by = my + 75 + (i / 3) * 28;
                k_draw_btn(bx, by, 138, 24, pkeys[i], g_pick_plato_sel == i);
            }

            k_draw_text(mx + 47, my + 258, "Description / Action Label (Optional):", 210, 200, 190);
            bool sel_lbl = (g_pick_field == 1);
            k_draw_rect(mx + 47, my + 265, 426, 28, 18, 12, 8, true, sel_lbl ? 255 : 80, sel_lbl ? 140 : 40, sel_lbl ? 20 : 10);
            char dlbl[64]; snprintf(dlbl, sizeof(dlbl), "%s%s", g_label_buf, sel_lbl ? "_" : "");
            k_draw_text(mx + 53, my + 285, dlbl, 255, 255, 255);

            k_draw_btn(mx + 150, my + 310, 100, 30, g_is_editing_bind ? "Save" : "Assign", true);
            k_draw_btn(mx + 270, my + 310, 100, 30, "Cancel", false);
        } else {
            k_draw_text(mx + 47, my + 95, "Type any character or symbol to send:", 210, 200, 190);
            k_draw_text(mx + 47, my + 115, "(Letters, numbers, symbols: +, -, ;, @, etc.)", 150, 145, 140);

            bool sel_chr = (g_pick_field == 0);
            k_draw_rect(mx + 230, my + 120, 60, 30, 10, 10, 10, true, sel_chr ? 255 : 80, sel_chr ? 140 : 40, sel_chr ? 20 : 10);
            char dchr[16]; snprintf(dchr, sizeof(dchr), "%s%s", g_char_buf, sel_chr ? "_" : "");
            k_draw_text(mx + 254, my + 142, dchr, 255, 255, 255);

            k_draw_text(mx + 60, my + 175, "Description / Action Label (Optional):", 210, 200, 190);
            bool sel_lbl = (g_pick_field == 1);
            k_draw_rect(mx + 60, my + 185, 400, 28, 18, 12, 8, true, sel_lbl ? 255 : 80, sel_lbl ? 140 : 40, sel_lbl ? 20 : 10);
            char dlbl[64]; snprintf(dlbl, sizeof(dlbl), "%s%s", g_label_buf, sel_lbl ? "_" : "");
            k_draw_text(mx + 66, my + 205, dlbl, 255, 255, 255);

            k_draw_btn(mx + 150, my + 235, 100, 30, g_is_editing_bind ? "Save" : "Assign", true);
            k_draw_btn(mx + 270, my + 235, 100, 30, "Cancel", false);
        }
    }

    SDL_RenderPresent(g_kren);
}
