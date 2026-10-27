#include "win_menu.h"
#include <stdio.h>
#include <wchar.h>

HMENU win_menu_create(const plato_profile_list_t *profiles, const plato_script_list_t *scripts) {
    HMENU hMenuBar = CreateMenu();

    /* ============================================================= */
    /* 1. Connection                                                 */
    /* ============================================================= */
    HMENU hConn = CreatePopupMenu();
    AppendMenuW(hConn, MF_STRING, IDM_CONN_CONNECT_DEFAULT, L"Same window default profile");

    HMENU hSubProf = CreatePopupMenu();
    if (profiles && profiles->count > 0) {
        for (size_t i = 0; i < profiles->count && i < (IDM_PROFILE_MAX - IDM_PROFILE_BASE); i++) {
            WCHAR wname[128];
            swprintf(wname, sizeof(wname)/sizeof(WCHAR), L"%hs%s",
                     profiles->profiles[i].name,
                     profiles->profiles[i].is_default ? L" *" : L"");
            AppendMenuW(hSubProf, MF_STRING, IDM_PROFILE_BASE + i, wname);
        }
    } else {
        AppendMenuW(hSubProf, MF_GRAYED, 0, L"(No Profiles)");
    }
    AppendMenuW(hConn, MF_POPUP, (UINT_PTR)hSubProf, L"Same window with profile");

    AppendMenuW(hConn, MF_STRING, IDM_FILE_NEW_WINDOW, L"New window default profile\tCtrl+N");

    HMENU hNewWinProf = CreatePopupMenu();
    if (profiles && profiles->count > 0) {
        for (size_t i = 0; i < profiles->count && i < (IDM_FILE_NEW_WIN_PROFILE_MAX - IDM_FILE_NEW_WIN_PROFILE_BASE); i++) {
            WCHAR wname[128];
            swprintf(wname, sizeof(wname)/sizeof(WCHAR), L"%hs", profiles->profiles[i].name);
            AppendMenuW(hNewWinProf, MF_STRING, IDM_FILE_NEW_WIN_PROFILE_BASE + i, wname);
        }
    } else {
        AppendMenuW(hNewWinProf, MF_GRAYED, 0, L"(No Profiles)");
    }
    AppendMenuW(hConn, MF_POPUP, (UINT_PTR)hNewWinProf, L"New window with profile");

    AppendMenuW(hConn, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hConn, MF_STRING, IDM_CONN_DISCONNECT, L"Disconnect this window\tCtrl+D");
    AppendMenuW(hConn, MF_STRING, IDM_FILE_CLOSE_WINDOW, L"Close this window\tCtrl+W");
    AppendMenuW(hConn, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hConn, MF_STRING, IDM_CONN_MANAGE_PROFILES, L"Manage Profiles...");
    AppendMenuW(hConn, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hConn, MF_STRING, IDM_FILE_EXIT, L"Exit\tAlt+F4");

    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hConn, L"&Connection");

    /* ============================================================= */
    /* 2. Scripts                                                    */
    /* ============================================================= */
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

    /* ============================================================= */
    /* 3. View                                                       */
    /* ============================================================= */
    HMENU hView = CreatePopupMenu();
    AppendMenuW(hView, MF_STRING, IDM_VIEW_MODE_CRISP_MONO, L"Crisp Monochrome");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_MODE_SPLIT_MONO, L"Split Monochrome");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_MODE_REAL_PLASMA, L"Real Plasma");

    HMENU hPlasmaDecay = CreatePopupMenu();
    AppendMenuW(hPlasmaDecay, MF_STRING, IDM_VIEW_PLASMA_DECAY_100, L"100 ms (Authentic Plasma)");
    AppendMenuW(hPlasmaDecay, MF_STRING, IDM_VIEW_PLASMA_DECAY_200, L"200 ms (Warm Glow)");
    AppendMenuW(hPlasmaDecay, MF_STRING, IDM_VIEW_PLASMA_DECAY_500, L"500 ms (Medium)");
    AppendMenuW(hPlasmaDecay, MF_STRING, IDM_VIEW_PLASMA_DECAY_1000, L"1000 ms (Long/Radar)");
    AppendMenuW(hPlasmaDecay, MF_STRING, IDM_VIEW_PLASMA_DECAY_2000, L"2000 ms (Ultra Persistence)");
    AppendMenuW(hPlasmaDecay, MF_STRING, IDM_VIEW_PLASMA_DECAY_5000, L"5000 ms (5s Extreme)");
    AppendMenuW(hView, MF_POPUP, (UINT_PTR)hPlasmaDecay, L"Real Plasma Persistence");

    HMENU hPlasmaDist = CreatePopupMenu();
    AppendMenuW(hPlasmaDist, MF_STRING, IDM_VIEW_PLASMA_DIST_NONE, L"None (Flat)");
    AppendMenuW(hPlasmaDist, MF_STRING, IDM_VIEW_PLASMA_DIST_BARREL, L"Barrel (Curved Glass)");
    AppendMenuW(hPlasmaDist, MF_STRING, IDM_VIEW_PLASMA_DIST_CYL, L"Cylindrical (Default)");
    AppendMenuW(hView, MF_POPUP, (UINT_PTR)hPlasmaDist, L"Real Plasma Distortion");

    AppendMenuW(hView, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hView, MF_STRING, IDM_VIEW_MODE_CRISP_COLOR, L"Crisp Color");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_MODE_REAL_CRT, L"Real Color CRT");

    HMENU hCrtBeam = CreatePopupMenu();
    AppendMenuW(hCrtBeam, MF_STRING, IDM_VIEW_CRT_BEAM_STD, L"Standard (Authentic 13\")");
    AppendMenuW(hCrtBeam, MF_STRING, IDM_VIEW_CRT_BEAM_HIGH, L"High (Soft Glow)");
    AppendMenuW(hCrtBeam, MF_STRING, IDM_VIEW_CRT_BEAM_ULTRA, L"Ultra (Vintage Arcade)");
    AppendMenuW(hView, MF_POPUP, (UINT_PTR)hCrtBeam, L"CRT Beam Profile");

    HMENU hCrtDecay = CreatePopupMenu();
    AppendMenuW(hCrtDecay, MF_STRING, IDM_VIEW_CRT_DECAY_20, L"20 ms (Default CRT)");
    AppendMenuW(hCrtDecay, MF_STRING, IDM_VIEW_CRT_DECAY_200, L"200 ms (Warm Glow)");
    AppendMenuW(hCrtDecay, MF_STRING, IDM_VIEW_CRT_DECAY_500, L"500 ms (Medium)");
    AppendMenuW(hCrtDecay, MF_STRING, IDM_VIEW_CRT_DECAY_1000, L"1000 ms (Long/Radar)");
    AppendMenuW(hCrtDecay, MF_STRING, IDM_VIEW_CRT_DECAY_2000, L"2000 ms (Ultra Persistence)");
    AppendMenuW(hCrtDecay, MF_STRING, IDM_VIEW_CRT_DECAY_5000, L"5000 ms (5s Extreme)");
    AppendMenuW(hView, MF_POPUP, (UINT_PTR)hCrtDecay, L"CRT Persistence");

    HMENU hCrtDist = CreatePopupMenu();
    AppendMenuW(hCrtDist, MF_STRING, IDM_VIEW_CRT_DIST_NONE, L"None (Flat)");
    AppendMenuW(hCrtDist, MF_STRING, IDM_VIEW_CRT_DIST_BARREL, L"Barrel (Curved Glass)");
    AppendMenuW(hCrtDist, MF_STRING, IDM_VIEW_CRT_DIST_CYL, L"Cylindrical");
    AppendMenuW(hView, MF_POPUP, (UINT_PTR)hCrtDist, L"CRT Distortion");

    AppendMenuW(hView, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hView, MF_STRING, IDM_VIEW_MODE_REAL_TEKTRONIX, L"Real Tektronix 4014 (DVBST)");

    HMENU hTekSpeed = CreatePopupMenu();
    AppendMenuW(hTekSpeed, MF_STRING, IDM_VIEW_TEK_SPEED_INSTANT, L"Instant (Uncapped)");
    AppendMenuW(hTekSpeed, MF_STRING, IDM_VIEW_TEK_SPEED_110,     L"110 Baud");
    AppendMenuW(hTekSpeed, MF_STRING, IDM_VIEW_TEK_SPEED_150,     L"150 Baud");
    AppendMenuW(hTekSpeed, MF_STRING, IDM_VIEW_TEK_SPEED_300,     L"300 Baud");
    AppendMenuW(hTekSpeed, MF_STRING, IDM_VIEW_TEK_SPEED_600,     L"600 Baud");
    AppendMenuW(hTekSpeed, MF_STRING, IDM_VIEW_TEK_SPEED_1200,    L"1200 Baud");
    AppendMenuW(hTekSpeed, MF_STRING, IDM_VIEW_TEK_SPEED_2400,    L"2400 Baud");
    AppendMenuW(hTekSpeed, MF_STRING, IDM_VIEW_TEK_SPEED_4800,    L"4800 Baud");
    AppendMenuW(hTekSpeed, MF_STRING, IDM_VIEW_TEK_SPEED_9600,    L"9600 Baud");
    AppendMenuW(hView, MF_POPUP, (UINT_PTR)hTekSpeed, L"Tektronix Connection Speed");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_TEK_ERASE_FLASH, L"Tektronix Erase Flash");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_TEK_ACCURATE_BG, L"Tektronix Accurate Background");

    AppendMenuW(hView, MF_SEPARATOR, 0, NULL);

    AppendMenuW(hView, MF_STRING, IDM_VIEW_FULLSCREEN, L"Full Screen\tF11");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_FPS_HUD, L"Show Performance HUD (FPS/CPU)\tF12");

    AppendMenuW(hView, MF_SEPARATOR, 0, NULL);
    HMENU hOverrideColor = CreatePopupMenu();
    AppendMenuW(hOverrideColor, MF_STRING, IDM_VIEW_COLOR_DEFAULT, L"Default");
    AppendMenuW(hOverrideColor, MF_STRING, IDM_VIEW_COLOR_GREEN, L"Green (P31)");
    AppendMenuW(hOverrideColor, MF_STRING, IDM_VIEW_COLOR_AMBER, L"Amber (P40)");
    AppendMenuW(hOverrideColor, MF_STRING, IDM_VIEW_COLOR_WHITE, L"White (P4)");
    AppendMenuW(hView, MF_POPUP, (UINT_PTR)hOverrideColor, L"Override Color");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hView, L"&View");

    /* ============================================================= */
    /* 4. Keyboard (Posizione tra View ed Edit)                       */
    /* ============================================================= */
    HMENU hKeyboard = CreatePopupMenu();
    /* I profili dinamici verranno inseriti qui da win_update_arrow_menu_items() */
    AppendMenuW(hKeyboard, MF_STRING, IDM_TOOLS_CYCLE_ARROW_MODE, L"Cycle Profile (Next Preset)\tCtrl+Shift+G");
    AppendMenuW(hKeyboard, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hKeyboard, MF_STRING, IDM_TOOLS_KEYMAP_MANAGE, L"Manage Keyboard Profiles...");
    AppendMenuW(hKeyboard, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hKeyboard, MF_STRING, IDM_VIEW_KEYBOARD_REF, L"Keyboard Reference\tCtrl+K");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hKeyboard, L"&Keyboard");

    /* ============================================================= */
    /* 5. Edit                                                       */
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
    /* 5. Micro Tutor                                                */
    /* ============================================================= */
    HMENU hMicroTutor = CreatePopupMenu();
    AppendMenuW(hMicroTutor, MF_STRING, IDM_TOOLS_BOOT_MTE, L"Load and Boot from MTE...");
    AppendMenuW(hMicroTutor, MF_STRING, IDM_TOOLS_MT_LOG, L"Enable Z80 Diagnostic Log");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMicroTutor, L"&Micro Tutor");

    /* ============================================================= */
    /* 6. Tools (Intatto)                                            */
    /* ============================================================= */
    HMENU hTools = CreatePopupMenu();
    AppendMenuW(hTools, MF_STRING, IDM_EDIT_LIVE_TEXT_BUFFER, L"Show Live Text Buffer\tCtrl+Shift+T");
    AppendMenuW(hTools, MF_STRING, IDM_TOOLS_DIAG_LOG, L"Enable Network Diagnostic Log");
    AppendMenuW(hTools, MF_STRING, IDM_TOOLS_RENDERER_LOG, L"Enable Renderer Performance Log");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hTools, L"&Tools");

    /* ============================================================= */
    /* 7. Help                                                       */
    /* ============================================================= */
    HMENU hHelp = CreatePopupMenu();
    AppendMenuW(hHelp, MF_STRING, IDM_HELP_ABOUT, L"About PlatoLives...");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hHelp, L"&Help");

    return hMenuBar;
}

