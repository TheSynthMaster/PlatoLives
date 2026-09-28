#include "win_menu.h"
#include <stdio.h>

HMENU win_menu_create(const plato_profile_list_t *profiles, const plato_script_list_t *scripts) {
    HMENU hMenuBar = CreateMenu();

    /* ============================================================= */
    /* 1. File (Sequenza identica a mac_window.m: 1032-1048)          */
    /* ============================================================= */
    HMENU hFile = CreatePopupMenu();
    AppendMenuW(hFile, MF_STRING, IDM_FILE_NEW_WINDOW, L"New Window\tCtrl+N");

    HMENU hNewWinProf = CreatePopupMenu();
    if (profiles && profiles->count > 0) {
        for (size_t i = 0; i < profiles->count; i++) {
            wchar_t wname[128];
            MultiByteToWideChar(CP_UTF8, 0, profiles->profiles[i].name, -1, wname, 128);
            AppendMenuW(hNewWinProf, MF_STRING, IDM_FILE_NEW_WIN_PROFILE_BASE + i, wname);
        }
    } else {
        AppendMenuW(hNewWinProf, MF_GRAYED, 0, L"(No Profiles)");
    }
    AppendMenuW(hFile, MF_POPUP, (UINT_PTR)hNewWinProf, L"New Window with Profile");
    AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hFile, MF_STRING, IDM_FILE_CLOSE_WINDOW, L"Close Window\tCtrl+W");
    AppendMenuW(hFile, MF_STRING, IDM_FILE_EXIT, L"Exit\tAlt+F4");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hFile, L"&File");

    /* ============================================================= */
    /* 2. Edit                                                        */
    /* ============================================================= */
    HMENU hEdit = CreatePopupMenu();
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_COPY_TEXT_VERBATIM, L"Copy All Text Verbatim\tCtrl+C");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_COPY_TEXT_COMPACT, L"Copy All Text Compact\tCtrl+Shift+C");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_COPY_SELECTED_TEXT, L"Copy Selected Text...\tCtrl+Shift+T");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_COPY_SCREEN_IMAGE, L"Copy Screen Image");
    AppendMenuW(hEdit, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_PASTE, L"Paste Text\tCtrl+V");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_CANCEL_PASTE, L"Cancel Paste\tCtrl+.");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hEdit, L"&Edit");

    /* ============================================================= */
    /* 3. Connection (Sequenza identica a mac_window.m: 1093-1273)    */
    /* ============================================================= */
    HMENU hConn = CreatePopupMenu();
    AppendMenuW(hConn, MF_STRING, IDM_CONN_CONNECT_DEFAULT, L"Connect to Default Profile");
    AppendMenuW(hConn, MF_STRING, IDM_CONN_MANAGE_PROFILES, L"Manage Profiles...");
    AppendMenuW(hConn, MF_SEPARATOR, 0, NULL);

    HMENU hSubProf = CreatePopupMenu();
    if (profiles && profiles->count > 0) {
        for (size_t i = 0; i < profiles->count; i++) {
            wchar_t wname[128];
            MultiByteToWideChar(CP_UTF8, 0, profiles->profiles[i].name, -1, wname, 128);
            AppendMenuW(hSubProf, MF_STRING, IDM_PROFILE_BASE + i, wname);
        }
    } else {
        AppendMenuW(hSubProf, MF_GRAYED, 0, L"(No Profiles)");
    }
    AppendMenuW(hConn, MF_POPUP, (UINT_PTR)hSubProf, L"Profiles");
    AppendMenuW(hConn, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hConn, MF_STRING, IDM_CONN_DISCONNECT, L"Disconnect");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hConn, L"&Connection");

    /* ============================================================= */
    /* 4. View (Sequenza esatta al millimetro: 1100-1234)             */
    /* ============================================================= */
    HMENU hView = CreatePopupMenu();
    AppendMenuW(hView, MF_STRING, IDM_VIEW_MODE_CRISP_MONO, L"Crisp Monochrome");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_MODE_SPLIT_MONO, L"Split Monochrome");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_MODE_REAL_PLASMA, L"Real Plasma");

    /* Posizione 3: Real Plasma Persistence */
    HMENU hPlasmaDecay = CreatePopupMenu();
    AppendMenuW(hPlasmaDecay, MF_STRING, IDM_VIEW_PLASMA_DECAY_100, L"100 ms (Authentic Plasma)");
    AppendMenuW(hPlasmaDecay, MF_STRING, IDM_VIEW_PLASMA_DECAY_200, L"200 ms (Warm Glow)");
    AppendMenuW(hPlasmaDecay, MF_STRING, IDM_VIEW_PLASMA_DECAY_500, L"500 ms (Medium)");
    AppendMenuW(hPlasmaDecay, MF_STRING, IDM_VIEW_PLASMA_DECAY_1000, L"1000 ms (Long/Radar)");
    AppendMenuW(hPlasmaDecay, MF_STRING, IDM_VIEW_PLASMA_DECAY_2000, L"2000 ms (Ultra Persistence)");
    AppendMenuW(hPlasmaDecay, MF_STRING, IDM_VIEW_PLASMA_DECAY_5000, L"5000 ms (5s Extreme)");
    AppendMenuW(hView, MF_POPUP, (UINT_PTR)hPlasmaDecay, L"Real Plasma Persistence");

    /* Posizione 4: Real Plasma Distortion */
    HMENU hPlasmaDist = CreatePopupMenu();
    AppendMenuW(hPlasmaDist, MF_STRING, IDM_VIEW_PLASMA_DIST_NONE, L"None (Flat)");
    AppendMenuW(hPlasmaDist, MF_STRING, IDM_VIEW_PLASMA_DIST_BARREL, L"Barrel (Curved Glass)");
    AppendMenuW(hPlasmaDist, MF_STRING, IDM_VIEW_PLASMA_DIST_CYL, L"Cylindrical (Default)");
    AppendMenuW(hView, MF_POPUP, (UINT_PTR)hPlasmaDist, L"Real Plasma Distortion");

    AppendMenuW(hView, MF_SEPARATOR, 0, NULL);

    AppendMenuW(hView, MF_STRING, IDM_VIEW_MODE_CRISP_COLOR, L"Crisp Color");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_MODE_REAL_CRT, L"Real Color CRT");

    /* Posizione 8: CRT Beam Profile */
    HMENU hCrtBeam = CreatePopupMenu();
    AppendMenuW(hCrtBeam, MF_STRING, IDM_VIEW_CRT_BEAM_STD, L"Standard (Authentic 13\")");
    AppendMenuW(hCrtBeam, MF_STRING, IDM_VIEW_CRT_BEAM_HIGH, L"High (Soft Glow)");
    AppendMenuW(hCrtBeam, MF_STRING, IDM_VIEW_CRT_BEAM_ULTRA, L"Ultra (Vintage Arcade)");
    AppendMenuW(hView, MF_POPUP, (UINT_PTR)hCrtBeam, L"CRT Beam Profile");

    /* Posizione 9: CRT Persistence */
    HMENU hCrtDecay = CreatePopupMenu();
    AppendMenuW(hCrtDecay, MF_STRING, IDM_VIEW_CRT_DECAY_20, L"20 ms (Default CRT)");
    AppendMenuW(hCrtDecay, MF_STRING, IDM_VIEW_CRT_DECAY_200, L"200 ms (Warm Glow)");
    AppendMenuW(hCrtDecay, MF_STRING, IDM_VIEW_CRT_DECAY_500, L"500 ms (Medium)");
    AppendMenuW(hCrtDecay, MF_STRING, IDM_VIEW_CRT_DECAY_1000, L"1000 ms (Long/Radar)");
    AppendMenuW(hCrtDecay, MF_STRING, IDM_VIEW_CRT_DECAY_2000, L"2000 ms (Ultra Persistence)");
    AppendMenuW(hCrtDecay, MF_STRING, IDM_VIEW_CRT_DECAY_5000, L"5000 ms (5s Extreme)");
    AppendMenuW(hView, MF_POPUP, (UINT_PTR)hCrtDecay, L"CRT Persistence");

    /* Posizione 10: CRT Distortion */
    HMENU hCrtDist = CreatePopupMenu();
    AppendMenuW(hCrtDist, MF_STRING, IDM_VIEW_CRT_DIST_NONE, L"None (Flat)");
    AppendMenuW(hCrtDist, MF_STRING, IDM_VIEW_CRT_DIST_BARREL, L"Barrel (Curved Glass)");
    AppendMenuW(hCrtDist, MF_STRING, IDM_VIEW_CRT_DIST_CYL, L"Cylindrical");
    AppendMenuW(hView, MF_POPUP, (UINT_PTR)hCrtDist, L"CRT Distortion");

    AppendMenuW(hView, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hView, MF_STRING, IDM_VIEW_KEYBOARD_REF, L"Keyboard Reference\tCtrl+K");
    AppendMenuW(hView, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hView, MF_STRING, IDM_VIEW_FULLSCREEN, L"Full Screen\tF11");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_FPS_HUD, L"Show Performance HUD (FPS/CPU)\tF12");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hView, L"&View");

    /* ============================================================= */
    /* 5. Tools (Identico 1:1 a mac_window.m: 1238-1246)             */
    /* ============================================================= */
    HMENU hTools = CreatePopupMenu();
    AppendMenuW(hTools, MF_STRING, IDM_EDIT_LIVE_TEXT_BUFFER, L"Show Live Text Buffer\tCtrl+Shift+T");
    AppendMenuW(hTools, MF_STRING, IDM_TOOLS_DIAG_LOG, L"Enable Network Diagnostic Log");
    AppendMenuW(hTools, MF_STRING, IDM_TOOLS_RENDERER_LOG, L"Enable Renderer Performance Log");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hTools, L"&Tools");

    /* ============================================================= */
    /* 6. Help                                                        */
    /* ============================================================= */
    HMENU hHelp = CreatePopupMenu();
    
    /* Scripts Menu */
    HMENU hScripts = CreatePopupMenu();
    AppendMenuW(hScripts, MF_STRING, IDM_SCRIPTS_MANAGE, L"Manage Scripts...");
    AppendMenuW(hScripts, MF_STRING, IDM_SCRIPTS_REFERENCE, L"Scripting Reference...");
    AppendMenuW(hScripts, MF_STRING, IDM_SCRIPTS_CANCEL, L"Cancel Script Execution\tCtrl+Shift+X");
    AppendMenuW(hScripts, MF_SEPARATOR, 0, NULL);
    size_t active_scripts = 0;
    if (scripts && scripts->count > 0) {
        for (size_t i = 0; i < scripts->count && i < (IDM_SCRIPTS_MAX - IDM_SCRIPTS_BASE); i++) {
            if (!scripts->scripts[i].enabled) continue;
            active_scripts++;
            WCHAR witem[128];
            if (scripts->scripts[i].hotkey_display[0]) {
                swprintf(witem, sizeof(witem)/sizeof(WCHAR), L"%hs\t%hs",
                         scripts->scripts[i].name, scripts->scripts[i].hotkey_display);
            } else {
                swprintf(witem, sizeof(witem)/sizeof(WCHAR), L"%hs", scripts->scripts[i].name);
            }
            AppendMenuW(hScripts, MF_STRING, IDM_SCRIPTS_BASE + i, witem);
        }
    }
    if (active_scripts == 0) {
        AppendMenuW(hScripts, MF_GRAYED, 0, L"(No Active Scripts)");
    }
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hScripts, L"&Scripts");

    AppendMenuW(hHelp, MF_STRING, IDM_HELP_ABOUT, L"About PlatoLives...");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hHelp, L"&Help");

    return hMenuBar;
}

