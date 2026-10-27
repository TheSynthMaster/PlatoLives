#include "linux_menu.h"
#include "linux_theme.h"
#include "linux_hud.h"
#include "linux_gl.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GL_SILENCE_DEPRECATION
#if defined(__APPLE__)
#else
#define GL_GLEXT_PROTOTYPES
#endif

#include "plato/stb_truetype.h"
#include "fonts/roboto_font_data.h"

/* Palette Dark Amber Canonica PLATO (Endianness Little-Endian Little-Safe) */
#define COLOR_RGBA(r, g, b, a) ((uint32_t)(r) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16) | ((uint32_t)(a) << 24))

#define COL_BG           (linux_theme_current().rgba_bg_dark)
#define COL_BAR_BG       (linux_theme_current().rgba_bg_box)
#define COL_BORDER       (linux_theme_current().rgba_border)
#define COL_HOVER        (linux_theme_current().rgba_hover_bg)
#define COL_HOVER_BORDER (linux_theme_current().rgba_bright)
#define COL_TEXT         (linux_theme_current().rgba_normal)
#define COL_TEXT_HI      (linux_theme_current().rgba_bright)
#define COL_TEXT_DIM     (linux_theme_current().rgba_disabled)
#define COL_SHORTCUT     (linux_theme_current().rgba_dim)
#define COL_SEPARATOR    (linux_theme_current().rgba_border)
#define DROPDOWN_GAP     3                             /* Separatore menu-dropdown */

/* Motore TrueType ad Anti-Aliasing Vettoriale Integrato */
static unsigned char g_font_atlas[512 * 512];
static stbtt_bakedchar g_cdata[96]; /* ASCII 32..126 */
static bool g_font_ready = false;

static void init_font(void) {
    if (g_font_ready) return;
    stbtt_BakeFontBitmap(g_roboto_ttf, 0, 17.5f, g_font_atlas, 512, 512, 32, 96, g_cdata);
    g_font_ready = true;
}

static int get_char_width(char c) {
    init_font();
    if ((unsigned char)c < 32 || (unsigned char)c >= 128) return 6;
    return (int)(g_cdata[c - 32].xadvance + 0.5f);
}

static int get_text_width(const char *str) {
    if (!str) return 0;
    int w = 0;
    while (*str) {
        w += get_char_width(*str);
        str++;
    }
    return w;
}

static void draw_text(uint32_t *surf, int sw, int sh, int x0, int y0, const char *str, uint32_t col) {
    init_font();
    float x = (float)x0;
    float y = (float)y0 + 13.5f; /* Baseline naturale */

    uint32_t cr = col & 0xFF;
    uint32_t cg = (col >> 8) & 0xFF;
    uint32_t cb = (col >> 16) & 0xFF;

    while (*str) {
        char c = *str++;
        if ((unsigned char)c < 32 || (unsigned char)c >= 128) continue;
        const stbtt_bakedchar *b = &g_cdata[c - 32];

        int rx = (int)(x + b->xoff + 0.5f);
        int ry = (int)(y + b->yoff + 0.5f);
        int gw = b->x1 - b->x0;
        int gh = b->y1 - b->y0;

        for (int r = 0; r < gh; r++) {
            int py = ry + r;
            if (py < 0 || py >= sh) continue;
            for (int col_i = 0; col_i < gw; col_i++) {
                int px = rx + col_i;
                if (px < 0 || px >= sw) continue;

                unsigned char alpha = g_font_atlas[(b->y0 + r) * 512 + (b->x0 + col_i)];
                if (alpha == 0) continue;

                if (alpha >= 250) {
                    surf[py * sw + px] = col;
                } else {
                    uint32_t dst = surf[py * sw + px];
                    uint32_t dr = dst & 0xFF;
                    uint32_t dg = (dst >> 8) & 0xFF;
                    uint32_t db = (dst >> 16) & 0xFF;
                    uint32_t da = (dst >> 24) & 0xFF;

                    uint32_t a = alpha;
                    uint32_t inv_a = 255 - a;

                    uint32_t out_r = (cr * a + dr * inv_a) / 255;
                    uint32_t out_g = (cg * a + dg * inv_a) / 255;
                    uint32_t out_b = (cb * a + db * inv_a) / 255;
                    uint32_t out_a = (255 * a + da * inv_a) / 255;

                    surf[py * sw + px] = out_r | (out_g << 8) | (out_b << 16) | (out_a << 24);
                }
            }
        }
        x += b->xadvance;
    }
}

/* Simboli speciali grafici */
static void draw_bullet(uint32_t *surf, int sw, int sh, int cx, int cy, uint32_t col) {
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            if (dx*dx + dy*dy <= 4) {
                int px = cx + dx, py = cy + dy;
                if (px >= 0 && px < sw && py >= 0 && py < sh) surf[py * sw + px] = col;
            }
        }
    }
}

static void draw_check(uint32_t *surf, int sw, int sh, int x, int y, uint32_t col) {
    /* Segno di spunta ✓ */
    static const int coords[][2] = {
        {0, 4}, {1, 5}, {2, 6}, {3, 5}, {4, 4}, {5, 3}, {6, 2}, {7, 1}
    };
    for (size_t i = 0; i < sizeof(coords)/sizeof(coords[0]); ++i) {
        int px = x + coords[i][0];
        int py = y + coords[i][1];
        if (px >= 0 && px < sw && py >= 0 && py < sh) {
            surf[py * sw + px] = col;
            surf[(py+1) * sw + px] = col;
        }
    }
}

