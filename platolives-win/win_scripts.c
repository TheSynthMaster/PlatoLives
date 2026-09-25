#define WIN32_LEAN_AND_MEAN
#include "win_scripts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLATO_PLASMA_COLOR     RGB(255, 110, 0)
#define PLATO_PLASMA_BRIGHT    RGB(255, 160, 40)
#define PLATO_PLASMA_DISABLED  RGB(120, 55, 0)
#define PLATO_BG_DARK          RGB(16, 12, 10)
#define PLATO_BG_BOX           RGB(24, 18, 14)
#define PLATO_BG_HOVER         RGB(45, 20, 0)

#define IDC_SCR_LIST           401
#define IDC_SCR_BTN_NEW        402
#define IDC_SCR_BTN_CLONE      403
#define IDC_SCR_BTN_DELETE     404
#define IDC_SCR_EDIT_NAME      405
#define IDC_SCR_CHK_ENABLED    406
#define IDC_SCR_EDIT_CHAR_DEL  407
#define IDC_SCR_EDIT_NEXT_DEL  408
#define IDC_SCR_EDIT_CMD_DEL   409
#define IDC_SCR_LBL_HOTKEY     410
#define IDC_SCR_BTN_RECORD     411
#define IDC_SCR_BTN_CLEAR_KEY  412
#define IDC_SCR_EDIT_BODY      413
#define IDC_SCR_BTN_SAVE       415
#define IDC_SCR_BTN_CLOSE      416

typedef struct {
    HWND hwnd;
    HWND hList;
    HWND hBtnNew, hBtnClone, hBtnDelete;
    HWND hEditName, hChkEnabled;
    HWND hEditCharDel, hEditNextDel, hEditCmdDel;
    HWND hLblHotkey, hBtnRecord, hBtnClearKey;
    HWND hEditBody;
    HWND hBtnSave, hBtnClose;

    HBRUSH hbrBg, hbrBox, hbrHover;
    HPEN hPenBorder, hPenHighlight;
    HFONT hFontUI, hFontMono;

    plato_script_list_t *list;
    plato_terminal_t *terminal;
    int current_index;
    bool is_recording;
} scr_dlg_ctx_t;

static scr_dlg_ctx_t g_scr_dlg;

