#define WIN32_LEAN_AND_MEAN
#include "win_profiles.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLATO_PLASMA_COLOR     RGB(255, 110, 0)
#define PLATO_PLASMA_BRIGHT    RGB(255, 160, 40)
#define PLATO_PLASMA_DISABLED  RGB(120, 55, 0)
#define PLATO_BG_DARK          RGB(16, 12, 10)
#define PLATO_BG_BOX           RGB(24, 18, 14)
#define PLATO_BG_HOVER         RGB(45, 20, 0)

#define IDC_LIST_PROFILES       201
#define IDC_EDIT_NAME           202
#define IDC_EDIT_HOST           203
#define IDC_EDIT_PORT           204
#define IDC_COMBO_MODE          205

#define IDC_LBL_PLASMA_DECAY    206
#define IDC_COMBO_PLASMA_DECAY  207
#define IDC_LBL_PLASMA_DIST     208
#define IDC_COMBO_PLASMA_DIST   209

#define IDC_LBL_CRT_BEAM        210
#define IDC_COMBO_CRT_BEAM      211
#define IDC_LBL_CRT_DECAY       212
#define IDC_COMBO_CRT_DECAY     213
#define IDC_LBL_CRT_DIST        214
#define IDC_COMBO_CRT_DIST      215

#define IDC_CHK_FULLSCREEN      216
#define IDC_CHK_DEFAULT         217
#define IDC_CHK_SCRIPT          218
#define IDC_EDIT_SCRIPT         219

#define IDC_BTN_CONNECT         220
#define IDC_BTN_SAVE            221
#define IDC_BTN_NEW             222
#define IDC_BTN_DELETE          223
#define IDC_BTN_DEFAULT         224
#define IDC_BTN_CLOSE           225

typedef struct {
    HWND hwnd;
    HWND hList;
    HWND hEditName, hEditHost, hEditPort;
    HWND hComboMode;
    HWND hLblPlasmaDecay, hComboPlasmaDecay;
    HWND hLblPlasmaDist, hComboPlasmaDist;
    HWND hLblCrtBeam, hComboCrtBeam;
    HWND hLblCrtDecay, hComboCrtDecay;
    HWND hLblCrtDist, hComboCrtDist;
    HWND hChkFullscreen, hChkDefault, hChkScript, hEditScript;
    HWND hBtnNew, hBtnDelete, hBtnDefault, hBtnConnect, hBtnSave, hBtnClose;

    HBRUSH hbrBg;
    HBRUSH hbrBox;
    HBRUSH hbrHover;
    HPEN hPenBorder;
    HPEN hPenHighlight;
    HFONT hFontUI;
    HFONT hFontMono;

    plato_profile_list_t *list;
    int current_index;
    win_profile_connect_cb connect_cb;
    void *user_data;
} dlg_ctx_t;

static dlg_ctx_t g_dlg;

static const int g_decay_ms_values[] = { 100, 200, 500, 1000, 2000, 5000 };
static const int g_crt_decay_ms_values[] = { 20, 200, 500, 1000, 2000, 5000 };

static int index_for_ms(const int *arr, size_t count, int val) {
    for (size_t i = 0; i < count; i++) {
        if (arr[i] == val) return (int)i;
    }
    return 0;
}

static void dlg_update_visibility(dlg_ctx_t *ctx, int mode) {
    bool isPlasma = (mode == 0 || mode == 2); /* Real Plasma o Split */
    bool isCRT = (mode == 4);                 /* Real Color CRT */

    ShowWindow(ctx->hLblPlasmaDecay, isPlasma ? SW_SHOW : SW_HIDE);
    ShowWindow(ctx->hComboPlasmaDecay, isPlasma ? SW_SHOW : SW_HIDE);
    ShowWindow(ctx->hLblPlasmaDist, isPlasma ? SW_SHOW : SW_HIDE);
    ShowWindow(ctx->hComboPlasmaDist, isPlasma ? SW_SHOW : SW_HIDE);

    ShowWindow(ctx->hLblCrtBeam, isCRT ? SW_SHOW : SW_HIDE);
    ShowWindow(ctx->hComboCrtBeam, isCRT ? SW_SHOW : SW_HIDE);
    ShowWindow(ctx->hLblCrtDecay, isCRT ? SW_SHOW : SW_HIDE);
    ShowWindow(ctx->hComboCrtDecay, isCRT ? SW_SHOW : SW_HIDE);
    ShowWindow(ctx->hLblCrtDist, isCRT ? SW_SHOW : SW_HIDE);
    ShowWindow(ctx->hComboCrtDist, isCRT ? SW_SHOW : SW_HIDE);
}