static void fill_rect(uint32_t *surf, int sw, int sh, int x0, int y0, int w, int h, uint32_t col) {
    for (int y = y0; y < y0 + h && y < sh; ++y) {
        if (y < 0) continue;
        for (int x = x0; x < x0 + w && x < sw; ++x) {
            if (x < 0) continue;
            surf[y * sw + x] = col;
        }
    }
}


/* Definizione Struttura Menu */
typedef enum {
    TYPE_ITEM = 0,
    TYPE_SEPARATOR,
    TYPE_SUBMENU
} item_type_t;

typedef struct {
    const char *label;
    const char *shortcut;
    menu_action_code_t action;
    item_type_t type;
    int submenu_id;      /* Identificativo del sottomenu se TYPE_SUBMENU */
    bool is_radio;
    bool is_checked;
    bool is_disabled;
} gui_menu_item_t;

typedef struct {
    const char *title;
    int x;
    int width;
    gui_menu_item_t items[64];
    int item_count;
} gui_top_menu_t;

typedef struct {
    int parent_top_idx;
    int parent_item_idx;
    gui_menu_item_t items[16];
    int item_count;
} gui_sub_menu_t;

enum {
    SUB_CONN_SAME_WIN = 1,
    SUB_CONN_NEW_WIN,
    SUB_VIEW_PLASMA_DECAY,
    SUB_VIEW_PLASMA_DIST,
    SUB_VIEW_CRT_BEAM,
    SUB_VIEW_CRT_DECAY,
    SUB_VIEW_CRT_DIST,
    SUB_VIEW_TEK_SPEED,
    SUB_VIEW_OVERRIDE_COLOR
};