static void scr_populate_fields(scr_dlg_ctx_t *ctx, int index) {
    if (index < 0 || (size_t)index >= ctx->list->count) return;
    ctx->current_index = index;
    const plato_script_t *s = &ctx->list->scripts[index];

    SetWindowTextA(ctx->hEditName, s->name);
    SendMessage(ctx->hChkEnabled, BM_SETCHECK, s->enabled ? BST_CHECKED : BST_UNCHECKED, 0);

    char num_buf[32];
    snprintf(num_buf, sizeof(num_buf), "%d", s->char_delay_ms);
    SetWindowTextA(ctx->hEditCharDel, num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", s->next_delay_ms);
    SetWindowTextA(ctx->hEditNextDel, num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", s->command_delay_ms);
    SetWindowTextA(ctx->hEditCmdDel, num_buf);

    char hk_text[64];
    snprintf(hk_text, sizeof(hk_text), "Key: %s", (s->hotkey_display[0] ? s->hotkey_display : "None"));
    SetWindowTextA(ctx->hLblHotkey, hk_text);

    char crlf_buf[PLATO_SCRIPT_MAX_BODY * 2];
    size_t o = 0;
    for (size_t i = 0; s->body[i] && o + 3 < sizeof(crlf_buf); i++) {
        if (s->body[i] == 10) { crlf_buf[o++] = 13; crlf_buf[o++] = 10; }
        else if (s->body[i] == 13) { continue; }
        else { crlf_buf[o++] = s->body[i]; }
    }
    crlf_buf[o] = 0;
    SetWindowTextA(ctx->hEditBody, crlf_buf);
}

static void scr_refresh_list(scr_dlg_ctx_t *ctx) {
    SendMessage(ctx->hList, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < ctx->list->count; i++) {
        char item_text[128];
        const plato_script_t *s = &ctx->list->scripts[i];
        snprintf(item_text, sizeof(item_text), "%s %s%s%s",
                 s->enabled ? "[ON]" : "[OFF]", s->name,
                 s->hotkey_display[0] ? " (" : "",
                 s->hotkey_display[0] ? s->hotkey_display : "");
        if (s->hotkey_display[0]) strcat(item_text, ")");
        SendMessageA(ctx->hList, LB_ADDSTRING, 0, (LPARAM)item_text);
    }
    SendMessage(ctx->hList, LB_SETCURSEL, (WPARAM)ctx->current_index, 0);
    scr_populate_fields(ctx, ctx->current_index);
}

static void scr_save_current(scr_dlg_ctx_t *ctx) {
    if (ctx->current_index < 0 || (size_t)ctx->current_index >= ctx->list->count) return;
    plato_script_t *s = &ctx->list->scripts[ctx->current_index];

    GetWindowTextA(ctx->hEditName, s->name, sizeof(s->name));
    s->enabled = (SendMessage(ctx->hChkEnabled, BM_GETCHECK, 0, 0) == BST_CHECKED);

    char num_buf[32];
    GetWindowTextA(ctx->hEditCharDel, num_buf, sizeof(num_buf));
    s->char_delay_ms = atoi(num_buf);
    if (s->char_delay_ms < 0) s->char_delay_ms = 0;

    GetWindowTextA(ctx->hEditNextDel, num_buf, sizeof(num_buf));
    s->next_delay_ms = atoi(num_buf);
    if (s->next_delay_ms < 0) s->next_delay_ms = 0;

    GetWindowTextA(ctx->hEditCmdDel, num_buf, sizeof(num_buf));
    s->command_delay_ms = atoi(num_buf);
    if (s->command_delay_ms < 0) s->command_delay_ms = 0;

    char raw_body[PLATO_SCRIPT_MAX_BODY * 2];
    GetWindowTextA(ctx->hEditBody, raw_body, sizeof(raw_body));
    size_t o = 0;
    for (size_t i = 0; raw_body[i] && o + 1 < sizeof(s->body); i++) {
        if (raw_body[i] == 13) continue;
        s->body[o++] = raw_body[i];
    }
    s->body[o] = 0;
    plato_scripts_save(ctx->list, NULL);
}

static bool is_reserved_shortcut(uint32_t mods, uint32_t vk) {
    bool ctrl = (mods & PLATO_HOTKEY_MOD_CTRL) != 0;
    if (ctrl && (vk == 67 || vk == 86 || vk == 84 || vk == 190)) return true;
    if (vk == VK_F11 || vk == VK_F12) return true;
    if ((mods & PLATO_HOTKEY_MOD_ALT) && vk == VK_F4) return true;
    return false;
}


static WNDPROC g_old_edit_box_proc = NULL;
static LRESULT CALLBACK EditBoxSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (ctrl) {
            if (wParam == 'A' || wParam == 'a') {
                SendMessage(hwnd, EM_SETSEL, 0, -1);
                return 0;
            } else if (wParam == 'C' || wParam == 'c') {
                SendMessage(hwnd, WM_COPY, 0, 0);
                return 0;
            } else if (wParam == 'V' || wParam == 'v') {
                SendMessage(hwnd, WM_PASTE, 0, 0);
                return 0;
            }
        }
    }
    return CallWindowProcW(g_old_edit_box_proc, hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK ScriptsDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    scr_dlg_ctx_t *ctx = &g_scr_dlg;
    switch (msg) {
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, ctx->hbrBg);
            return 1;
        }

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

            if (dis->CtlType == ODT_LISTBOX && dis->CtlID == IDC_SCR_LIST) {
                if ((int)dis->itemID < 0) return TRUE;
                bool is_selected = (dis->itemState & ODS_SELECTED);
                HBRUSH fill = is_selected ? ctx->hbrHover : ctx->hbrBg;
                FillRect(dis->hDC, &dis->rcItem, fill);

                if (is_selected) {
                    HPEN oldP = (HPEN)SelectObject(dis->hDC, ctx->hPenBorder);
                    HBRUSH oldB = (HBRUSH)SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
                    Rectangle(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom);
                    SelectObject(dis->hDC, oldP);
                    SelectObject(dis->hDC, oldB);
                }

                char text[128] = {0};
                SendMessageA(dis->hwndItem, LB_GETTEXT, dis->itemID, (LPARAM)text);
                SetBkMode(dis->hDC, TRANSPARENT);
                SetTextColor(dis->hDC, is_selected ? PLATO_PLASMA_BRIGHT : PLATO_PLASMA_COLOR);
                RECT rc = dis->rcItem;
                rc.left += 6;
                DrawTextA(dis->hDC, text, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                return TRUE;
            }

            if (dis->CtlType == ODT_BUTTON) {
                bool is_pressed = (dis->itemState & ODS_SELECTED);
                bool is_disabled = (dis->itemState & ODS_DISABLED);
                HBRUSH fill = is_pressed ? ctx->hbrHover : ctx->hbrBg;
                FillRect(dis->hDC, &dis->rcItem, fill);

                HPEN pen = is_disabled ? ctx->hPenBorder : (is_pressed ? ctx->hPenHighlight : ctx->hPenBorder);
                HPEN oldP = (HPEN)SelectObject(dis->hDC, pen);
                HBRUSH oldB = (HBRUSH)SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
                Rectangle(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom);
                SelectObject(dis->hDC, oldP);
                SelectObject(dis->hDC, oldB);

                WCHAR wtext[64] = {0};
                GetWindowTextW(dis->hwndItem, wtext, (int)(sizeof(wtext)/sizeof(WCHAR)));
                SetBkMode(dis->hDC, TRANSPARENT);
                COLORREF col = is_disabled ? PLATO_PLASMA_DISABLED : (is_pressed ? PLATO_PLASMA_BRIGHT : PLATO_PLASMA_COLOR);
                SetTextColor(dis->hDC, col);
                RECT rc = dis->rcItem;
                DrawTextW(dis->hDC, wtext, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                return TRUE;
            }
            break;
        }

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            if (ctx->is_recording) {
                uint32_t vk = (uint32_t)wParam;
                if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU) return 0;

                uint32_t mods = 0;
                char disp[PLATO_SCRIPT_MAX_HOTKEY] = {0};
                if (GetKeyState(VK_CONTROL) & 0x8000) { mods |= PLATO_HOTKEY_MOD_CTRL; strcat(disp, "Ctrl+"); }
                if (GetKeyState(VK_MENU) & 0x8000)    { mods |= PLATO_HOTKEY_MOD_ALT; strcat(disp, "Alt+"); }
                if (GetKeyState(VK_SHIFT) & 0x8000)   { mods |= PLATO_HOTKEY_MOD_SHIFT; strcat(disp, "Shift+"); }

                if (is_reserved_shortcut(mods, vk)) {
                    MessageBoxW(hwnd, L"This key combination is reserved by PlatoLives.\nPlease choose another combination.", L"Reserved Shortcut", MB_OK | MB_ICONWARNING);
                    ctx->is_recording = false;
                    SetWindowTextW(ctx->hBtnRecord, L"Record Key");
                    return 0;
                }

                char key_name[16] = {0};
                if (vk >= VK_F1 && vk <= VK_F12) {
                    snprintf(key_name, sizeof(key_name), "F%d", (int)(vk - VK_F1 + 1));
                } else if ((vk >= 48 && vk <= 57) || (vk >= 65 && vk <= 90)) {
                    snprintf(key_name, sizeof(key_name), "%c", (char)vk);
                } else {
                    snprintf(key_name, sizeof(key_name), "Key#%u", vk);
                }
                strcat(disp, key_name);

                if (ctx->current_index >= 0 && (size_t)ctx->current_index < ctx->list->count) {
                    plato_script_t *s = &ctx->list->scripts[ctx->current_index];
                    s->hotkey_modifiers = mods;
                    s->hotkey_key = vk;
                    snprintf(s->hotkey_display, sizeof(s->hotkey_display), "%s", disp);
                    char hk_lbl[64];
                    snprintf(hk_lbl, sizeof(hk_lbl), "Key: %s", disp);
                    SetWindowTextA(ctx->hLblHotkey, hk_lbl);
                }

                ctx->is_recording = false;
                SetWindowTextW(ctx->hBtnRecord, L"Record Key");
                scr_refresh_list(ctx);
                return 0;
            }
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);

            if (wmId == IDC_SCR_LIST && wmEvent == LBN_SELCHANGE) {
                scr_save_current(ctx);
                int sel = (int)SendMessage(ctx->hList, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR) scr_populate_fields(ctx, sel);
                return 0;
            }

            switch (wmId) {
                case IDC_SCR_BTN_RECORD:
                    ctx->is_recording = !ctx->is_recording;
                    SetWindowTextW(ctx->hBtnRecord, ctx->is_recording ? L"Press key..." : L"Record Key");
                    if (ctx->is_recording) SetFocus(hwnd);
                    return 0;

                case IDC_SCR_BTN_CLEAR_KEY:
                    if (ctx->current_index >= 0 && (size_t)ctx->current_index < ctx->list->count) {
                        plato_script_t *s = &ctx->list->scripts[ctx->current_index];
                        s->hotkey_modifiers = 0;
                        s->hotkey_key = 0;
                        s->hotkey_display[0] = 0;
                        SetWindowTextA(ctx->hLblHotkey, "Key: None");
                        scr_refresh_list(ctx);
                    }
                    return 0;

                case IDC_SCR_BTN_SAVE:
                    scr_save_current(ctx);
                    scr_refresh_list(ctx);
                    MessageBoxW(hwnd, L"Script saved.", L"PlatoLives", MB_OK | MB_ICONINFORMATION);
                    return 0;

                case IDC_SCR_BTN_NEW: {
                    scr_save_current(ctx);
                    plato_script_t ns = {0};
                    snprintf(ns.name, sizeof(ns.name), "Script %zu", ctx->list->count + 1);
                    ns.enabled = true;
                    ns.char_delay_ms = 250;
                    ns.next_delay_ms = 500;
                    ns.command_delay_ms = 0;
                    snprintf(ns.body, sizeof(ns.body), "%s", PLATO_DEFAULT_AUTOLOGIN_SCRIPT);
                    int idx = plato_scripts_add(ctx->list, &ns);
                    ctx->current_index = idx;
                    scr_refresh_list(ctx);
                    return 0;
                }

                case IDC_SCR_BTN_CLONE:
                    if (ctx->current_index >= 0 && (size_t)ctx->current_index < ctx->list->count) {
                        scr_save_current(ctx);
                        int nidx = plato_scripts_clone(ctx->list, (size_t)ctx->current_index);
                        if (nidx >= 0) ctx->current_index = nidx;
                        scr_refresh_list(ctx);
                    }
                    return 0;

                case IDC_SCR_BTN_DELETE:
                    if (ctx->list->count <= 1) {
                        MessageBoxW(hwnd, L"Cannot delete the only script.", L"PlatoLives", MB_OK | MB_ICONWARNING);
                        return 0;
                    }
                    plato_scripts_delete(ctx->list, (size_t)ctx->current_index);
                    if (ctx->current_index >= (int)ctx->list->count) ctx->current_index = (int)ctx->list->count - 1;
                    scr_refresh_list(ctx);
                    return 0;

                case IDC_SCR_BTN_CLOSE:
                case IDCANCEL:
                    scr_save_current(ctx);
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
            UnregisterClassW(L"PlatoScriptsDlgClassEx", GetModuleHandle(NULL));
            ctx->hwnd = NULL;
            return 0;
        }

        case WM_CLOSE:
            scr_save_current(ctx);
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void win_scripts_dialog_show(HWND hParent, plato_script_list_t *list, plato_terminal_t *terminal) {
    if (!list) return;

    if (g_scr_dlg.hwnd && IsWindow(g_scr_dlg.hwnd)) {
        ShowWindow(g_scr_dlg.hwnd, SW_RESTORE);
        SetForegroundWindow(g_scr_dlg.hwnd);
        return;
    }

    memset(&g_scr_dlg, 0, sizeof(g_scr_dlg));
    g_scr_dlg.list = list;
    g_scr_dlg.terminal = terminal;
    g_scr_dlg.current_index = 0;

    g_scr_dlg.hbrBg = CreateSolidBrush(PLATO_BG_DARK);
    g_scr_dlg.hbrBox = CreateSolidBrush(PLATO_BG_BOX);
    g_scr_dlg.hbrHover = CreateSolidBrush(PLATO_BG_HOVER);
    g_scr_dlg.hPenBorder = CreatePen(PS_SOLID, 1, PLATO_PLASMA_COLOR);
    g_scr_dlg.hPenHighlight = CreatePen(PS_SOLID, 1, PLATO_PLASMA_BRIGHT);

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = ScriptsDlgProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_scr_dlg.hbrBg;
    wc.lpszClassName = L"PlatoScriptsDlgClassEx";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        wc.lpszClassName, L"Manage Scripts - PlatoLives",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 1060, 700,
        hParent, NULL, wc.hInstance, NULL
    );
    if (!hwnd) return;
    g_scr_dlg.hwnd = hwnd;

    HMODULE hDwm = LoadLibraryA("dwmapi.dll");
    if (hDwm) {
        typedef HRESULT (WINAPI *fnDwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
        fnDwmSetWindowAttribute pDwm = (fnDwmSetWindowAttribute)GetProcAddress(hDwm, "DwmSetWindowAttribute");
        if (pDwm) {
            BOOL dark_mode = TRUE;
            pDwm(hwnd, 20, &dark_mode, sizeof(dark_mode));
            pDwm(hwnd, 19, &dark_mode, sizeof(dark_mode));
            COLORREF caption_bg = PLATO_BG_DARK;
            COLORREF caption_fg = PLATO_PLASMA_COLOR;
            pDwm(hwnd, 35, &caption_bg, sizeof(caption_bg));
            pDwm(hwnd, 36, &caption_fg, sizeof(caption_fg));
        }
        FreeLibrary(hDwm);
    }

    g_scr_dlg.hFontUI = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    g_scr_dlg.hFontMono = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                      FIXED_PITCH | FF_MODERN, L"Consolas");

    CreateWindowW(L"STATIC", L"Scripts:", WS_CHILD | WS_VISIBLE, 16, 12, 180, 18, hwnd, NULL, wc.hInstance, NULL);
    g_scr_dlg.hList = CreateWindowW(L"LISTBOX", L"",
                                    WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
                                    16, 32, 200, 500, hwnd, (HMENU)IDC_SCR_LIST, wc.hInstance, NULL);
    SendMessage(g_scr_dlg.hList, LB_SETITEMHEIGHT, 0, 22);

    g_scr_dlg.hBtnNew = CreateWindowW(L"BUTTON", L"+ New", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 16, 542, 60, 26, hwnd, (HMENU)IDC_SCR_BTN_NEW, wc.hInstance, NULL);
    g_scr_dlg.hBtnClone = CreateWindowW(L"BUTTON", L"Clone", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 82, 542, 60, 26, hwnd, (HMENU)IDC_SCR_BTN_CLONE, wc.hInstance, NULL);
    g_scr_dlg.hBtnDelete = CreateWindowW(L"BUTTON", L"Delete", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 148, 542, 68, 26, hwnd, (HMENU)IDC_SCR_BTN_DELETE, wc.hInstance, NULL);

    int rx = 236;
    CreateWindowW(L"STATIC", L"Script Name:", WS_CHILD | WS_VISIBLE, rx, 12, 100, 18, hwnd, NULL, wc.hInstance, NULL);
    g_scr_dlg.hEditName = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, rx, 32, 330, 22, hwnd, (HMENU)IDC_SCR_EDIT_NAME, wc.hInstance, NULL);
    g_scr_dlg.hChkEnabled = CreateWindowW(L"BUTTON", L"Enabled", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, rx + 375, 32, 110, 22, hwnd, (HMENU)IDC_SCR_CHK_ENABLED, wc.hInstance, NULL);

    CreateWindowW(L"STATIC", L"Char Delay (ms):", WS_CHILD | WS_VISIBLE, rx, 64, 110, 18, hwnd, NULL, wc.hInstance, NULL);
    g_scr_dlg.hEditCharDel = CreateWindowW(L"EDIT", L"250", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, rx, 82, 110, 22, hwnd, (HMENU)IDC_SCR_EDIT_CHAR_DEL, wc.hInstance, NULL);

    CreateWindowW(L"STATIC", L"NEXT Delay (ms):", WS_CHILD | WS_VISIBLE, rx + 125, 64, 110, 18, hwnd, NULL, wc.hInstance, NULL);
    g_scr_dlg.hEditNextDel = CreateWindowW(L"EDIT", L"500", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, rx + 125, 82, 110, 22, hwnd, (HMENU)IDC_SCR_EDIT_NEXT_DEL, wc.hInstance, NULL);

    CreateWindowW(L"STATIC", L"Cmd Delay (ms):", WS_CHILD | WS_VISIBLE, rx + 250, 64, 110, 18, hwnd, NULL, wc.hInstance, NULL);
    g_scr_dlg.hEditCmdDel = CreateWindowW(L"EDIT", L"0", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, rx + 250, 82, 110, 22, hwnd, (HMENU)IDC_SCR_EDIT_CMD_DEL, wc.hInstance, NULL);

    g_scr_dlg.hLblHotkey = CreateWindowW(L"STATIC", L"Key: None", WS_CHILD | WS_VISIBLE, rx, 118, 220, 22, hwnd, (HMENU)IDC_SCR_LBL_HOTKEY, wc.hInstance, NULL);
    g_scr_dlg.hBtnRecord = CreateWindowW(L"BUTTON", L"Record Key", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, rx + 230, 114, 110, 26, hwnd, (HMENU)IDC_SCR_BTN_RECORD, wc.hInstance, NULL);
    g_scr_dlg.hBtnClearKey = CreateWindowW(L"BUTTON", L"Clear Key", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, rx + 350, 114, 90, 26, hwnd, (HMENU)IDC_SCR_BTN_CLEAR_KEY, wc.hInstance, NULL);

    CreateWindowW(L"STATIC", L"Script Code:", WS_CHILD | WS_VISIBLE, rx, 150, 200, 18, hwnd, NULL, wc.hInstance, NULL);
    g_scr_dlg.hEditBody = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | WS_VSCROLL,
                                        rx, 170, 785, 410, hwnd, (HMENU)IDC_SCR_EDIT_BODY, wc.hInstance, NULL);
    g_old_edit_box_proc = (WNDPROC)SetWindowLongPtrW(g_scr_dlg.hEditBody, GWLP_WNDPROC, (LONG_PTR)EditBoxSubclassProc);
    g_scr_dlg.hBtnSave = CreateWindowW(L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, rx + 565, 600, 100, 32, hwnd, (HMENU)IDC_SCR_BTN_SAVE, wc.hInstance, NULL);
    g_scr_dlg.hBtnClose = CreateWindowW(L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, rx + 675, 600, 105, 32, hwnd, (HMENU)IDC_SCR_BTN_CLOSE, wc.hInstance, NULL);

    HWND hChild = GetWindow(hwnd, GW_CHILD);
    while (hChild) {
        if (hChild == g_scr_dlg.hEditBody && g_scr_dlg.hFontMono) {
            SendMessage(hChild, WM_SETFONT, (WPARAM)g_scr_dlg.hFontMono, TRUE);
        } else {
            SendMessage(hChild, WM_SETFONT, (WPARAM)g_scr_dlg.hFontUI, TRUE);
        }
        hChild = GetWindow(hChild, GW_HWNDNEXT);
    }

    scr_refresh_list(&g_scr_dlg);

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
