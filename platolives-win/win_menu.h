#ifndef WIN_MENU_H
#define WIN_MENU_H

#include <windows.h>
#include <stdbool.h>
#include "plato/plato_profile.h"

/* Menu File */
#define IDM_FILE_NEW_WINDOW             1000
#define IDM_FILE_NEW_WIN_PROFILE_BASE   2100
#define IDM_FILE_NEW_WIN_PROFILE_MAX    (IDM_FILE_NEW_WIN_PROFILE_BASE + PLATO_MAX_PROFILES)
#define IDM_FILE_CLOSE_WINDOW           1002
#define IDM_FILE_EXIT                   1003

/* Menu Edit */
#define IDM_EDIT_UNDO                   1101
#define IDM_EDIT_REDO                   1102
#define IDM_EDIT_CUT                    1103
#define IDM_EDIT_COPY_TEXT_VERBATIM     1104
#define IDM_EDIT_COPY_TEXT_COMPACT      1110
#define IDM_EDIT_COPY_SELECTED_TEXT     1108
#define IDM_EDIT_COPY_SCREEN_IMAGE      1107
#define IDM_EDIT_PASTE                  1105
#define IDM_EDIT_CANCEL_PASTE           1109
#define IDM_EDIT_LIVE_TEXT_BUFFER       1108
#define IDM_EDIT_COPY_SCREEN_TEXT       1107

/* Menu Connection */
#define IDM_CONN_CONNECT_DEFAULT        1201
#define IDM_CONN_MANAGE_PROFILES        1202
#define IDM_PROFILE_BASE                2000
#define IDM_PROFILE_MAX                 (IDM_PROFILE_BASE + PLATO_MAX_PROFILES)
#define IDM_CONN_DISCONNECT             1204

/* Menu View - Sequenza identica a mac_window.m */
#define IDM_VIEW_MODE_CRISP_MONO        1301
#define IDM_VIEW_MODE_SPLIT_MONO        1302
#define IDM_VIEW_MODE_REAL_PLASMA       1300
#define IDM_VIEW_MODE_CRISP_COLOR       1303
#define IDM_VIEW_MODE_REAL_CRT          1304

#define IDM_VIEW_PLASMA_DECAY_100       1320
#define IDM_VIEW_PLASMA_DECAY_200       1321
#define IDM_VIEW_PLASMA_DECAY_500       1322
#define IDM_VIEW_PLASMA_DECAY_1000      1323
#define IDM_VIEW_PLASMA_DECAY_2000      1324
#define IDM_VIEW_PLASMA_DECAY_5000      1325

#define IDM_VIEW_PLASMA_DIST_NONE       1330
#define IDM_VIEW_PLASMA_DIST_BARREL     1331
#define IDM_VIEW_PLASMA_DIST_CYL        1332

#define IDM_VIEW_CRT_BEAM_STD           1340
#define IDM_VIEW_CRT_BEAM_HIGH          1341
#define IDM_VIEW_CRT_BEAM_ULTRA         1342

#define IDM_VIEW_CRT_DECAY_20           1350
#define IDM_VIEW_CRT_DECAY_200          1351
#define IDM_VIEW_CRT_DECAY_500          1352
#define IDM_VIEW_CRT_DECAY_1000         1353
#define IDM_VIEW_CRT_DECAY_2000         1354
#define IDM_VIEW_CRT_DECAY_5000         1355

#define IDM_VIEW_CRT_DIST_NONE          1360
#define IDM_VIEW_CRT_DIST_BARREL        1361
#define IDM_VIEW_CRT_DIST_CYL           1362

#define IDM_VIEW_KEYBOARD_REF           1380
#define IDM_VIEW_FULLSCREEN             1391
#define IDM_VIEW_FPS_HUD                1392

/* Menu Tools */
#define IDM_TOOLS_SAVE_SCREEN_TEXT      1401
#define IDM_TOOLS_DIAG_LOG              1402
#define IDM_TOOLS_RENDERER_LOG          1403

/* Menu Help */
#define IDM_HELP_ABOUT                  1501

HMENU win_menu_create(const plato_profile_list_t *profiles);
void win_menu_update_state(HMENU menu, const plato_profile_t *active_p, bool connected, bool diag_log, bool renderer_log, bool fullscreen, const plato_profile_list_t *profiles, int current_profile_idx);

#endif /* WIN_MENU_H */