void linux_menu_init(linux_menu_context_t *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->active_top_menu = -1;
    ctx->active_sub_menu = -1;
    ctx->hover_top_item = -1;
    ctx->hover_main_item = -1;
    ctx->hover_sub_item = -1;
    ctx->is_open = false;

    glGenTextures(1, &ctx->ui_texture_id);
    glBindTexture(GL_TEXTURE_2D, ctx->ui_texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    linux_menu_reload_data(ctx);
}

void linux_menu_reload_data(linux_menu_context_t *ctx) {
    plato_profiles_load(&ctx->profiles);
    if (!ctx->scripts) {
        ctx->scripts = plato_scripts_create();
    }
    plato_scripts_load(ctx->scripts, plato_scripts_get_default_path());
}

void linux_menu_destroy(linux_menu_context_t *ctx) {
    if (ctx->ui_surface) {
        free(ctx->ui_surface);
        ctx->ui_surface = NULL;
    }
    if (ctx->ui_texture_id) {
        glDeleteTextures(1, &ctx->ui_texture_id);
    }
    if (ctx->scripts) {
        plato_scripts_free(ctx->scripts);
        ctx->scripts = NULL;
    }
}

/* Costruzione Dinamica Menu Principali */
static int build_top_menus(linux_menu_context_t *ctx, const plato_profile_t *p, gui_top_menu_t *top) {
    int x_cur = 12;
    int count = 8;

    /* 1. Connection */
    top[0].title = "Connection";
    top[0].x = x_cur;
    top[0].width = get_text_width(top[0].title) + 18;
    x_cur += top[0].width + 4;
    {
        gui_menu_item_t *it = top[0].items;
        int n = 0;
        it[n++] = (gui_menu_item_t){"Same window default profile", NULL, ACT_CONN_DEFAULT, TYPE_ITEM, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"Same window with profile", NULL, ACT_NONE, TYPE_SUBMENU, SUB_CONN_SAME_WIN, false, false, false};
        it[n++] = (gui_menu_item_t){"New window default profile", "Ctrl+N", ACT_CONN_NEW_WIN_DEFAULT, TYPE_ITEM, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"New window with profile", NULL, ACT_NONE, TYPE_SUBMENU, SUB_CONN_NEW_WIN, false, false, false};
        it[n++] = (gui_menu_item_t){"-", NULL, ACT_NONE, TYPE_SEPARATOR, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"Disconnect this window", "Ctrl+D", ACT_CONN_DISCONNECT, TYPE_ITEM, 0, false, false, !ctx->connected};
        it[n++] = (gui_menu_item_t){"Close this window", "Ctrl+W", ACT_CONN_CLOSE_WINDOW, TYPE_ITEM, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"-", NULL, ACT_NONE, TYPE_SEPARATOR, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"Manage Profiles...", NULL, ACT_CONN_MANAGE_PROFILES, TYPE_ITEM, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"-", NULL, ACT_NONE, TYPE_SEPARATOR, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"Exit", "Ctrl+Q", ACT_CONN_EXIT, TYPE_ITEM, 0, false, false, false};
        top[0].item_count = n;
    }

    /* 2. Scripts */
    top[1].title = "Scripts";
    top[1].x = x_cur;
    top[1].width = get_text_width(top[1].title) + 18;
    x_cur += top[1].width + 4;
    {
        gui_menu_item_t *it = top[1].items;
        int n = 0;
        it[n++] = (gui_menu_item_t){"Manage Scripts...", NULL, ACT_SCRIPTS_MANAGE, TYPE_ITEM, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"Scripting Reference...", NULL, ACT_SCRIPTS_REFERENCE, TYPE_ITEM, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"Cancel Script Execution", "Ctrl+Shift+X", ACT_SCRIPTS_CANCEL, TYPE_ITEM, 0, false, false, !ctx->is_script_running};
        it[n++] = (gui_menu_item_t){"-", NULL, ACT_NONE, TYPE_SEPARATOR, 0, false, false, false};
        
        int added = 0;
        if (ctx->scripts && ctx->scripts->count > 0) {
            for (size_t s = 0; s < ctx->scripts->count && n < 19; ++s) {
                if (!ctx->scripts->scripts[s].enabled) continue;
                it[n++] = (gui_menu_item_t){ctx->scripts->scripts[s].name, ctx->scripts->scripts[s].hotkey_display, (menu_action_code_t)(ACT_SCRIPTS_EXEC_BASE + s), TYPE_ITEM, 0, false, false, false};
                added++;
            }
        }
        if (added == 0) {
            it[n++] = (gui_menu_item_t){"(No Active Scripts)", NULL, ACT_NONE, TYPE_ITEM, 0, false, false, true};
        }
        top[1].item_count = n;
    }

    /* 3. View */
    top[2].title = "View";
    top[2].x = x_cur;
    top[2].width = get_text_width(top[2].title) + 18;
    x_cur += top[2].width + 4;
    {
        bool isPlasma = (p->display_mode == 0 || p->display_mode == 2);
        bool isCRT = (p->display_mode == 4);

        gui_menu_item_t *it = top[2].items;
        int n = 0;
        it[n++] = (gui_menu_item_t){"Crisp Monochrome", NULL, ACT_VIEW_CRISP_MONO, TYPE_ITEM, 0, true, p->display_mode == 1, false};
        it[n++] = (gui_menu_item_t){"Split Monochrome", NULL, ACT_VIEW_SPLIT_MONO, TYPE_ITEM, 0, true, p->display_mode == 2, false};
        it[n++] = (gui_menu_item_t){"Real Plasma", NULL, ACT_VIEW_REAL_PLASMA, TYPE_ITEM, 0, true, p->display_mode == 0, false};
        it[n++] = (gui_menu_item_t){"Real Plasma Persistence", NULL, ACT_NONE, TYPE_SUBMENU, SUB_VIEW_PLASMA_DECAY, false, false, !isPlasma};
        it[n++] = (gui_menu_item_t){"Real Plasma Distortion", NULL, ACT_NONE, TYPE_SUBMENU, SUB_VIEW_PLASMA_DIST, false, false, !(p->display_mode == 0)};
        it[n++] = (gui_menu_item_t){"-", NULL, ACT_NONE, TYPE_SEPARATOR, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"Crisp Color", NULL, ACT_VIEW_CRISP_COLOR, TYPE_ITEM, 0, true, p->display_mode == 3, false};
        it[n++] = (gui_menu_item_t){"Real Color CRT", NULL, ACT_VIEW_REAL_CRT, TYPE_ITEM, 0, true, p->display_mode == 4, false};
        it[n++] = (gui_menu_item_t){"CRT Beam Profile", NULL, ACT_NONE, TYPE_SUBMENU, SUB_VIEW_CRT_BEAM, false, false, !isCRT};
        it[n++] = (gui_menu_item_t){"CRT Persistence", NULL, ACT_NONE, TYPE_SUBMENU, SUB_VIEW_CRT_DECAY, false, false, !isCRT};
        it[n++] = (gui_menu_item_t){"CRT Distortion", NULL, ACT_NONE, TYPE_SUBMENU, SUB_VIEW_CRT_DIST, false, false, !isCRT};
        it[n++] = (gui_menu_item_t){"-", NULL, ACT_NONE, TYPE_SEPARATOR, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"Real Tektronix 4014 (DVBST)", NULL, ACT_VIEW_REAL_TEKTRONIX, TYPE_ITEM, 0, true, p->display_mode == 5, false};
        it[n++] = (gui_menu_item_t){"Tektronix Connection Speed", NULL, ACT_NONE, TYPE_SUBMENU, SUB_VIEW_TEK_SPEED, false, false, (p->display_mode != 5)};
        it[n++] = (gui_menu_item_t){"Tektronix Erase Flash", NULL, ACT_VIEW_TEK_ERASE_FLASH, TYPE_ITEM, 0, true, p->tek_erase_flash, (p->display_mode != 5)};
        it[n++] = (gui_menu_item_t){"Tektronix Accurate Background", NULL, ACT_VIEW_TEK_ACCURATE_BG, TYPE_ITEM, 0, true, p->tek_accurate_bg, (p->display_mode != 5)};
        it[n++] = (gui_menu_item_t){"-", NULL, ACT_NONE, TYPE_SEPARATOR, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"Full Screen", "F11", ACT_VIEW_FULLSCREEN, TYPE_ITEM, 0, false, ctx->fullscreen, false};
        it[n++] = (gui_menu_item_t){"Show Performance HUD (FPS/CPU)", "F12", ACT_VIEW_FPS_HUD, TYPE_ITEM, 0, false, linux_hud_is_open(), false};
        it[n++] = (gui_menu_item_t){"Hide Menu Bar", "Ctrl+Shift+M", ACT_VIEW_HIDE_MENU_BAR, TYPE_ITEM, 0, true, ctx->hide_menu_bar, false};
        it[n++] = (gui_menu_item_t){"-", NULL, ACT_NONE, TYPE_SEPARATOR, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"Override Color", NULL, ACT_NONE, TYPE_SUBMENU, SUB_VIEW_OVERRIDE_COLOR, false, false, (p->display_mode == 5)};
        top[2].item_count = n;
    }

    /* 4. Keyboard (Posizione tra View ed Edit) - Dinamico */
    top[3].title = "Keyboard";
    top[3].x = x_cur;
    top[3].width = get_text_width(top[3].title) + 18;
    x_cur += top[3].width + 4;
    {
        gui_menu_item_t *it = top[3].items;
        int n = 0;
        if (ctx->keymap_mgr) {
            int activeIdx = plato_keymap_get_active_index(ctx->keymap_mgr);
            for (int i = 0; i < plato_keymap_get_count(ctx->keymap_mgr); i++) {
                const plato_keymap_profile_t *p = plato_keymap_get_profile(ctx->keymap_mgr, i);
                if (!p) continue;
                static char lbls[32][64];
                snprintf(lbls[i], sizeof(lbls[i]), "%s%s", p->name, p->is_builtin ? " (Built-in)" : "");
                it[n++] = (gui_menu_item_t){lbls[i], NULL, ACT_TOOLS_KEYMAP_PROFILE_BASE + i, TYPE_ITEM, 0, false, (i == activeIdx), false};
            }
        }
        it[n++] = (gui_menu_item_t){"-", NULL, ACT_NONE, TYPE_SEPARATOR, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"Cycle Profile (Next Preset)", "Ctrl+Shift+G", ACT_TOOLS_CYCLE_ARROW_MODE, TYPE_ITEM, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"-", NULL, ACT_NONE, TYPE_SEPARATOR, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"Manage Keyboard Profiles...", NULL, ACT_TOOLS_KEYMAP_MANAGE, TYPE_ITEM, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"-", NULL, ACT_NONE, TYPE_SEPARATOR, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"Keyboard Reference", "Ctrl+K", ACT_VIEW_KEYBOARD_REF, TYPE_ITEM, 0, false, ctx->show_keyref, false};
        top[3].item_count = n;
    }

    /* 5. Edit */
    top[4].title = "Edit";
    top[4].x = x_cur;
    top[4].width = get_text_width(top[4].title) + 18;
    x_cur += top[4].width + 4;
    {
        gui_menu_item_t *it = top[4].items;
        int n = 0;
        it[n++] = (gui_menu_item_t){"Copy All Text Verbatim", "Ctrl+C", ACT_EDIT_COPY_VERBATIM, TYPE_ITEM, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"Copy All Text Compact", "Ctrl+Shift+C", ACT_EDIT_COPY_COMPACT, TYPE_ITEM, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"Copy Selected Text...", "Ctrl+Shift+T", ACT_EDIT_COPY_SELECTED, TYPE_ITEM, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"Copy Screen Image", NULL, ACT_EDIT_COPY_IMAGE, TYPE_ITEM, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"-", NULL, ACT_NONE, TYPE_SEPARATOR, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"Paste Text", "Ctrl+V", ACT_EDIT_PASTE, TYPE_ITEM, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"Cancel Paste", "Ctrl+.", ACT_EDIT_CANCEL_PASTE, TYPE_ITEM, 0, false, false, false};
        top[4].item_count = n;
    }

    /* 6. Micro Tutor */
    bool isTek = (p->display_mode == 5);
    top[5].title = "Micro Tutor";
    top[5].x = x_cur;
    top[5].width = get_text_width(top[5].title) + 18;
    x_cur += top[5].width + 4;
    {
        gui_menu_item_t *it = top[5].items;
        int n = 0;
        it[n++] = (gui_menu_item_t){"Load and Boot from MTE...", NULL, ACT_MT_BOOT_MTE, TYPE_ITEM, 0, false, false, isTek};
        it[n++] = (gui_menu_item_t){"Enable Z80 Diagnostic Log", NULL, ACT_MT_TOGGLE_LOG, TYPE_ITEM, 0, false, ctx->z80_log, isTek};
        top[5].item_count = n;
    }

    /* 7. Tools */
    top[6].title = "Tools";
    top[6].x = x_cur;
    top[6].width = get_text_width(top[6].title) + 18;
    x_cur += top[6].width + 4;
    {
        gui_menu_item_t *it = top[6].items;
        int n = 0;
        it[n++] = (gui_menu_item_t){"Show Live Text Buffer", "Ctrl+Shift+T", ACT_TOOLS_LIVE_TEXT, TYPE_ITEM, 0, false, false, false};
        it[n++] = (gui_menu_item_t){"Enable Diagnostic Log", NULL, ACT_TOOLS_DIAG_LOG, TYPE_ITEM, 0, false, ctx->diag_log, false};
        it[n++] = (gui_menu_item_t){"Enable Renderer Performance Log", NULL, ACT_TOOLS_RENDERER_LOG, TYPE_ITEM, 0, false, ctx->renderer_log, false};
        top[6].item_count = n;
    }

    /* 8. Help */
    top[7].title = "Help";
    top[7].x = x_cur;
    top[7].width = get_text_width(top[7].title) + 18;
    x_cur += top[7].width + 4;
    {
        gui_menu_item_t *it = top[7].items;
        int n = 0;
        it[n++] = (gui_menu_item_t){"About PlatoLives...", NULL, ACT_HELP_ABOUT, TYPE_ITEM, 0, false, false, false};
        top[7].item_count = n;
    }

    return count;
}

/* Costruzione Dinamica Sottomenu */
static int build_submenu(linux_menu_context_t *ctx, const plato_profile_t *p, int sub_id, gui_menu_item_t *items) {
    int n = 0;
    switch (sub_id) {
        case SUB_CONN_SAME_WIN:
        case SUB_CONN_NEW_WIN: {
            bool is_new = (sub_id == SUB_CONN_NEW_WIN);
            if (ctx->profiles.count > 0) {
                for (size_t i = 0; i < ctx->profiles.count && n < 15; ++i) {
                    items[n++] = (gui_menu_item_t){
                        ctx->profiles.profiles[i].name, NULL,
                        (menu_action_code_t)((is_new ? ACT_CONN_NEW_WIN_PROFILE_BASE : ACT_CONN_PROFILE_BASE) + i),
                        TYPE_ITEM, 0, !is_new, !is_new && ((int)i == ctx->current_profile_idx), false
                    };
                }
            } else {
                items[n++] = (gui_menu_item_t){"(No Profiles)", NULL, ACT_NONE, TYPE_ITEM, 0, false, false, true};
            }
            break;
        }
        case SUB_VIEW_PLASMA_DECAY:
            items[n++] = (gui_menu_item_t){"100 ms (Authentic Plasma)", NULL, ACT_VIEW_PLASMA_DECAY_100, TYPE_ITEM, 0, true, p->persistence_ms == 100, false};
            items[n++] = (gui_menu_item_t){"200 ms (Warm Glow)", NULL, ACT_VIEW_PLASMA_DECAY_200, TYPE_ITEM, 0, true, p->persistence_ms == 200, false};
            items[n++] = (gui_menu_item_t){"500 ms (Medium)", NULL, ACT_VIEW_PLASMA_DECAY_500, TYPE_ITEM, 0, true, p->persistence_ms == 500, false};
            items[n++] = (gui_menu_item_t){"1000 ms (Long/Radar)", NULL, ACT_VIEW_PLASMA_DECAY_1000, TYPE_ITEM, 0, true, p->persistence_ms == 1000, false};
            items[n++] = (gui_menu_item_t){"2000 ms (Ultra Persistence)", NULL, ACT_VIEW_PLASMA_DECAY_2000, TYPE_ITEM, 0, true, p->persistence_ms == 2000, false};
            items[n++] = (gui_menu_item_t){"5000 ms (5s Extreme)", NULL, ACT_VIEW_PLASMA_DECAY_5000, TYPE_ITEM, 0, true, p->persistence_ms == 5000, false};
            break;
        case SUB_VIEW_PLASMA_DIST:
            items[n++] = (gui_menu_item_t){"None (Flat)", NULL, ACT_VIEW_PLASMA_DIST_NONE, TYPE_ITEM, 0, true, p->plasma_distortion == 0, false};
            items[n++] = (gui_menu_item_t){"Barrel (Curved Glass)", NULL, ACT_VIEW_PLASMA_DIST_BARREL, TYPE_ITEM, 0, true, p->plasma_distortion == 1, false};
            items[n++] = (gui_menu_item_t){"Cylindrical (Default)", NULL, ACT_VIEW_PLASMA_DIST_CYL, TYPE_ITEM, 0, true, p->plasma_distortion == 2, false};
            break;
        case SUB_VIEW_CRT_BEAM:
            items[n++] = (gui_menu_item_t){"Standard (Authentic 13\")", NULL, ACT_VIEW_CRT_BEAM_STD, TYPE_ITEM, 0, true, p->crt_beam_level == 0, false};
            items[n++] = (gui_menu_item_t){"High (Soft Glow)", NULL, ACT_VIEW_CRT_BEAM_HIGH, TYPE_ITEM, 0, true, p->crt_beam_level == 1, false};
            items[n++] = (gui_menu_item_t){"Ultra (Vintage Arcade)", NULL, ACT_VIEW_CRT_BEAM_ULTRA, TYPE_ITEM, 0, true, p->crt_beam_level == 2, false};
            break;
        case SUB_VIEW_CRT_DECAY:
            items[n++] = (gui_menu_item_t){"20 ms (Default CRT)", NULL, ACT_VIEW_CRT_DECAY_20, TYPE_ITEM, 0, true, p->crt_persistence_ms == 20, false};
            items[n++] = (gui_menu_item_t){"200 ms (Warm Glow)", NULL, ACT_VIEW_CRT_DECAY_200, TYPE_ITEM, 0, true, p->crt_persistence_ms == 200, false};
            items[n++] = (gui_menu_item_t){"500 ms (Medium)", NULL, ACT_VIEW_CRT_DECAY_500, TYPE_ITEM, 0, true, p->crt_persistence_ms == 500, false};
            items[n++] = (gui_menu_item_t){"1000 ms (Long/Radar)", NULL, ACT_VIEW_CRT_DECAY_1000, TYPE_ITEM, 0, true, p->crt_persistence_ms == 1000, false};
            items[n++] = (gui_menu_item_t){"2000 ms (Ultra Persistence)", NULL, ACT_VIEW_CRT_DECAY_2000, TYPE_ITEM, 0, true, p->crt_persistence_ms == 2000, false};
            items[n++] = (gui_menu_item_t){"5000 ms (5s Extreme)", NULL, ACT_VIEW_CRT_DECAY_5000, TYPE_ITEM, 0, true, p->crt_persistence_ms == 5000, false};
            break;
        case SUB_VIEW_CRT_DIST:
            items[n++] = (gui_menu_item_t){"None (Flat)", NULL, ACT_VIEW_CRT_DIST_NONE, TYPE_ITEM, 0, true, p->crt_distortion == 0, false};
            items[n++] = (gui_menu_item_t){"Barrel (Curved Glass)", NULL, ACT_VIEW_CRT_DIST_BARREL, TYPE_ITEM, 0, true, p->crt_distortion == 1, false};
            items[n++] = (gui_menu_item_t){"Cylindrical", NULL, ACT_VIEW_CRT_DIST_CYL, TYPE_ITEM, 0, true, p->crt_distortion == 2, false};
            break;
        case SUB_VIEW_TEK_SPEED: {
            int baud = p->tek_baud_rate;
            items[n++] = (gui_menu_item_t){"Instant (Uncapped)", NULL, ACT_VIEW_TEK_SPEED_INSTANT, TYPE_ITEM, 0, true, baud == 0, false};
            items[n++] = (gui_menu_item_t){"110 Baud", NULL, ACT_VIEW_TEK_SPEED_110, TYPE_ITEM, 0, true, baud == 110, false};
            items[n++] = (gui_menu_item_t){"150 Baud", NULL, ACT_VIEW_TEK_SPEED_150, TYPE_ITEM, 0, true, baud == 150, false};
            items[n++] = (gui_menu_item_t){"300 Baud", NULL, ACT_VIEW_TEK_SPEED_300, TYPE_ITEM, 0, true, baud == 300, false};
            items[n++] = (gui_menu_item_t){"600 Baud", NULL, ACT_VIEW_TEK_SPEED_600, TYPE_ITEM, 0, true, baud == 600, false};
            items[n++] = (gui_menu_item_t){"1200 Baud", NULL, ACT_VIEW_TEK_SPEED_1200, TYPE_ITEM, 0, true, baud == 1200, false};
            items[n++] = (gui_menu_item_t){"2400 Baud", NULL, ACT_VIEW_TEK_SPEED_2400, TYPE_ITEM, 0, true, baud == 2400, false};
            items[n++] = (gui_menu_item_t){"4800 Baud", NULL, ACT_VIEW_TEK_SPEED_4800, TYPE_ITEM, 0, true, baud == 4800, false};
            items[n++] = (gui_menu_item_t){"9600 Baud", NULL, ACT_VIEW_TEK_SPEED_9600, TYPE_ITEM, 0, true, baud == 9600, false};
            break;
        }
        case SUB_VIEW_OVERRIDE_COLOR:
            items[n++] = (gui_menu_item_t){"Default", NULL, ACT_VIEW_COLOR_DEFAULT, TYPE_ITEM, 0, true, p->phosphor_color == 0, false};
            items[n++] = (gui_menu_item_t){"Green (P31)", NULL, ACT_VIEW_COLOR_GREEN, TYPE_ITEM, 0, true, p->phosphor_color == 1, false};
            items[n++] = (gui_menu_item_t){"Amber (P40)", NULL, ACT_VIEW_COLOR_AMBER, TYPE_ITEM, 0, true, p->phosphor_color == 2, false};
            items[n++] = (gui_menu_item_t){"White (P4)", NULL, ACT_VIEW_COLOR_WHITE, TYPE_ITEM, 0, true, p->phosphor_color == 3, false};
            break;
        default:
            break;
    }
    return n;
}

#define MAIN_POPUP_WIDTH 280
#define SUB_POPUP_WIDTH  250
#define ITEM_HEIGHT      24

/* Calcolo Dinamico Larghezza Popup Principale */
static int get_main_popup_width(const gui_top_menu_t *top) {
    int max_w = MAIN_POPUP_WIDTH;
    if (!top) return max_w;
    for (int j = 0; j < top->item_count; ++j) {
        const gui_menu_item_t *it = &top->items[j];
        if (it->type == TYPE_SEPARATOR) continue;
        int text_w = get_text_width(it->label);
        int sc_w = 0;
        if (it->type == TYPE_SUBMENU) {
            sc_w = 20;
        } else if (it->shortcut) {
            sc_w = get_text_width(it->shortcut) + 24;
        }
        int req_w = 26 + text_w + 24 + sc_w + 16;
        if (req_w > max_w) max_w = req_w;
    }
    return max_w;
}

menu_action_code_t linux_menu_handle_mouse(linux_menu_context_t *ctx, const plato_profile_t *profile, int mouse_x, int mouse_y, bool clicked) {
    gui_top_menu_t top[8];
    plato_profile_t dummy;
    memset(&dummy, 0, sizeof(dummy));
    const plato_profile_t *cur_p = profile ? profile : &dummy;
    int top_count = build_top_menus(ctx, cur_p, top);

    /* Gestione Auto-Reveal per Hide Menu Bar con Timeout di Grazia di 2.0 secondi */
    if (ctx->hide_menu_bar) {
        double now_sec = (double)SDL_GetTicks() / 1000.0;
        if (mouse_y <= 14 || ctx->is_open) {
            ctx->menu_bar_revealed = true;
            ctx->menu_hide_deadline = now_sec + 2.0;
        } else if (mouse_y > (MENU_BAR_HEIGHT + DROPDOWN_GAP + 6)) {
            if (now_sec >= ctx->menu_hide_deadline && !ctx->is_open) {
                ctx->menu_bar_revealed = false;
            }
        }
        if (!ctx->menu_bar_revealed && !ctx->is_open) {
            ctx->hover_top_item = -1;
            return ACT_NONE;
        }
    }

    /* 1. Click sulla barra superiore */
    if (mouse_y <= MENU_BAR_HEIGHT) {
        for (int i = 0; i < top_count; ++i) {
            if (mouse_x >= top[i].x && mouse_x <= (top[i].x + top[i].width)) {
                ctx->hover_top_item = i;
                if (clicked) {
                    if (ctx->active_top_menu == i && ctx->is_open) {
                        ctx->is_open = false;
                        ctx->active_top_menu = -1;
                        ctx->active_sub_menu = -1;
                    } else {
                        ctx->active_top_menu = i;
                        ctx->active_sub_menu = -1;
                        ctx->is_open = true;
                    }
                } else if (ctx->is_open) {
                    ctx->active_top_menu = i;
                    ctx->active_sub_menu = -1;
                }
                return ACT_NONE;
            }
        }
        if (clicked) {
            ctx->is_open = false;
            ctx->active_top_menu = -1;
            ctx->active_sub_menu = -1;
        }
        return ACT_NONE;
    }

    /* 2. Controllo Popup Attivo */
    if (ctx->is_open && ctx->active_top_menu >= 0 && ctx->active_top_menu < top_count) {
        gui_top_menu_t *cur_top = &top[ctx->active_top_menu];
        int px = cur_top->x;
        int py = MENU_BAR_HEIGHT + DROPDOWN_GAP;
        int pw = get_main_popup_width(cur_top);
        int ph = cur_top->item_count * ITEM_HEIGHT;

        /* Controllo se siamo nel sottomenu secondario */
        if (ctx->active_sub_menu > 0 && ctx->hover_main_item >= 0) {
            int sub_x = px + pw - 4;
            int sub_y = py + ctx->hover_main_item * ITEM_HEIGHT;
            gui_menu_item_t sub_items[16];
            int sub_count = build_submenu(ctx, cur_p, ctx->active_sub_menu, sub_items);
            int sub_w = SUB_POPUP_WIDTH;
            int sub_h = sub_count * ITEM_HEIGHT;

            /* Auto-flip verso l'alto se sfora in basso */
            if (ctx->ui_height > 0 && (sub_y + sub_h) > (ctx->ui_height - 6)) {
                sub_y = ctx->ui_height - sub_h - 6;
            }
            if (sub_y < MENU_BAR_HEIGHT) sub_y = MENU_BAR_HEIGHT;

            if (mouse_x >= sub_x && mouse_x <= (sub_x + sub_w) &&
                mouse_y >= sub_y && mouse_y <= (sub_y + sub_h)) {
                
                int s_idx = (mouse_y - sub_y) / ITEM_HEIGHT;
                ctx->hover_sub_item = s_idx;

                if (clicked && s_idx >= 0 && s_idx < sub_count && !sub_items[s_idx].is_disabled) {
                    menu_action_code_t act = sub_items[s_idx].action;
                    ctx->is_open = false;
                    ctx->active_top_menu = -1;
                    ctx->active_sub_menu = -1;
                    return act;
                }
                return ACT_NONE;
            }
        }

        /* Controllo se siamo nel menu principale aperto */
        if (mouse_x >= px && mouse_x <= (px + pw) && mouse_y >= py && mouse_y <= (py + ph)) {
            int m_idx = (mouse_y - py) / ITEM_HEIGHT;
            ctx->hover_main_item = m_idx;
            ctx->hover_sub_item = -1;

            if (m_idx >= 0 && m_idx < cur_top->item_count) {
                gui_menu_item_t *it = &cur_top->items[m_idx];
                if (it->type == TYPE_SUBMENU) {
                    if (!it->is_disabled) {
                        ctx->active_sub_menu = it->submenu_id;
                    } else {
                        ctx->active_sub_menu = -1;
                    }
                } else {
                    ctx->active_sub_menu = -1;
                }

                if (clicked && it->type == TYPE_ITEM && !it->is_disabled) {
                    menu_action_code_t act = it->action;
                    ctx->is_open = false;
                    ctx->active_top_menu = -1;
                    ctx->active_sub_menu = -1;
                    return act;
                }
            }
            return ACT_NONE;
        } else if (clicked) {
            ctx->is_open = false;
            ctx->active_top_menu = -1;
            ctx->active_sub_menu = -1;
        }
    }

    return ACT_NONE;
}

void linux_menu_update_surface(linux_menu_context_t *ctx, const plato_profile_t *p, const struct plato_terminal *term, int win_w, int win_h) {
    int max_h = (win_h > 768) ? 768 : win_h;

    size_t req_size = (size_t)win_w * max_h;
    if (ctx->ui_width != win_w || ctx->ui_height != max_h || !ctx->ui_surface) {
        free(ctx->ui_surface);
        ctx->ui_surface = (uint32_t *)calloc(req_size, sizeof(uint32_t));
        ctx->ui_width = win_w;
        ctx->ui_height = max_h;
    } else {
        memset(ctx->ui_surface, 0, req_size * sizeof(uint32_t));
    }

    gui_top_menu_t top[8];
    int top_count = build_top_menus(ctx, p, top);

    /* 1. Disegno Sfondo Barra Menu Superiore */
    fill_rect(ctx->ui_surface, win_w, max_h, 0, 0, win_w, MENU_BAR_HEIGHT, COL_BAR_BG);
    fill_rect(ctx->ui_surface, win_w, max_h, 0, MENU_BAR_HEIGHT - 1, win_w, 1, COL_SEPARATOR);

    /* 2. Disegno Titoli Top Bar */
    for (int i = 0; i < top_count; ++i) {
        uint32_t tcol = COL_TEXT;
        if (i == 5 && p && p->display_mode == 5) {
            tcol = COL_TEXT_DIM; /* Micro Tutor dimmed su Tektronix 4014 */
        } else if (ctx->is_open && ctx->active_top_menu == i) {
            fill_rect(ctx->ui_surface, win_w, max_h, top[i].x, 3, top[i].width, MENU_BAR_HEIGHT - 6, COL_HOVER);
            tcol = COL_TEXT_HI;
        }
        draw_text(ctx->ui_surface, win_w, max_h, top[i].x + 9, 7, top[i].title, tcol);
    }

    /* 3. Disegno Menu Popup Principale */
    if (ctx->is_open && ctx->active_top_menu >= 0 && ctx->active_top_menu < top_count) {
        gui_top_menu_t *cur_top = &top[ctx->active_top_menu];
        int px = cur_top->x;
        int py = MENU_BAR_HEIGHT + DROPDOWN_GAP;
        int pw = get_main_popup_width(cur_top);
        int ph = cur_top->item_count * ITEM_HEIGHT;

        /* Sfondo con bordi */
        fill_rect(ctx->ui_surface, win_w, max_h, px, py, pw, ph, COL_BG);
        fill_rect(ctx->ui_surface, win_w, max_h, px, py, pw, 1, COL_HOVER_BORDER);
        fill_rect(ctx->ui_surface, win_w, max_h, px, py, 1, ph, COL_BORDER);
        fill_rect(ctx->ui_surface, win_w, max_h, px + pw - 1, py, 1, ph, COL_BORDER);
        fill_rect(ctx->ui_surface, win_w, max_h, px, py + ph - 1, pw, 1, COL_BORDER);

        for (int j = 0; j < cur_top->item_count; ++j) {
            gui_menu_item_t *it = &cur_top->items[j];
            int iy = py + j * ITEM_HEIGHT;

            if (it->type == TYPE_SEPARATOR) {
                fill_rect(ctx->ui_surface, win_w, max_h, px + 8, iy + ITEM_HEIGHT/2, pw - 16, 1, COL_SEPARATOR);
                continue;
            }

            bool is_hover = (ctx->hover_main_item == j);
            if (is_hover && !it->is_disabled) {
                fill_rect(ctx->ui_surface, win_w, max_h, px + 2, iy + 2, pw - 4, ITEM_HEIGHT - 4, COL_HOVER);
            }

            uint32_t text_col = it->is_disabled ? COL_TEXT_DIM : (is_hover ? COL_TEXT_HI : COL_TEXT);

            /* Indicatori Radio (•) o Checkmark (✓) */
            if (it->is_radio && it->is_checked) {
                draw_bullet(ctx->ui_surface, win_w, max_h, px + 14, iy + 12, text_col);
            } else if (!it->is_radio && it->is_checked) {
                draw_check(ctx->ui_surface, win_w, max_h, px + 10, iy + 7, text_col);
            }

            /* Etichetta principale */
            draw_text(ctx->ui_surface, win_w, max_h, px + 26, iy + 5, it->label, text_col);

            /* Scorciatoia o Freccia Sottomenu */
            if (it->type == TYPE_SUBMENU) {
                draw_text(ctx->ui_surface, win_w, max_h, px + pw - 16, iy + 5, ">", text_col);
            } else if (it->shortcut) {
                int sc_len = get_text_width(it->shortcut);
                draw_text(ctx->ui_surface, win_w, max_h, px + pw - sc_len - 12, iy + 5, it->shortcut, it->is_disabled ? COL_TEXT_DIM : COL_SHORTCUT);
            }
        }

        /* 4. Disegno Sottomenu Nidificato */
        if (ctx->active_sub_menu > 0 && ctx->hover_main_item >= 0) {
            int sub_x = px + pw - 4;
            int sub_y = py + ctx->hover_main_item * ITEM_HEIGHT;
            gui_menu_item_t sub_items[16];
            int sub_count = build_submenu(ctx, p, ctx->active_sub_menu, sub_items);
            int sub_w = SUB_POPUP_WIDTH;
            int sub_h = sub_count * ITEM_HEIGHT;

            /* Auto-flip verso l'alto se sfora in basso */
            if ((sub_y + sub_h) > (max_h - 6)) {
                sub_y = max_h - sub_h - 6;
            }
            if (sub_y < MENU_BAR_HEIGHT) sub_y = MENU_BAR_HEIGHT;

            fill_rect(ctx->ui_surface, win_w, max_h, sub_x, sub_y, sub_w, sub_h, COL_BG);
            fill_rect(ctx->ui_surface, win_w, max_h, sub_x, sub_y, sub_w, 1, COL_HOVER_BORDER);
            fill_rect(ctx->ui_surface, win_w, max_h, sub_x, sub_y, 1, sub_h, COL_BORDER);
            fill_rect(ctx->ui_surface, win_w, max_h, sub_x + sub_w - 1, sub_y, 1, sub_h, COL_BORDER);
            fill_rect(ctx->ui_surface, win_w, max_h, sub_x, sub_y + sub_h - 1, sub_w, 1, COL_BORDER);

            for (int k = 0; k < sub_count; ++k) {
                gui_menu_item_t *sit = &sub_items[k];
                int siy = sub_y + k * ITEM_HEIGHT;

                bool sub_hover = (ctx->hover_sub_item == k);
                if (sub_hover && !sit->is_disabled) {
                    fill_rect(ctx->ui_surface, win_w, max_h, sub_x + 2, siy + 2, sub_w - 4, ITEM_HEIGHT - 4, COL_HOVER);
                }

                uint32_t stext_col = sit->is_disabled ? COL_TEXT_DIM : (sub_hover ? COL_TEXT_HI : COL_TEXT);

                if (sit->is_radio && sit->is_checked) {
                    draw_bullet(ctx->ui_surface, win_w, max_h, sub_x + 14, siy + 12, stext_col);
                } else if (!sit->is_radio && sit->is_checked) {
                    draw_check(ctx->ui_surface, win_w, max_h, sub_x + 10, siy + 7, stext_col);
                }

                draw_text(ctx->ui_surface, win_w, max_h, sub_x + 26, siy + 5, sit->label, stext_col);
            }
        }
    }

    /* 5. Performance HUD disaccoppiato in finestra secondaria indipendente (F12) */

    /* Caricamento su Texture OpenGL UI */
    glBindTexture(GL_TEXTURE_2D, ctx->ui_texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, win_w, max_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, ctx->ui_surface);
}