void win_menu_update_state(HMENU menu, const plato_profile_t *p, bool connected, bool diag_log, bool renderer_log, bool fullscreen, const plato_profile_list_t *profiles, int current_profile_idx) {
    if (!menu || !p) return;

    EnableMenuItem(menu, IDM_CONN_CONNECT_DEFAULT, MF_BYCOMMAND | MF_ENABLED);
    EnableMenuItem(menu, IDM_FILE_NEW_WINDOW, MF_BYCOMMAND | MF_ENABLED);
    EnableMenuItem(menu, IDM_CONN_DISCONNECT, connected ? (MF_BYCOMMAND | MF_ENABLED) : (MF_BYCOMMAND | MF_GRAYED));

    /* Modalità Display: 0: Real Plasma, 1: Crisp Mono, 2: Split Mono, 3: Crisp Color, 4: Real CRT */
    UINT mode_id = IDM_VIEW_MODE_REAL_PLASMA;
    if (p->display_mode == 1) mode_id = IDM_VIEW_MODE_CRISP_MONO;
    else if (p->display_mode == 2) mode_id = IDM_VIEW_MODE_SPLIT_MONO;
    else if (p->display_mode == 3) mode_id = IDM_VIEW_MODE_CRISP_COLOR;
    else if (p->display_mode == 4) mode_id = IDM_VIEW_MODE_REAL_CRT;
    else if (p->display_mode == 5) mode_id = IDM_VIEW_MODE_REAL_TEKTRONIX;

    CheckMenuRadioItem(menu, IDM_VIEW_MODE_REAL_PLASMA, IDM_VIEW_MODE_REAL_TEKTRONIX, mode_id, MF_BYCOMMAND);

    CheckMenuItem(menu, IDM_VIEW_TEK_ERASE_FLASH, p->tek_erase_flash ? (MF_BYCOMMAND | MF_CHECKED) : (MF_BYCOMMAND | MF_UNCHECKED));
    CheckMenuItem(menu, IDM_VIEW_TEK_ACCURATE_BG, p->tek_accurate_bg ? (MF_BYCOMMAND | MF_CHECKED) : (MF_BYCOMMAND | MF_UNCHECKED));
    EnableMenuItem(menu, IDM_VIEW_TEK_ERASE_FLASH, p->display_mode == 5 ? (MF_BYCOMMAND | MF_ENABLED) : (MF_BYCOMMAND | MF_GRAYED));
    EnableMenuItem(menu, IDM_VIEW_TEK_ACCURATE_BG, p->display_mode == 5 ? (MF_BYCOMMAND | MF_ENABLED) : (MF_BYCOMMAND | MF_GRAYED));

    /* Sottomenu Ottici in View (Indice 2 nella menubar) */
    HMENU hView = GetSubMenu(menu, 2);
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
        bool isTek        = (p->display_mode == 5);

        /* Posizione 10: CRT Distortion */
        EnableMenuItem(hView, 10, MF_BYPOSITION | (isCRT ? MF_ENABLED : MF_GRAYED));
        /* Posizione 13: Tektronix Connection Speed */
        EnableMenuItem(hView, 13, MF_BYPOSITION | (isTek ? MF_ENABLED : MF_GRAYED));
        /* Posizione 18: Override Color (disabilitato in Crisp Color e Tektronix P31) */
        bool canOverrideColor = (p->display_mode != 3 && p->display_mode != 5);
        EnableMenuItem(hView, 18, MF_BYPOSITION | (canOverrideColor ? MF_ENABLED : MF_GRAYED));
    }

    /* 5. Micro Tutor (Indice 5 nella menubar): non disponibile su Tektronix 4014 (DVBST) */
    bool isTekMode = (p->display_mode == 5);
    EnableMenuItem(menu, 5, MF_BYPOSITION | (isTekMode ? MF_GRAYED : MF_ENABLED));
    HMENU hMicroTutorMenu = GetSubMenu(menu, 5);
    if (hMicroTutorMenu) {
        EnableMenuItem(hMicroTutorMenu, IDM_TOOLS_BOOT_MTE, MF_BYCOMMAND | (isTekMode ? MF_GRAYED : MF_ENABLED));
        EnableMenuItem(hMicroTutorMenu, IDM_TOOLS_MT_LOG, MF_BYCOMMAND | (isTekMode ? MF_GRAYED : MF_ENABLED));
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

    /* Tektronix Connection Speed Checkmark */
    UINT tek_speed_id = IDM_VIEW_TEK_SPEED_2400;
    if (p->tek_baud_rate == 0) tek_speed_id = IDM_VIEW_TEK_SPEED_INSTANT;
    else if (p->tek_baud_rate == 110) tek_speed_id = IDM_VIEW_TEK_SPEED_110;
    else if (p->tek_baud_rate == 150) tek_speed_id = IDM_VIEW_TEK_SPEED_150;
    else if (p->tek_baud_rate == 300) tek_speed_id = IDM_VIEW_TEK_SPEED_300;
    else if (p->tek_baud_rate == 600) tek_speed_id = IDM_VIEW_TEK_SPEED_600;
    else if (p->tek_baud_rate == 1200) tek_speed_id = IDM_VIEW_TEK_SPEED_1200;
    else if (p->tek_baud_rate == 2400) tek_speed_id = IDM_VIEW_TEK_SPEED_2400;
    else if (p->tek_baud_rate == 4800) tek_speed_id = IDM_VIEW_TEK_SPEED_4800;
    else if (p->tek_baud_rate == 9600) tek_speed_id = IDM_VIEW_TEK_SPEED_9600;

    HMENU hTekSpeedSub = hView ? GetSubMenu(hView, 13) : NULL;
    if (hTekSpeedSub) {
        CheckMenuRadioItem(hTekSpeedSub, IDM_VIEW_TEK_SPEED_INSTANT, IDM_VIEW_TEK_SPEED_9600, tek_speed_id, MF_BYCOMMAND);
    }
    CheckMenuRadioItem(menu, IDM_VIEW_TEK_SPEED_INSTANT, IDM_VIEW_TEK_SPEED_9600, tek_speed_id, MF_BYCOMMAND);
    for (UINT sId = IDM_VIEW_TEK_SPEED_INSTANT; sId <= IDM_VIEW_TEK_SPEED_9600; sId++) {
        CheckMenuItem(menu, sId, MF_BYCOMMAND | (sId == tek_speed_id ? MF_CHECKED : MF_UNCHECKED));
        if (hTekSpeedSub) {
            CheckMenuItem(hTekSpeedSub, sId, MF_BYCOMMAND | (sId == tek_speed_id ? MF_CHECKED : MF_UNCHECKED));
        }
    }

    /* Phosphor Color Override */
    CheckMenuRadioItem(menu, IDM_VIEW_COLOR_DEFAULT, IDM_VIEW_COLOR_WHITE,
                       IDM_VIEW_COLOR_DEFAULT + p->phosphor_color, MF_BYCOMMAND);

    /* Profiles Submenu */
    if (profiles && profiles->count > 0 && current_profile_idx >= 0) {
        CheckMenuRadioItem(menu, IDM_PROFILE_BASE, IDM_PROFILE_BASE + (UINT)profiles->count - 1,
                           IDM_PROFILE_BASE + (UINT)current_profile_idx, MF_BYCOMMAND);
    }

    CheckMenuItem(menu, IDM_TOOLS_DIAG_LOG, diag_log ? (MF_BYCOMMAND | MF_CHECKED) : (MF_BYCOMMAND | MF_UNCHECKED));
    CheckMenuItem(menu, IDM_TOOLS_RENDERER_LOG, renderer_log ? (MF_BYCOMMAND | MF_CHECKED) : (MF_BYCOMMAND | MF_UNCHECKED));
    CheckMenuItem(menu, IDM_VIEW_FULLSCREEN, fullscreen ? (MF_BYCOMMAND | MF_CHECKED) : (MF_BYCOMMAND | MF_UNCHECKED));
}