void win_menu_update_state(HMENU menu, const plato_profile_t *p, bool connected, bool diag_log, bool renderer_log, bool fullscreen, const plato_profile_list_t *profiles, int current_profile_idx) {
    if (!menu || !p) return;

    EnableMenuItem(menu, IDM_CONN_CONNECT_DEFAULT, connected ? (MF_BYCOMMAND | MF_GRAYED) : (MF_BYCOMMAND | MF_ENABLED));
    EnableMenuItem(menu, IDM_CONN_DISCONNECT, connected ? (MF_BYCOMMAND | MF_ENABLED) : (MF_BYCOMMAND | MF_GRAYED));

    /* Modalità Display: 0: Real Plasma, 1: Crisp Mono, 2: Split Mono, 3: Crisp Color, 4: Real CRT */
    UINT mode_id = IDM_VIEW_MODE_REAL_PLASMA;
    if (p->display_mode == 1) mode_id = IDM_VIEW_MODE_CRISP_MONO;
    else if (p->display_mode == 2) mode_id = IDM_VIEW_MODE_SPLIT_MONO;
    else if (p->display_mode == 3) mode_id = IDM_VIEW_MODE_CRISP_COLOR;
    else if (p->display_mode == 4) mode_id = IDM_VIEW_MODE_REAL_CRT;

    CheckMenuRadioItem(menu, IDM_VIEW_MODE_REAL_PLASMA, IDM_VIEW_MODE_REAL_CRT, mode_id, MF_BYCOMMAND);

    /* ============================================================= */
    /* LOGICA DINAMICA SOTTOMENU OTTICI (Identica a mac_window.m)    */
    /* ============================================================= */
    HMENU hView = GetSubMenu(menu, 3);
    if (hView) {
        bool isPlasmaAny  = (p->display_mode == 0 || p->display_mode == 2);
        bool isPlasmaFull = (p->display_mode == 0);
        bool isCRT        = (p->display_mode == 4);

        /* Posizione 3: Real Plasma Persistence */
        EnableMenuItem(hView, 3, MF_BYPOSITION | (isPlasmaAny ? MF_ENABLED : MF_GRAYED));
        /* Posizione 4: Real Plasma Distortion */
        EnableMenuItem(hView, 4, MF_BYPOSITION | (isPlasmaFull ? MF_ENABLED : MF_GRAYED));
        /* Posizione 8: CRT Beam Profile */
        EnableMenuItem(hView, 8, MF_BYPOSITION | (isCRT ? MF_ENABLED : MF_GRAYED));
        /* Posizione 9: CRT Persistence */
        EnableMenuItem(hView, 9, MF_BYPOSITION | (isCRT ? MF_ENABLED : MF_GRAYED));
        /* Posizione 10: CRT Distortion */
        EnableMenuItem(hView, 10, MF_BYPOSITION | (isCRT ? MF_ENABLED : MF_GRAYED));
    }

    /* Plasma Decay */
    UINT pd_id = IDM_VIEW_PLASMA_DECAY_100;
    if (p->persistence_ms == 200) pd_id = IDM_VIEW_PLASMA_DECAY_200;
    else if (p->persistence_ms == 500) pd_id = IDM_VIEW_PLASMA_DECAY_500;
    else if (p->persistence_ms == 1000) pd_id = IDM_VIEW_PLASMA_DECAY_1000;
    else if (p->persistence_ms == 2000) pd_id = IDM_VIEW_PLASMA_DECAY_2000;
    else if (p->persistence_ms == 5000) pd_id = IDM_VIEW_PLASMA_DECAY_5000;
    CheckMenuRadioItem(menu, IDM_VIEW_PLASMA_DECAY_100, IDM_VIEW_PLASMA_DECAY_5000, pd_id, MF_BYCOMMAND);

    /* Plasma Distortion */
    CheckMenuRadioItem(menu, IDM_VIEW_PLASMA_DIST_NONE, IDM_VIEW_PLASMA_DIST_CYL,
                       IDM_VIEW_PLASMA_DIST_NONE + p->plasma_distortion, MF_BYCOMMAND);

    /* CRT Beam */
    CheckMenuRadioItem(menu, IDM_VIEW_CRT_BEAM_STD, IDM_VIEW_CRT_BEAM_ULTRA,
                       IDM_VIEW_CRT_BEAM_STD + p->crt_beam_level, MF_BYCOMMAND);

    /* CRT Decay */
    UINT cd_id = IDM_VIEW_CRT_DECAY_20;
    if (p->crt_persistence_ms == 200) cd_id = IDM_VIEW_CRT_DECAY_200;
    else if (p->crt_persistence_ms == 500) cd_id = IDM_VIEW_CRT_DECAY_500;
    else if (p->crt_persistence_ms == 1000) cd_id = IDM_VIEW_CRT_DECAY_1000;
    else if (p->crt_persistence_ms == 2000) cd_id = IDM_VIEW_CRT_DECAY_2000;
    else if (p->crt_persistence_ms == 5000) cd_id = IDM_VIEW_CRT_DECAY_5000;
    CheckMenuRadioItem(menu, IDM_VIEW_CRT_DECAY_20, IDM_VIEW_CRT_DECAY_5000, cd_id, MF_BYCOMMAND);

    /* CRT Distortion */
    CheckMenuRadioItem(menu, IDM_VIEW_CRT_DIST_NONE, IDM_VIEW_CRT_DIST_CYL,
                       IDM_VIEW_CRT_DIST_NONE + p->crt_distortion, MF_BYCOMMAND);

    /* Profiles Submenu */
    if (profiles && profiles->count > 0 && current_profile_idx >= 0) {
        CheckMenuRadioItem(menu, IDM_PROFILE_BASE, IDM_PROFILE_BASE + (UINT)profiles->count - 1,
                           IDM_PROFILE_BASE + (UINT)current_profile_idx, MF_BYCOMMAND);
    }

    CheckMenuItem(menu, IDM_TOOLS_DIAG_LOG, diag_log ? (MF_BYCOMMAND | MF_CHECKED) : (MF_BYCOMMAND | MF_UNCHECKED));
    CheckMenuItem(menu, IDM_TOOLS_RENDERER_LOG, renderer_log ? (MF_BYCOMMAND | MF_CHECKED) : (MF_BYCOMMAND | MF_UNCHECKED));
    CheckMenuItem(menu, IDM_VIEW_FULLSCREEN, fullscreen ? (MF_BYCOMMAND | MF_CHECKED) : (MF_BYCOMMAND | MF_UNCHECKED));
}