static void dlg_populate_fields(dlg_ctx_t *ctx, int index) {
    if (index < 0 || (size_t)index >= ctx->list->count) return;
    ctx->current_index = index;
    const plato_profile_t *p = &ctx->list->profiles[index];

    SetWindowTextA(ctx->hEditName, p->name);
    SetWindowTextA(ctx->hEditHost, p->host);
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", p->port);
    SetWindowTextA(ctx->hEditPort, port_str);

    SendMessage(ctx->hComboMode, CB_SETCURSEL, (WPARAM)p->display_mode, 0);

    /* Plasma */
    int d_idx = index_for_ms(g_decay_ms_values, 6, p->persistence_ms);
    SendMessage(ctx->hComboPlasmaDecay, CB_SETCURSEL, (WPARAM)d_idx, 0);
    SendMessage(ctx->hComboPlasmaDist, CB_SETCURSEL, (WPARAM)p->plasma_distortion, 0);

    /* CRT */
    SendMessage(ctx->hComboCrtBeam, CB_SETCURSEL, (WPARAM)p->crt_beam_level, 0);
    int cd_idx = index_for_ms(g_crt_decay_ms_values, 6, p->crt_persistence_ms);
    SendMessage(ctx->hComboCrtDecay, CB_SETCURSEL, (WPARAM)cd_idx, 0);
    SendMessage(ctx->hComboCrtDist, CB_SETCURSEL, (WPARAM)p->crt_distortion, 0);

    SendMessage(ctx->hChkFullscreen, BM_SETCHECK, p->full_screen ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(ctx->hChkDefault, BM_SETCHECK, p->is_default ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(ctx->hChkScript, BM_SETCHECK, p->startup_script_enabled ? BST_CHECKED : BST_UNCHECKED, 0);

    /* Template o Script esistente */
    const char *src_script = p->startup_script;
    if (!src_script || strlen(src_script) == 0) {
        src_script = "wait 2s\nkey NEXT\nwait 5s\nsend user\nkey NEXT\nwait 3s\nsend group\nkey SHIFT-STOP\nwait 3s\nsend password\nwait 1s\nkey NEXT";
    }

    char crlf_script[PLATO_SCRIPT_MAX_LEN * 2];
    size_t c_idx = 0;
    for (size_t i = 0; src_script[i] && c_idx < sizeof(crlf_script) - 3; i++) {
        if (src_script[i] == '\n') {
            crlf_script[c_idx++] = '\r';
            crlf_script[c_idx++] = '\n';
        } else if (src_script[i] == '\r') {
            continue;
        } else {
            crlf_script[c_idx++] = src_script[i];
        }
    }
    crlf_script[c_idx] = 0;
    SetWindowTextA(ctx->hEditScript, crlf_script);

    dlg_update_visibility(ctx, p->display_mode);
}

static void dlg_refresh_list(dlg_ctx_t *ctx) {
    SendMessage(ctx->hList, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < ctx->list->count; i++) {
        char item_text[128];
        snprintf(item_text, sizeof(item_text), "%s%s",
                 ctx->list->profiles[i].name,
                 ctx->list->profiles[i].is_default ? " (Default)" : "");
        SendMessageA(ctx->hList, LB_ADDSTRING, 0, (LPARAM)item_text);
    }
    SendMessage(ctx->hList, LB_SETCURSEL, (WPARAM)ctx->current_index, 0);
    dlg_populate_fields(ctx, ctx->current_index);
}

static void dlg_save_current(dlg_ctx_t *ctx) {
    if (ctx->current_index < 0 || (size_t)ctx->current_index >= ctx->list->count) return;
    plato_profile_t *p = &ctx->list->profiles[ctx->current_index];

    GetWindowTextA(ctx->hEditName, p->name, sizeof(p->name));
    GetWindowTextA(ctx->hEditHost, p->host, sizeof(p->host));
    char port_str[16];
    GetWindowTextA(ctx->hEditPort, port_str, sizeof(port_str));
    p->port = atoi(port_str);
    if (p->port <= 0) p->port = 8005;

    p->display_mode = (int)SendMessage(ctx->hComboMode, CB_GETCURSEL, 0, 0);

    int d_idx = (int)SendMessage(ctx->hComboPlasmaDecay, CB_GETCURSEL, 0, 0);
    p->persistence_ms = (d_idx >= 0 && d_idx < 6) ? g_decay_ms_values[d_idx] : 100;
    p->plasma_distortion = (int)SendMessage(ctx->hComboPlasmaDist, CB_GETCURSEL, 0, 0);

    p->crt_beam_level = (int)SendMessage(ctx->hComboCrtBeam, CB_GETCURSEL, 0, 0);
    int cd_idx = (int)SendMessage(ctx->hComboCrtDecay, CB_GETCURSEL, 0, 0);
    p->crt_persistence_ms = (cd_idx >= 0 && cd_idx < 6) ? g_crt_decay_ms_values[cd_idx] : 20;
    p->crt_distortion = (int)SendMessage(ctx->hComboCrtDist, CB_GETCURSEL, 0, 0);

    p->full_screen = (SendMessage(ctx->hChkFullscreen, BM_GETCHECK, 0, 0) == BST_CHECKED);
    p->startup_script_enabled = (SendMessage(ctx->hChkScript, BM_GETCHECK, 0, 0) == BST_CHECKED);

    bool is_def = (SendMessage(ctx->hChkDefault, BM_GETCHECK, 0, 0) == BST_CHECKED);
    if (is_def) {
        plato_profiles_set_default(ctx->list, (size_t)ctx->current_index);
    }

    char raw_script[PLATO_SCRIPT_MAX_LEN * 2];
    GetWindowTextA(ctx->hEditScript, raw_script, sizeof(raw_script));

    size_t d_idx_out = 0;
    for (size_t i = 0; raw_script[i] && d_idx_out < sizeof(p->startup_script) - 1; i++) {
        if (raw_script[i] == '\r') continue;
        p->startup_script[d_idx_out++] = raw_script[i];
    }
    p->startup_script[d_idx_out] = 0;

    plato_profiles_save(ctx->list);
}

static LRESULT CALLBACK ProfilesDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    dlg_ctx_t *ctx = &g_dlg;
    switch (msg) {
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, PLATO_PLASMA_COLOR);
            return (LRESULT)ctx->hbrBg;
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, PLATO_BG_BOX);
            SetTextColor(hdc, PLATO_PLASMA_COLOR);
            return (LRESULT)ctx->hbrBox;
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
            if (!dis) break;

            if (dis->CtlType == ODT_LISTBOX && dis->CtlID == IDC_LIST_PROFILES) {
                if ((int)dis->itemID < 0) return TRUE;

                bool is_selected = (dis->itemState & ODS_SELECTED);
                HBRUSH fill_brush = is_selected ? ctx->hbrHover : ctx->hbrBg;
                FillRect(dis->hDC, &dis->rcItem, fill_brush);

                if (is_selected) {
                    HPEN old_pen = (HPEN)SelectObject(dis->hDC, ctx->hPenBorder);
                    HBRUSH old_brush = (HBRUSH)SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
                    Rectangle(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom);
                    SelectObject(dis->hDC, old_pen);
                    SelectObject(dis->hDC, old_brush);
                }

                char text[128] = {0};
                SendMessageA(dis->hwndItem, LB_GETTEXT, dis->itemID, (LPARAM)text);

                SetBkMode(dis->hDC, TRANSPARENT);
                SetTextColor(dis->hDC, is_selected ? PLATO_PLASMA_BRIGHT : PLATO_PLASMA_COLOR);

                RECT text_rc = dis->rcItem;
                text_rc.left += 6;
                DrawTextA(dis->hDC, text, -1, &text_rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                return TRUE;
            }

            if (dis->CtlType == ODT_BUTTON) {
                bool is_pressed = (dis->itemState & ODS_SELECTED);
                bool is_disabled = (dis->itemState & ODS_DISABLED);

                HBRUSH fill_brush = is_pressed ? ctx->hbrHover : ctx->hbrBg;
                FillRect(dis->hDC, &dis->rcItem, fill_brush);

                HPEN pen = is_disabled ? ctx->hPenBorder : (is_pressed ? ctx->hPenHighlight : ctx->hPenBorder);
                HPEN old_pen = (HPEN)SelectObject(dis->hDC, pen);
                HBRUSH old_brush = (HBRUSH)SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
                Rectangle(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom);
                SelectObject(dis->hDC, old_pen);
                SelectObject(dis->hDC, old_brush);

                WCHAR wtext[64] = {0};
                GetWindowTextW(dis->hwndItem, wtext, (int)(sizeof(wtext) / sizeof(WCHAR)));

                SetBkMode(dis->hDC, TRANSPARENT);
                COLORREF text_color = is_disabled ? PLATO_PLASMA_DISABLED : (is_pressed ? PLATO_PLASMA_BRIGHT : PLATO_PLASMA_COLOR);
                SetTextColor(dis->hDC, text_color);

                RECT rc = dis->rcItem;
                DrawTextW(dis->hDC, wtext, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                return TRUE;
            }
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);

            if (wmId == IDC_LIST_PROFILES && wmEvent == LBN_SELCHANGE) {
                dlg_save_current(ctx);
                int sel = (int)SendMessage(ctx->hList, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR) dlg_populate_fields(ctx, sel);
                return 0;
            }

            if (wmId == IDC_COMBO_MODE && wmEvent == CBN_SELCHANGE) {
                int mode = (int)SendMessage(ctx->hComboMode, CB_GETCURSEL, 0, 0);
                dlg_update_visibility(ctx, mode);
                return 0;
            }

            switch (wmId) {
                case IDC_BTN_CONNECT:
                    dlg_save_current(ctx);
                    if (ctx->connect_cb && ctx->current_index >= 0 && (size_t)ctx->current_index < ctx->list->count) {
                        ctx->connect_cb(&ctx->list->profiles[ctx->current_index], ctx->user_data);
                    }
                    DestroyWindow(hwnd);
                    return 0;

                case IDC_BTN_SAVE:
                    dlg_save_current(ctx);
                    dlg_refresh_list(ctx);
                    MessageBoxW(hwnd, L"Profile settings saved.", L"PlatoLives", MB_OK | MB_ICONINFORMATION);
                    return 0;

                case IDC_BTN_NEW: {
                    dlg_save_current(ctx);
                    plato_profile_t np = {0};
                    snprintf(np.name, sizeof(np.name), "Profile %d", (int)ctx->list->count + 1);
                    snprintf(np.host, sizeof(np.host), "cyberserv.org");
                    np.port = 8005;
                    np.display_mode = 0;
                    np.persistence_ms = 100;
                    np.plasma_distortion = 2;
                    np.crt_beam_level = 1;
                    np.crt_persistence_ms = 20;
                    np.crt_distortion = 2;
                    snprintf(np.startup_script, sizeof(np.startup_script),
                             "wait 2s\nkey NEXT\nwait 5s\nsend user\nkey NEXT\nwait 3s\nsend group\nkey SHIFT-STOP\nwait 3s\nsend password\nwait 1s\nkey NEXT");
                    plato_profiles_add(ctx->list, &np);
                    ctx->current_index = (int)(ctx->list->count - 1);
                    dlg_refresh_list(ctx);
                    return 0;
                }

                case IDC_BTN_DELETE:
                    if (ctx->list->count <= 1) {
                        MessageBoxW(hwnd, L"Cannot delete the only profile.", L"PlatoLives", MB_OK | MB_ICONWARNING);
                        return 0;
                    }
                    plato_profiles_delete(ctx->list, (size_t)ctx->current_index);
                    if (ctx->current_index >= (int)ctx->list->count) ctx->current_index = (int)ctx->list->count - 1;
                    dlg_refresh_list(ctx);
                    return 0;

                case IDC_BTN_DEFAULT:
                    dlg_save_current(ctx);
                    plato_profiles_set_default(ctx->list, (size_t)ctx->current_index);
                    dlg_refresh_list(ctx);
                    return 0;

                case IDC_BTN_CLOSE:
                case IDCANCEL:
                    dlg_save_current(ctx);
                    DestroyWindow(hwnd);
                    return 0;
            }
            break;
        }

        case WM_DESTROY: {
            if (ctx->hbrBg) DeleteObject(ctx->hbrBg);
            if (ctx->hbrBox) DeleteObject(ctx->hbrBox);
            if (ctx->hbrHover) DeleteObject(ctx->hbrHover);
            if (ctx->hPenBorder) DeleteObject(ctx->hPenBorder);
            if (ctx->hPenHighlight) DeleteObject(ctx->hPenHighlight);
            if (ctx->hFontMono) DeleteObject(ctx->hFontMono);
            memset(ctx, 0, sizeof(*ctx));
            return 0;
        }

        case WM_CLOSE:
            dlg_save_current(ctx);
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void win_profiles_dialog_show(HWND hParent, plato_profile_list_t *list, win_profile_connect_cb cb, void *user_data) {
    if (!list) return;

    memset(&g_dlg, 0, sizeof(g_dlg));
    g_dlg.list = list;
    g_dlg.current_index = list->default_index;
    g_dlg.connect_cb = cb;
    g_dlg.user_data = user_data;

    g_dlg.hbrBg = CreateSolidBrush(PLATO_BG_DARK);
    g_dlg.hbrBox = CreateSolidBrush(PLATO_BG_BOX);
    g_dlg.hbrHover = CreateSolidBrush(PLATO_BG_HOVER);
    g_dlg.hPenBorder = CreatePen(PS_SOLID, 1, PLATO_PLASMA_COLOR);
    g_dlg.hPenHighlight = CreatePen(PS_SOLID, 1, PLATO_PLASMA_BRIGHT);

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = ProfilesDlgProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_dlg.hbrBg;
    wc.lpszClassName = L"PlatoProfilesDlgClassEx";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        wc.lpszClassName, L"Connection Profiles - PlatoLives",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 640,
        hParent, NULL, wc.hInstance, NULL
    );
    if (!hwnd) return;
    g_dlg.hwnd = hwnd;

    /* DWM Dark Caption e Orange Title via dynamic linking */
    HMODULE hDwm = LoadLibraryA("dwmapi.dll");
    if (hDwm) {
        typedef HRESULT (WINAPI *fnDwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
        fnDwmSetWindowAttribute pDwm = (fnDwmSetWindowAttribute)GetProcAddress(hDwm, "DwmSetWindowAttribute");
        if (pDwm) {
            BOOL dark_mode = TRUE;
            pDwm(hwnd, 20, &dark_mode, sizeof(dark_mode)); /* DWMWA_USE_IMMERSIVE_DARK_MODE */
            pDwm(hwnd, 19, &dark_mode, sizeof(dark_mode)); /* Legacy fallback */
            COLORREF caption_bg = PLATO_BG_DARK;
            COLORREF caption_fg = PLATO_PLASMA_COLOR;
            pDwm(hwnd, 35, &caption_bg, sizeof(caption_bg)); /* DWMWA_CAPTION_COLOR */
            pDwm(hwnd, 36, &caption_fg, sizeof(caption_fg)); /* DWMWA_TEXT_COLOR */
        }
        FreeLibrary(hDwm);
    }

    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT hFontMono = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                  FIXED_PITCH | FF_MODERN, L"Consolas");
    g_dlg.hFontUI = hFont;
    g_dlg.hFontMono = hFontMono;

    /* Sinistra: Lista Profili */
    CreateWindowW(L"STATIC", L"Profiles:", WS_CHILD | WS_VISIBLE, 16, 12, 180, 18, hwnd, NULL, wc.hInstance, NULL);
    g_dlg.hList = CreateWindowW(L"LISTBOX", L"",
                                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
                                16, 32, 190, 500, hwnd, (HMENU)IDC_LIST_PROFILES, wc.hInstance, NULL);
    SendMessage(g_dlg.hList, LB_SETITEMHEIGHT, 0, 22);

    g_dlg.hBtnNew = CreateWindowW(L"BUTTON", L"+ New", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 16, 542, 60, 26, hwnd, (HMENU)IDC_BTN_NEW, wc.hInstance, NULL);
    g_dlg.hBtnDelete = CreateWindowW(L"BUTTON", L"Delete", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 82, 542, 60, 26, hwnd, (HMENU)IDC_BTN_DELETE, wc.hInstance, NULL);
    g_dlg.hBtnDefault = CreateWindowW(L"BUTTON", L"Default", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 148, 542, 58, 26, hwnd, (HMENU)IDC_BTN_DEFAULT, wc.hInstance, NULL);

    /* Destra: Dettagli */
    int rx = 226;
    CreateWindowW(L"STATIC", L"Profile Name:", WS_CHILD | WS_VISIBLE, rx, 12, 120, 18, hwnd, NULL, wc.hInstance, NULL);
    g_dlg.hEditName = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, rx, 32, 460, 22, hwnd, (HMENU)IDC_EDIT_NAME, wc.hInstance, NULL);

    CreateWindowW(L"STATIC", L"Server Host:", WS_CHILD | WS_VISIBLE, rx, 60, 140, 18, hwnd, NULL, wc.hInstance, NULL);
    g_dlg.hEditHost = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, rx, 78, 350, 22, hwnd, (HMENU)IDC_EDIT_HOST, wc.hInstance, NULL);

    CreateWindowW(L"STATIC", L"Port:", WS_CHILD | WS_VISIBLE, rx + 365, 60, 80, 18, hwnd, NULL, wc.hInstance, NULL);
    g_dlg.hEditPort = CreateWindowW(L"EDIT", L"8005", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, rx + 365, 78, 95, 22, hwnd, (HMENU)IDC_EDIT_PORT, wc.hInstance, NULL);

    CreateWindowW(L"STATIC", L"Display Mode:", WS_CHILD | WS_VISIBLE, rx, 108, 120, 18, hwnd, NULL, wc.hInstance, NULL);
    g_dlg.hComboMode = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, rx, 126, 460, 150, hwnd, (HMENU)IDC_COMBO_MODE, wc.hInstance, NULL);
    SendMessageW(g_dlg.hComboMode, CB_ADDSTRING, 0, (LPARAM)L"Real Plasma");
    SendMessageW(g_dlg.hComboMode, CB_ADDSTRING, 0, (LPARAM)L"Crisp Monochrome");
    SendMessageW(g_dlg.hComboMode, CB_ADDSTRING, 0, (LPARAM)L"Split Monochrome");
    SendMessageW(g_dlg.hComboMode, CB_ADDSTRING, 0, (LPARAM)L"Crisp Color");
    SendMessageW(g_dlg.hComboMode, CB_ADDSTRING, 0, (LPARAM)L"Real Color CRT");

    /* Controlli Real Plasma */
    g_dlg.hLblPlasmaDecay = CreateWindowW(L"STATIC", L"Real Plasma Persistence:", WS_CHILD | WS_VISIBLE, rx, 156, 220, 18, hwnd, NULL, wc.hInstance, NULL);
    g_dlg.hComboPlasmaDecay = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, rx, 174, 220, 140, hwnd, (HMENU)IDC_COMBO_PLASMA_DECAY, wc.hInstance, NULL);
    SendMessageW(g_dlg.hComboPlasmaDecay, CB_ADDSTRING, 0, (LPARAM)L"100 ms (Authentic Plasma)");
    SendMessageW(g_dlg.hComboPlasmaDecay, CB_ADDSTRING, 0, (LPARAM)L"200 ms (Warm Glow)");
    SendMessageW(g_dlg.hComboPlasmaDecay, CB_ADDSTRING, 0, (LPARAM)L"500 ms (Medium)");
    SendMessageW(g_dlg.hComboPlasmaDecay, CB_ADDSTRING, 0, (LPARAM)L"1000 ms (Long/Radar)");
    SendMessageW(g_dlg.hComboPlasmaDecay, CB_ADDSTRING, 0, (LPARAM)L"2000 ms (Ultra Persistence)");
    SendMessageW(g_dlg.hComboPlasmaDecay, CB_ADDSTRING, 0, (LPARAM)L"5000 ms (5s Extreme)");

    g_dlg.hLblPlasmaDist = CreateWindowW(L"STATIC", L"Real Plasma Distortion:", WS_CHILD | WS_VISIBLE, rx + 235, 156, 220, 18, hwnd, NULL, wc.hInstance, NULL);
    g_dlg.hComboPlasmaDist = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, rx + 235, 174, 225, 120, hwnd, (HMENU)IDC_COMBO_PLASMA_DIST, wc.hInstance, NULL);
    SendMessageW(g_dlg.hComboPlasmaDist, CB_ADDSTRING, 0, (LPARAM)L"None (Flat)");
    SendMessageW(g_dlg.hComboPlasmaDist, CB_ADDSTRING, 0, (LPARAM)L"Barrel (Curved Glass)");
    SendMessageW(g_dlg.hComboPlasmaDist, CB_ADDSTRING, 0, (LPARAM)L"Cylindrical (Default)");

    /* Controlli Real Color CRT */
    g_dlg.hLblCrtBeam = CreateWindowW(L"STATIC", L"CRT Beam Profile:", WS_CHILD | WS_VISIBLE, rx, 156, 145, 18, hwnd, NULL, wc.hInstance, NULL);
    g_dlg.hComboCrtBeam = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, rx, 174, 145, 120, hwnd, (HMENU)IDC_COMBO_CRT_BEAM, wc.hInstance, NULL);
    SendMessageW(g_dlg.hComboCrtBeam, CB_ADDSTRING, 0, (LPARAM)L"Standard (Authentic 13\")");
    SendMessageW(g_dlg.hComboCrtBeam, CB_ADDSTRING, 0, (LPARAM)L"High (Soft Glow)");
    SendMessageW(g_dlg.hComboCrtBeam, CB_ADDSTRING, 0, (LPARAM)L"Ultra (Vintage Arcade)");

    g_dlg.hLblCrtDecay = CreateWindowW(L"STATIC", L"CRT Persistence:", WS_CHILD | WS_VISIBLE, rx + 155, 156, 150, 18, hwnd, NULL, wc.hInstance, NULL);
    g_dlg.hComboCrtDecay = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, rx + 155, 174, 150, 140, hwnd, (HMENU)IDC_COMBO_CRT_DECAY, wc.hInstance, NULL);
    SendMessageW(g_dlg.hComboCrtDecay, CB_ADDSTRING, 0, (LPARAM)L"20 ms (Default CRT)");
    SendMessageW(g_dlg.hComboCrtDecay, CB_ADDSTRING, 0, (LPARAM)L"200 ms (Warm Glow)");
    SendMessageW(g_dlg.hComboCrtDecay, CB_ADDSTRING, 0, (LPARAM)L"500 ms (Medium)");
    SendMessageW(g_dlg.hComboCrtDecay, CB_ADDSTRING, 0, (LPARAM)L"1000 ms (Long/Radar)");
    SendMessageW(g_dlg.hComboCrtDecay, CB_ADDSTRING, 0, (LPARAM)L"2000 ms (Ultra Persistence)");
    SendMessageW(g_dlg.hComboCrtDecay, CB_ADDSTRING, 0, (LPARAM)L"5000 ms (5s Extreme)");

    g_dlg.hLblCrtDist = CreateWindowW(L"STATIC", L"CRT Distortion:", WS_CHILD | WS_VISIBLE, rx + 315, 156, 145, 18, hwnd, NULL, wc.hInstance, NULL);
    g_dlg.hComboCrtDist = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, rx + 315, 174, 145, 120, hwnd, (HMENU)IDC_COMBO_CRT_DIST, wc.hInstance, NULL);
    SendMessageW(g_dlg.hComboCrtDist, CB_ADDSTRING, 0, (LPARAM)L"None (Flat)");
    SendMessageW(g_dlg.hComboCrtDist, CB_ADDSTRING, 0, (LPARAM)L"Barrel (Curved Glass)");
    SendMessageW(g_dlg.hComboCrtDist, CB_ADDSTRING, 0, (LPARAM)L"Cylindrical");

    /* Checkbox Opzioni */
    g_dlg.hChkFullscreen = CreateWindowW(L"BUTTON", L"Launch in Full Screen", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, rx, 206, 200, 18, hwnd, (HMENU)IDC_CHK_FULLSCREEN, wc.hInstance, NULL);
    g_dlg.hChkDefault = CreateWindowW(L"BUTTON", L"Default profile at startup", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, rx + 220, 206, 220, 18, hwnd, (HMENU)IDC_CHK_DEFAULT, wc.hInstance, NULL);
    g_dlg.hChkScript = CreateWindowW(L"BUTTON", L"Run Startup Script on connect", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, rx, 228, 400, 18, hwnd, (HMENU)IDC_CHK_SCRIPT, wc.hInstance, NULL);

    /* Script Editor */
    CreateWindowW(L"STATIC", L"Startup Script:", WS_CHILD | WS_VISIBLE, rx, 252, 200, 18, hwnd, NULL, wc.hInstance, NULL);
    g_dlg.hEditScript = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | WS_VSCROLL,
                                      rx, 272, 460, 220, hwnd, (HMENU)IDC_EDIT_SCRIPT, wc.hInstance, NULL);

    /* Pulsanti Azione */
    g_dlg.hBtnConnect = CreateWindowW(L"BUTTON", L"Connect", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, rx + 160, 502, 90, 30, hwnd, (HMENU)IDC_BTN_CONNECT, wc.hInstance, NULL);
    g_dlg.hBtnSave = CreateWindowW(L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, rx + 260, 502, 90, 30, hwnd, (HMENU)IDC_BTN_SAVE, wc.hInstance, NULL);
    g_dlg.hBtnClose = CreateWindowW(L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, rx + 360, 502, 100, 30, hwnd, (HMENU)IDC_BTN_CLOSE, wc.hInstance, NULL);

    /* Applica Font */
    HWND hChild = GetWindow(hwnd, GW_CHILD);
    while (hChild) {
        if (hChild == g_dlg.hEditScript && hFontMono) {
            SendMessage(hChild, WM_SETFONT, (WPARAM)hFontMono, TRUE);
        } else {
            SendMessage(hChild, WM_SETFONT, (WPARAM)hFont, TRUE);
        }
        hChild = GetWindow(hChild, GW_HWNDNEXT);
    }

    dlg_refresh_list(&g_dlg);

    RECT pr, dr;
    GetWindowRect(hParent, &pr);
    GetWindowRect(hwnd, &dr);
    int dw = dr.right - dr.left, dh = dr.bottom - dr.top;
    int px = pr.left + ((pr.right - pr.left) - dw) / 2;
    int py = pr.top + ((pr.bottom - pr.top) - dh) / 2;
    SetWindowPos(hwnd, HWND_TOP, px, py, dw, dh, SWP_SHOWWINDOW);

    MSG msg;
    while (IsWindow(hwnd) && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
