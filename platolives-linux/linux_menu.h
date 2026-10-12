#ifndef LINUX_MENU_H
#define LINUX_MENU_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "plato/plato_profile.h"
#include "plato/plato_script.h"

#define MENU_BAR_HEIGHT 28

typedef enum {
    ACT_NONE = 0,
    ACT_CONN_DEFAULT,
    ACT_CONN_PROFILE_BASE = 100, /* 100 .. 131 */
    ACT_CONN_NEW_WIN_DEFAULT = 200,
    ACT_CONN_NEW_WIN_PROFILE_BASE = 201, /* 201 .. 232 */
    ACT_CONN_DISCONNECT = 300,
    ACT_CONN_CLOSE_WINDOW,
    ACT_CONN_MANAGE_PROFILES,
    ACT_CONN_EXIT,

    ACT_SCRIPTS_MANAGE,
    ACT_SCRIPTS_REFERENCE,
    ACT_SCRIPTS_CANCEL,
    ACT_SCRIPTS_EXEC_BASE = 400, /* 400 .. 499 */

    ACT_VIEW_CRISP_MONO = 500,
    ACT_VIEW_SPLIT_MONO,
    ACT_VIEW_REAL_PLASMA,
    ACT_VIEW_PLASMA_DECAY_100,
    ACT_VIEW_PLASMA_DECAY_200,
    ACT_VIEW_PLASMA_DECAY_500,
    ACT_VIEW_PLASMA_DECAY_1000,
    ACT_VIEW_PLASMA_DECAY_2000,
    ACT_VIEW_PLASMA_DECAY_5000,
    ACT_VIEW_PLASMA_DIST_NONE,
    ACT_VIEW_PLASMA_DIST_BARREL,
    ACT_VIEW_PLASMA_DIST_CYL,
    ACT_VIEW_CRISP_COLOR,
    ACT_VIEW_REAL_CRT,
    ACT_VIEW_CRT_BEAM_STD,
    ACT_VIEW_CRT_BEAM_HIGH,
    ACT_VIEW_CRT_BEAM_ULTRA,
    ACT_VIEW_CRT_DECAY_20,
    ACT_VIEW_CRT_DECAY_200,
    ACT_VIEW_CRT_DECAY_500,
    ACT_VIEW_CRT_DECAY_1000,
    ACT_VIEW_CRT_DECAY_2000,
    ACT_VIEW_CRT_DECAY_5000,
    ACT_VIEW_CRT_DIST_NONE,
    ACT_VIEW_CRT_DIST_BARREL,
    ACT_VIEW_CRT_DIST_CYL,
    ACT_VIEW_KEYBOARD_REF,
    ACT_VIEW_FULLSCREEN,
    ACT_VIEW_FPS_HUD,
    ACT_VIEW_HIDE_MENU_BAR,

    ACT_EDIT_COPY_VERBATIM = 600,
    ACT_EDIT_COPY_COMPACT,
    ACT_EDIT_COPY_SELECTED,
    ACT_EDIT_COPY_IMAGE,
    ACT_EDIT_PASTE,
    ACT_EDIT_CANCEL_PASTE,

    ACT_MT_BOOT_MTE = 700,
    ACT_MT_TOGGLE_LOG,

    ACT_TOOLS_LIVE_TEXT = 800,
    ACT_TOOLS_DIAG_LOG,
    ACT_TOOLS_RENDERER_LOG,

    ACT_HELP_ABOUT = 900
} menu_action_code_t;

typedef struct {
    int active_top_menu;     /* -1: chiuso, 0..6: indice menu aperto */
    int active_sub_menu;     /* -1: nessun sottomenu secondario, >=0: indice sottomenu */
    int hover_top_item;
    int hover_main_item;
    int hover_sub_item;
    bool is_open;

    uint32_t *ui_surface;
    int ui_width;
    int ui_height;
    unsigned int ui_texture_id;

    /* Dati dinamici */
    plato_profile_list_t profiles;
    plato_script_list_t *scripts;
    bool is_script_running;
    int current_profile_idx;
    bool connected;
    bool diag_log;
    bool renderer_log;
    bool z80_log;
    bool fps_hud;
    bool fullscreen;
    bool show_keyref;
    bool hide_menu_bar;
    bool menu_bar_revealed;
    float menu_anim_y;         /* 0.0f = nascosto, 1.0f = visibile */
    double menu_hide_deadline; /* timestamp scadenza timeout 2 secondi */

    /* Telemetria reale frame e CPU */
    double hud_fps_total;
    double hud_fps_partial;
    double hud_fps_full;
    double hud_cpu_percent;
} linux_menu_context_t;

void linux_menu_init(linux_menu_context_t *ctx);
void linux_menu_destroy(linux_menu_context_t *ctx);
void linux_menu_reload_data(linux_menu_context_t *ctx);
menu_action_code_t linux_menu_handle_mouse(linux_menu_context_t *ctx, const plato_profile_t *profile, int mouse_x, int mouse_y, bool clicked);
void linux_menu_update_surface(linux_menu_context_t *ctx, const plato_profile_t *profile, const struct plato_terminal *term, int win_w, int win_h);

#endif /* LINUX_MENU_H */
