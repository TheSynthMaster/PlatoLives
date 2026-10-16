#define WIN32_LEAN_AND_MEAN
#include "win_keymap.h"
#include "win_menu.h"
#include "plato/plato_keyboard.h"
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLATO_PLASMA_COLOR     RGB(255, 110, 0)
#define PLATO_PLASMA_BRIGHT    RGB(255, 160, 40)
#define PLATO_PLASMA_DIM       RGB(140, 70, 20)
#define PLATO_BG_DARK          RGB(16, 12, 10)
#define PLATO_BG_BOX           RGB(24, 18, 14)
#define PLATO_BG_HOVER         RGB(60, 28, 10)

#define IDC_K_LIST_PROF   801
#define IDC_K_LIST_BIND   802
#define IDC_K_BTN_NEW     803
#define IDC_K_BTN_DUP     804
#define IDC_K_BTN_REN     805
#define IDC_K_BTN_DEL     806
#define IDC_K_BTN_REC     807
#define IDC_K_BTN_REM     808
#define IDC_K_BTN_EDIT    814
#define IDC_K_BTN_SET     809
#define IDC_K_BTN_CLOSE   810
#define IDC_K_LBL_TITLE   811
#define IDC_K_LBL_DESC    812
#define IDC_K_LBL_STAT    813

#define IDC_ACT_RADIO_CHAR  901
#define IDC_ACT_RADIO_PLATO 902
#define IDC_ACT_EDIT_CHAR   903
#define IDC_ACT_COMBO_PLATO 904
#define IDC_ACT_BTN_ASSIGN  905
#define IDC_ACT_BTN_CANCEL  906
#define IDC_ACT_EDIT_DESC   907

typedef struct {
    HWND hwnd;
    HWND hParent;
    HMENU hMenu;
    plato_keyboard_state_t *kstate;
    plato_keymap_manager_t *mgr;

    HWND hListProf, hListBind;
    HWND hBtnNew, hBtnDup, hBtnRen, hBtnDel;
    HWND hBtnRec, hBtnEdit, hBtnRem, hBtnSet, hBtnClose;
    HWND hLblTitle, hLblDesc, hLblStat;

    HBRUSH hbrBg, hbrBox, hbrHover;
    HPEN hPenBorder, hPenHighlight;
    HFONT hFontUI, hFontMono, hFontBold;

    int cur_prof;
    int cur_bind;
    bool is_recording;
} k_dlg_ctx_t;

static k_dlg_ctx_t g_kdlg;

/* Forward declaration */
extern void win_update_keyboard_menu(HMENU menu, plato_keyboard_state_t *state);

static void format_key_str(const plato_key_binding_t *b, char *out, size_t max_len) {
    char mods[64] = {0};
    if (b->modifiers & PLATO_KEY_MOD_CTRL)  strcat(mods, "Ctrl+");
    if (b->modifiers & PLATO_KEY_MOD_ALT)   strcat(mods, "Alt+");
    if (b->modifiers & PLATO_KEY_MOD_SHIFT) strcat(mods, "Shift+");
    if (b->modifiers & PLATO_KEY_MOD_GUI)   strcat(mods, "Win+");

    const char *k = "";
    char single[16] = {0};
    if (b->vkey != 0) {
        switch (b->vkey) {
            case PLATO_VKEY_UP:        k = "Up Arrow"; break;
            case PLATO_VKEY_DOWN:      k = "Down Arrow"; break;
            case PLATO_VKEY_LEFT:      k = "Left Arrow"; break;
            case PLATO_VKEY_RIGHT:     k = "Right Arrow"; break;
            case PLATO_VKEY_RETURN:    k = "Return / Enter"; break;
            case PLATO_VKEY_BACKSPACE: k = "Backspace"; break;
            case PLATO_VKEY_ESCAPE:    k = "Escape"; break;
            case PLATO_VKEY_TAB:       k = "Tab"; break;
            case PLATO_VKEY_PAGEUP:    k = "Page Up"; break;
            case PLATO_VKEY_PAGEDOWN:  k = "Page Down"; break;
            case PLATO_VKEY_HOME:      k = "Home"; break;
            case PLATO_VKEY_END:       k = "End"; break;
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
            default:
                if (b->vkey >= PLATO_VKEY_F1 && b->vkey <= PLATO_VKEY_F12) {
                    snprintf(single, sizeof(single), "F%u", b->vkey - PLATO_VKEY_F1 + 1);
                    k = single;
                } else {
                    snprintf(single, sizeof(single), "VKey %u", b->vkey);
                    k = single;
                }
                break;
        }
    } else if (b->codepoint != 0) {
        if (b->codepoint == ' ') k = "Space";
        else { snprintf(single, sizeof(single), "'%c'", (char)b->codepoint); k = single; }
    } else {
        k = "(None)";
    }
    snprintf(out, max_len, "%s%s", mods, k);
}

static void format_act_str(const plato_key_binding_t *b, char *out, size_t max_len) {
    if (b->type == PLATO_BIND_ACTION_RAW_BYTE) {
        char ch = (char)b->target.raw_byte;
        if (ch >= 32 && ch <= 126) snprintf(out, max_len, "Type '%c'", ch);
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

static const char *s_plato_names[] = {
    "NEXT", "BACK", "HELP", "LAB", "DATA", "STOP", "EDIT", "COPY",
    "MICRO", "FONT", "SUPER (Up)", "SUB (Down)", "ERASE (Left)", "ACCESS / TAB", "ANS", "TERM", "SQUARE"
};

static const plato_special_key_t s_plato_keys[] = {
    PLATO_KEY_NEXT, PLATO_KEY_BACK, PLATO_KEY_HELP, PLATO_KEY_LAB,
    PLATO_KEY_DATA, PLATO_KEY_STOP, PLATO_KEY_EDIT, PLATO_KEY_COPY,
    PLATO_KEY_MICRO, PLATO_KEY_FONT, PLATO_KEY_SUPER, PLATO_KEY_SUB,
    PLATO_KEY_ERASE, PLATO_KEY_ACCESS, PLATO_KEY_ANS, PLATO_KEY_TERM, PLATO_KEY_SQUARE
};

/* ========================================================================= */
/* UNIVERSAL ACTION PICKER                                                   */
/* ========================================================================= */
typedef struct {
    HWND hwnd;
    HWND hRadioChar, hRadioPlato;
    HWND hEditChar, hComboPlato;
    HWND hEditLabel;
    HWND hBtnAssign, hBtnCancel;
    HBRUSH hbrBg, hbrBox;
    HPEN hPenBorder, hPenHighlight;
    HFONT hFontUI;

    uint16_t captured_vk;
    uint32_t captured_cp;
    uint8_t  captured_mods;
    bool confirmed;
    plato_key_binding_t result_binding;
} act_picker_ctx_t;

static act_picker_ctx_t g_picker;

static LRESULT CALLBACK ActionPickerProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    act_picker_ctx_t *ctx = &g_picker;
    switch (msg) {
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rc; GetClientRect(hwnd, &rc);
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
        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, PLATO_BG_BOX);
            SetTextColor(hdc, RGB(255, 255, 255));
            return (LRESULT)ctx->hbrBox;
        }
        case WM_DRAWITEM: {
            DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
            if (!dis || dis->CtlType != ODT_BUTTON) break;
            bool is_pressed = (dis->itemState & ODS_SELECTED);
            FillRect(dis->hDC, &dis->rcItem, is_pressed ? g_kdlg.hbrHover : ctx->hbrBox);

            HPEN pen = is_pressed ? ctx->hPenHighlight : ctx->hPenBorder;
            HPEN old_pen = (HPEN)SelectObject(dis->hDC, pen);
            HBRUSH old_brush = (HBRUSH)SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
            Rectangle(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom);
            SelectObject(dis->hDC, old_pen);
            SelectObject(dis->hDC, old_brush);

            wchar_t text[64] = {0};
            GetWindowTextW(dis->hwndItem, text, 63);
            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, is_pressed ? RGB(255, 255, 255) : PLATO_PLASMA_BRIGHT);
            DrawTextW(dis->hDC, text, -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == IDC_ACT_RADIO_CHAR || wmId == IDC_ACT_RADIO_PLATO) {
                bool is_char = (SendMessage(ctx->hRadioChar, BM_GETCHECK, 0, 0) == BST_CHECKED);
                EnableWindow(ctx->hEditChar, is_char);
                EnableWindow(ctx->hComboPlato, !is_char);
            } else if (wmId == IDC_ACT_BTN_CANCEL) {
                ctx->confirmed = false;
                DestroyWindow(hwnd);
            } else if (wmId == IDC_ACT_BTN_ASSIGN) {
                bool is_char = (SendMessage(ctx->hRadioChar, BM_GETCHECK, 0, 0) == BST_CHECKED);
                memset(&ctx->result_binding, 0, sizeof(ctx->result_binding));
                ctx->result_binding.vkey = ctx->captured_vk;
                ctx->result_binding.codepoint = ctx->captured_cp;
                ctx->result_binding.modifiers = ctx->captured_mods;

                char user_desc[32] = {0};
                GetWindowTextA(ctx->hEditLabel, user_desc, sizeof(user_desc) - 1);

                if (is_char) {
                    char buf[8] = {0};
                    GetWindowTextA(ctx->hEditChar, buf, 7);
                    char c = buf[0] ? buf[0] : ' ';
                    ctx->result_binding.type = PLATO_BIND_ACTION_RAW_BYTE;
                    ctx->result_binding.target.raw_byte = (uint8_t)c;
                    if (strlen(user_desc) > 0) {
                        snprintf(ctx->result_binding.label, sizeof(ctx->result_binding.label), "%s", user_desc);
                    } else {
                        snprintf(ctx->result_binding.label, sizeof(ctx->result_binding.label), "Type '%c'", c);
                    }
                } else {
                    int sel = (int)SendMessage(ctx->hComboPlato, CB_GETCURSEL, 0, 0);
                    if (sel < 0 || sel >= 17) sel = 0;
                    ctx->result_binding.type = PLATO_BIND_ACTION_PLATO_KEY;
                    ctx->result_binding.target.plato_key = (uint16_t)s_plato_keys[sel];
                    if (strlen(user_desc) > 0) {
                        snprintf(ctx->result_binding.label, sizeof(ctx->result_binding.label), "%s", user_desc);
                    } else {
                        snprintf(ctx->result_binding.label, sizeof(ctx->result_binding.label), "PLATO: %s", s_plato_names[sel]);
                    }
                }
                ctx->confirmed = true;
                DestroyWindow(hwnd);
            }
            return 0;
        }
        case WM_DESTROY: {
            DeleteObject(ctx->hbrBg);
            DeleteObject(ctx->hbrBox);
            DeleteObject(ctx->hPenBorder);
            DeleteObject(ctx->hPenHighlight);
            DeleteObject(ctx->hFontUI);
            break;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static bool win_action_picker_show(HWND hParent, const plato_key_binding_t *init_bind, plato_key_binding_t *out_bind) {
    memset(&g_picker, 0, sizeof(g_picker));
    if (!init_bind) return false;

    g_picker.captured_vk = init_bind->vkey;
    g_picker.captured_cp = init_bind->codepoint;
    g_picker.captured_mods = init_bind->modifiers;
    g_picker.confirmed = false;

    g_picker.hbrBg = CreateSolidBrush(PLATO_BG_DARK);
    g_picker.hbrBox = CreateSolidBrush(PLATO_BG_BOX);
    g_picker.hPenBorder = CreatePen(PS_SOLID, 1, PLATO_PLASMA_COLOR);
    g_picker.hPenHighlight = CreatePen(PS_SOLID, 1, PLATO_PLASMA_BRIGHT);

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ActionPickerProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_picker.hbrBg;
    wc.lpszClassName = L"PlatoActPickerClass";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        wc.lpszClassName, (init_bind->type != 0) ? L"Edit Target Action - PlatoLives" : L"Assign Target Action - PlatoLives",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 460, 290,
        hParent, NULL, wc.hInstance, NULL
    );
    if (!hwnd) return false;
    g_picker.hwnd = hwnd;

    g_picker.hFontUI = CreateFontW(15,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");

    char kstr[64]; format_key_str(init_bind, kstr, sizeof(kstr));
    wchar_t cap_w[128]; swprintf(cap_w, 127, L"Physical Key: %hs", kstr);

    CreateWindowW(L"STATIC", cap_w, WS_CHILD | WS_VISIBLE, 20, 15, 410, 20, hwnd, NULL, wc.hInstance, NULL);

    g_picker.hRadioChar = CreateWindowW(L"BUTTON", L"Type Character / Symbol (e.g. f, +, ;, @):", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 20, 44, 320, 20, hwnd, (HMENU)IDC_ACT_RADIO_CHAR, wc.hInstance, NULL);
    g_picker.hEditChar = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER, 350, 42, 45, 24, hwnd, (HMENU)IDC_ACT_EDIT_CHAR, wc.hInstance, NULL);

    g_picker.hRadioPlato = CreateWindowW(L"BUTTON", L"Or select PLATO Special Key:", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 20, 78, 380, 20, hwnd, (HMENU)IDC_ACT_RADIO_PLATO, wc.hInstance, NULL);
    g_picker.hComboPlato = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 40, 102, 355, 200, hwnd, (HMENU)IDC_ACT_COMBO_PLATO, wc.hInstance, NULL);

    for (int i = 0; i < 17; i++) {
        WCHAR wn[64]; swprintf(wn, 63, L"%hs", s_plato_names[i]);
        SendMessageW(g_picker.hComboPlato, CB_ADDSTRING, 0, (LPARAM)wn);
    }

    CreateWindowW(L"STATIC", L"Description / Label (Optional):", WS_CHILD | WS_VISIBLE, 20, 138, 380, 18, hwnd, NULL, wc.hInstance, NULL);
    g_picker.hEditLabel = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 20, 158, 395, 24, hwnd, (HMENU)IDC_ACT_EDIT_DESC, wc.hInstance, NULL);

    bool is_char_mode = false;
    if (init_bind->type == PLATO_BIND_ACTION_RAW_BYTE) {
        is_char_mode = true;
        char def_s[2] = { (char)init_bind->target.raw_byte, 0 };
        SetWindowTextA(g_picker.hEditChar, def_s);
        SetWindowTextA(g_picker.hEditLabel, init_bind->label);
        SendMessage(g_picker.hComboPlato, CB_SETCURSEL, 0, 0);
    } else if (init_bind->type == PLATO_BIND_ACTION_PLATO_KEY) {
        is_char_mode = false;
        int sel_idx = 0;
        for (int i = 0; i < 17; i++) {
            if (s_plato_keys[i] == init_bind->target.plato_key) { sel_idx = i; break; }
        }
        SendMessage(g_picker.hComboPlato, CB_SETCURSEL, sel_idx, 0);
        SetWindowTextA(g_picker.hEditLabel, init_bind->label);
    } else {
        is_char_mode = (init_bind->vkey == 0 || (init_bind->vkey >= PLATO_VKEY_NUMPAD0 && init_bind->vkey <= PLATO_VKEY_NUMPAD9));
        SendMessage(g_picker.hComboPlato, CB_SETCURSEL, 0, 0);
        if (init_bind->codepoint >= 32 && init_bind->codepoint <= 126) {
            char def_s[2] = { (char)init_bind->codepoint, 0 };
            SetWindowTextA(g_picker.hEditChar, def_s);
        }
    }

    SendMessage(g_picker.hRadioChar, BM_SETCHECK, is_char_mode ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(g_picker.hRadioPlato, BM_SETCHECK, is_char_mode ? BST_UNCHECKED : BST_CHECKED, 0);
    EnableWindow(g_picker.hEditChar, is_char_mode);
    EnableWindow(g_picker.hComboPlato, !is_char_mode);

    g_picker.hBtnAssign = CreateWindowW(L"BUTTON", (init_bind->type != 0) ? L"Save" : L"Assign", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 230, 205, 95, 30, hwnd, (HMENU)IDC_ACT_BTN_ASSIGN, wc.hInstance, NULL);
    g_picker.hBtnCancel = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 335, 205, 80, 30, hwnd, (HMENU)IDC_ACT_BTN_CANCEL, wc.hInstance, NULL);

    HWND hChild = GetWindow(hwnd, GW_CHILD);
    while (hChild) {
        SendMessage(hChild, WM_SETFONT, (WPARAM)g_picker.hFontUI, TRUE);
        hChild = GetWindow(hChild, GW_HWNDNEXT);
    }

    RECT pr, dr;
    GetWindowRect(hParent, &pr);
    GetWindowRect(hwnd, &dr);
    int cx = pr.left + ((pr.right - pr.left) - (dr.right - dr.left)) / 2;
    int cy = pr.top + ((pr.bottom - pr.top) - (dr.bottom - dr.top)) / 2;
    SetWindowPos(hwnd, HWND_TOP, cx, cy, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);

    EnableWindow(hParent, FALSE);
    MSG msg;
    while (IsWindow(hwnd) && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);

    if (g_picker.confirmed) {
        *out_bind = g_picker.result_binding;
        return true;
    }
    return false;
}

/* ========================================================================= */
/* RENAME MODAL DIALOG                                                       */
/* ========================================================================= */
typedef struct {
    HWND hwnd;
    HWND hEdit;
    HWND hEditDesc;
    HBRUSH hbrBg, hbrBox;
    HPEN hPenBorder, hPenHighlight;
    HFONT hFontUI;
    bool confirmed;
    char result_name[64];
    char result_desc[128];
} ren_dlg_ctx_t;

static ren_dlg_ctx_t g_ren;

static LRESULT CALLBACK RenameProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ren_dlg_ctx_t *ctx = &g_ren;
    switch (msg) {
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rc; GetClientRect(hwnd, &rc);
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
        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, PLATO_BG_BOX);
            SetTextColor(hdc, RGB(255, 255, 255));
            return (LRESULT)ctx->hbrBox;
        }
        case WM_DRAWITEM: {
            DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
            if (!dis || dis->CtlType != ODT_BUTTON) break;
            bool is_pressed = (dis->itemState & ODS_SELECTED);
            FillRect(dis->hDC, &dis->rcItem, is_pressed ? g_kdlg.hbrHover : ctx->hbrBox);
            HPEN pen = is_pressed ? ctx->hPenHighlight : ctx->hPenBorder;
            HPEN old_pen = (HPEN)SelectObject(dis->hDC, pen);
            HBRUSH old_brush = (HBRUSH)SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
            Rectangle(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom);
            SelectObject(dis->hDC, old_pen);
            SelectObject(dis->hDC, old_brush);

            wchar_t text[64] = {0};
            GetWindowTextW(dis->hwndItem, text, 63);
            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, is_pressed ? RGB(255, 255, 255) : PLATO_PLASMA_BRIGHT);
            DrawTextW(dis->hDC, text, -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == 1) { // OK / Save
                GetWindowTextA(ctx->hEdit, ctx->result_name, sizeof(ctx->result_name) - 1);
                GetWindowTextA(ctx->hEditDesc, ctx->result_desc, sizeof(ctx->result_desc) - 1);
                ctx->confirmed = (strlen(ctx->result_name) > 0);
                DestroyWindow(hwnd);
            } else if (wmId == 2) { // Cancel
                ctx->confirmed = false;
                DestroyWindow(hwnd);
            }
            return 0;
        }
        case WM_DESTROY: {
            DeleteObject(ctx->hbrBg);
            DeleteObject(ctx->hbrBox);
            DeleteObject(ctx->hPenBorder);
            DeleteObject(ctx->hPenHighlight);
            DeleteObject(ctx->hFontUI);
            break;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static bool win_rename_show(HWND hParent, const char *cur_name, const char *cur_desc, char *out_name, char *out_desc) {
    memset(&g_ren, 0, sizeof(g_ren));
    g_ren.hbrBg = CreateSolidBrush(PLATO_BG_DARK);
    g_ren.hbrBox = CreateSolidBrush(PLATO_BG_BOX);
    g_ren.hPenBorder = CreatePen(PS_SOLID, 1, PLATO_PLASMA_COLOR);
    g_ren.hPenHighlight = CreatePen(PS_SOLID, 1, PLATO_PLASMA_BRIGHT);

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = RenameProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_ren.hbrBg;
    wc.lpszClassName = L"PlatoRenameClass";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        wc.lpszClassName, L"Edit Profile - PlatoLives",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 380, 215,
        hParent, NULL, wc.hInstance, NULL
    );
    if (!hwnd) return false;
    g_ren.hwnd = hwnd;

    g_ren.hFontUI = CreateFontW(15,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");

    CreateWindowW(L"STATIC", L"Profile Name:", WS_CHILD | WS_VISIBLE, 20, 15, 330, 18, hwnd, NULL, wc.hInstance, NULL);
    g_ren.hEdit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 20, 35, 325, 24, hwnd, NULL, wc.hInstance, NULL);
    SetWindowTextA(g_ren.hEdit, cur_name ? cur_name : "");

    CreateWindowW(L"STATIC", L"Description:", WS_CHILD | WS_VISIBLE, 20, 68, 330, 18, hwnd, NULL, wc.hInstance, NULL);
    g_ren.hEditDesc = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 20, 88, 325, 24, hwnd, NULL, wc.hInstance, NULL);
    SetWindowTextA(g_ren.hEditDesc, cur_desc ? cur_desc : "");

    CreateWindowW(L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 150, 130, 85, 28, hwnd, (HMENU)1, wc.hInstance, NULL);
    CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 250, 130, 85, 28, hwnd, (HMENU)2, wc.hInstance, NULL);

    HWND hChild = GetWindow(hwnd, GW_CHILD);
    while (hChild) {
        SendMessage(hChild, WM_SETFONT, (WPARAM)g_ren.hFontUI, TRUE);
        hChild = GetWindow(hChild, GW_HWNDNEXT);
    }

    RECT pr, dr;
    GetWindowRect(hParent, &pr);
    GetWindowRect(hwnd, &dr);
    int cx = pr.left + ((pr.right - pr.left) - (dr.right - dr.left)) / 2;
    int cy = pr.top + ((pr.bottom - pr.top) - (dr.bottom - dr.top)) / 2;
    SetWindowPos(hwnd, HWND_TOP, cx, cy, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);

    EnableWindow(hParent, FALSE);
    MSG msg;
    while (IsWindow(hwnd) && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);

    if (g_ren.confirmed) {
        if (out_name) {
            strncpy(out_name, g_ren.result_name, 63);
            out_name[63] = '\0';
        }
        if (out_desc) {
            strncpy(out_desc, g_ren.result_desc, 127);
            out_desc[127] = '\0';
        }
        return true;
    }
    return false;
}

/* ========================================================================= */
/* MAIN DIALOG REFRESH & EVENTS                                              */
/* ========================================================================= */
static void k_refresh_prof(void) {
    SendMessage(g_kdlg.hListProf, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < g_kdlg.mgr->profile_count; i++) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s%s", g_kdlg.mgr->profiles[i].name, g_kdlg.mgr->profiles[i].is_builtin ? " (Built-in)" : "");
        SendMessageA(g_kdlg.hListProf, LB_ADDSTRING, 0, (LPARAM)buf);
    }
    SendMessage(g_kdlg.hListProf, LB_SETCURSEL, g_kdlg.cur_prof, 0);
}

static void k_refresh_bind(void) {
    SendMessage(g_kdlg.hListBind, LB_RESETCONTENT, 0, 0);
    if (g_kdlg.cur_prof < 0 || g_kdlg.cur_prof >= g_kdlg.mgr->profile_count) return;
    plato_keymap_profile_t *p = &g_kdlg.mgr->profiles[g_kdlg.cur_prof];

    char title[128];
    snprintf(title, sizeof(title), "%s %s", p->name, p->is_builtin ? "[Built-in / Read-Only]" : "[User Defined]");
    SetWindowTextA(g_kdlg.hLblTitle, title);
    SetWindowTextA(g_kdlg.hLblDesc, p->description);

    EnableWindow(g_kdlg.hBtnNew, TRUE);
    EnableWindow(g_kdlg.hBtnDup, TRUE);
    EnableWindow(g_kdlg.hBtnRen, !p->is_builtin);
    EnableWindow(g_kdlg.hBtnDel, !p->is_builtin);
    EnableWindow(g_kdlg.hBtnRec, !p->is_builtin);
    bool can_edit_bind = (!p->is_builtin && g_kdlg.cur_bind >= 0 && g_kdlg.cur_bind < p->binding_count);
    EnableWindow(g_kdlg.hBtnRem, can_edit_bind);
    EnableWindow(g_kdlg.hBtnEdit, can_edit_bind);

    bool isAct = (g_kdlg.cur_prof == g_kdlg.mgr->active_profile_index);
    /* Utilizzo di SetWindowTextW con Wide Character per eliminare i caratteri spuri */
    SetWindowTextW(g_kdlg.hBtnSet, isAct ? L"\x2713 Active Profile" : L"Set as Active Profile");
    EnableWindow(g_kdlg.hBtnSet, !isAct);

    if (p->is_builtin) {
        SetWindowTextA(g_kdlg.hLblStat, "Built-in profile is read-only. Click 'Duplicate' to customize.");
    } else {
        char cnt[64]; snprintf(cnt, sizeof(cnt), "%d custom key bindings configured.", p->binding_count);
        SetWindowTextA(g_kdlg.hLblStat, cnt);
    }

    for (int i = 0; i < p->binding_count; i++) {
        char kstr[64], astr[64], full[256];
        format_key_str(&p->bindings[i], kstr, sizeof(kstr));
        format_act_str(&p->bindings[i], astr, sizeof(astr));
        snprintf(full, sizeof(full), "%-22s  %-24s  %s", kstr, astr, p->bindings[i].label);
        SendMessageA(g_kdlg.hListBind, LB_ADDSTRING, 0, (LPARAM)full);
    }
}

static LRESULT CALLBACK KeymapDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    k_dlg_ctx_t *ctx = &g_kdlg;
    switch (msg) {
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rc; GetClientRect(hwnd, &rc);
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

            if (dis->CtlType == ODT_LISTBOX) {
                if ((int)dis->itemID < 0) return TRUE;
                bool is_selected = (dis->itemState & ODS_SELECTED);
                FillRect(dis->hDC, &dis->rcItem, is_selected ? ctx->hbrHover : ctx->hbrBox);

                if (is_selected) {
                    HPEN oldP = (HPEN)SelectObject(dis->hDC, ctx->hPenHighlight);
                    HBRUSH oldB = (HBRUSH)SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
                    Rectangle(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom);
                    SelectObject(dis->hDC, oldP);
                    SelectObject(dis->hDC, oldB);
                }

                char text[256] = {0};
                SendMessageA(dis->hwndItem, LB_GETTEXT, dis->itemID, (LPARAM)text);
                SetBkMode(dis->hDC, TRANSPARENT);
                SetTextColor(dis->hDC, is_selected ? RGB(255, 255, 255) : PLATO_PLASMA_BRIGHT);

                RECT rc = dis->rcItem;
                rc.left += 6;
                DrawTextA(dis->hDC, text, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                return TRUE;
            }

            if (dis->CtlType == ODT_BUTTON) {
                bool is_pressed = (dis->itemState & ODS_SELECTED);
                bool is_disabled = (dis->itemState & ODS_DISABLED);
                FillRect(dis->hDC, &dis->rcItem, is_pressed ? ctx->hbrHover : ctx->hbrBox);

                HPEN pen = is_disabled ? ctx->hPenBorder : (is_pressed ? ctx->hPenHighlight : ctx->hPenBorder);
                HPEN old_pen = (HPEN)SelectObject(dis->hDC, pen);
                HBRUSH old_brush = (HBRUSH)SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
                Rectangle(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom);
                SelectObject(dis->hDC, old_pen);
                SelectObject(dis->hDC, old_brush);

                wchar_t text[128] = {0};
                GetWindowTextW(dis->hwndItem, text, 127);
                SetBkMode(dis->hDC, TRANSPARENT);
                SetTextColor(dis->hDC, is_disabled ? PLATO_PLASMA_DIM : (is_pressed ? RGB(255, 255, 255) : PLATO_PLASMA_BRIGHT));
                DrawTextW(dis->hDC, text, -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                return TRUE;
            }
            break;
        }

        /* Intercettazione Pressione Tasti Fisici ("Press Key to Bind...") */
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            if (ctx->is_recording) {
                if (wParam == VK_ESCAPE) {
                    ctx->is_recording = false;
                    k_refresh_bind();
                    return 0;
                }
                if (wParam == VK_SHIFT || wParam == VK_CONTROL || wParam == VK_MENU) {
                    return 0;
                }

                uint8_t mods = 0;
                if (GetKeyState(VK_CONTROL) & 0x8000) mods |= PLATO_KEY_MOD_CTRL;
                if (GetKeyState(VK_MENU) & 0x8000)    mods |= PLATO_KEY_MOD_ALT;
                if (GetKeyState(VK_SHIFT) & 0x8000)   mods |= PLATO_KEY_MOD_SHIFT;
                if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000)) mods |= PLATO_KEY_MOD_GUI;

                uint16_t vk = 0;
                uint32_t cp = 0;

                switch (wParam) {
                    case VK_UP:        vk = PLATO_VKEY_UP; break;
                    case VK_DOWN:      vk = PLATO_VKEY_DOWN; break;
                    case VK_LEFT:      vk = PLATO_VKEY_LEFT; break;
                    case VK_RIGHT:     vk = PLATO_VKEY_RIGHT; break;
                    case VK_RETURN:    vk = PLATO_VKEY_RETURN; break;
                    case VK_TAB:       vk = PLATO_VKEY_TAB; break;
                    case VK_BACK:      vk = PLATO_VKEY_BACKSPACE; break;
                    case VK_PRIOR:     vk = PLATO_VKEY_PAGEUP; break;
                    case VK_NEXT:      vk = PLATO_VKEY_PAGEDOWN; break;
                    case VK_HOME:      vk = PLATO_VKEY_HOME; break;
                    case VK_END:       vk = PLATO_VKEY_END; break;
                    case VK_NUMPAD0:   vk = PLATO_VKEY_NUMPAD0; break;
                    case VK_NUMPAD1:   vk = PLATO_VKEY_NUMPAD1; break;
                    case VK_NUMPAD2:   vk = PLATO_VKEY_NUMPAD2; break;
                    case VK_NUMPAD3:   vk = PLATO_VKEY_NUMPAD3; break;
                    case VK_NUMPAD4:   vk = PLATO_VKEY_NUMPAD4; break;
                    case VK_NUMPAD5:   vk = PLATO_VKEY_NUMPAD5; break;
                    case VK_NUMPAD6:   vk = PLATO_VKEY_NUMPAD6; break;
                    case VK_NUMPAD7:   vk = PLATO_VKEY_NUMPAD7; break;
                    case VK_NUMPAD8:   vk = PLATO_VKEY_NUMPAD8; break;
                    case VK_NUMPAD9:   vk = PLATO_VKEY_NUMPAD9; break;
                    default:
                        if (wParam >= VK_F1 && wParam <= VK_F12) {
                            vk = PLATO_VKEY_F1 + (wParam - VK_F1);
                        } else {
                            BYTE kstate[256];
                            GetKeyboardState(kstate);
                            WORD chars = 0;
                            if (ToAscii((UINT)wParam, (UINT)(lParam >> 16) & 0xFF, kstate, &chars, 0) == 1) {
                                cp = (uint32_t)(chars & 0xFF);
                            } else if (wParam >= 32 && wParam <= 126) {
                                cp = (uint32_t)wParam;
                            }
                        }
                        break;
                }

                ctx->is_recording = false;

                plato_key_binding_t init_b = { .vkey = vk, .codepoint = cp, .modifiers = mods };
                plato_key_binding_t new_b;
                if (win_action_picker_show(hwnd, &init_b, &new_b)) {
                    plato_keymap_profile_t *p = &ctx->mgr->profiles[ctx->cur_prof];
                    plato_keymap_profile_add_binding(p, &new_b);
                    plato_keymap_save_profile(p);
                }
                k_refresh_bind();
                return 0;
            }
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == IDC_K_LIST_PROF && HIWORD(wParam) == LBN_SELCHANGE) {
                ctx->cur_prof = (int)SendMessage(ctx->hListProf, LB_GETCURSEL, 0, 0);
                k_refresh_bind();
            } else if (wmId == IDC_K_LIST_BIND) {
                if (HIWORD(wParam) == LBN_SELCHANGE) {
                    ctx->cur_bind = (int)SendMessage(ctx->hListBind, LB_GETCURSEL, 0, 0);
                    plato_keymap_profile_t *p = &ctx->mgr->profiles[ctx->cur_prof];
                    bool can_edit_bind = (!p->is_builtin && ctx->cur_bind >= 0 && ctx->cur_bind < p->binding_count);
                    EnableWindow(ctx->hBtnRem, can_edit_bind);
                    EnableWindow(ctx->hBtnEdit, can_edit_bind);
                } else if (HIWORD(wParam) == LBN_DBLCLK) {
                    ctx->cur_bind = (int)SendMessage(ctx->hListBind, LB_GETCURSEL, 0, 0);
                    if (ctx->cur_prof >= 0 && ctx->cur_bind >= 0 && !ctx->mgr->profiles[ctx->cur_prof].is_builtin) {
                        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_K_BTN_EDIT, BN_CLICKED), 0);
                    }
                }
            } else if (wmId == IDC_K_BTN_EDIT) {
                if (ctx->cur_prof >= 0 && ctx->cur_bind >= 0 && !ctx->mgr->profiles[ctx->cur_prof].is_builtin) {
                    plato_keymap_profile_t *p = &ctx->mgr->profiles[ctx->cur_prof];
                    if (ctx->cur_bind < p->binding_count) {
                        plato_key_binding_t updated_b;
                        if (win_action_picker_show(hwnd, &p->bindings[ctx->cur_bind], &updated_b)) {
                            plato_keymap_profile_set_binding(p, ctx->cur_bind, &updated_b);
                            plato_keymap_save_profile(p);
                            k_refresh_bind();
                        }
                    }
                }
            } else if (wmId == IDC_K_BTN_CLOSE) {
                DestroyWindow(hwnd);
            } else if (wmId == IDC_K_BTN_SET) {
                /* Set Active sincrono sia sul manager che sullo stato di tastiera */
                plato_keyboard_set_active_keymap_by_index(ctx->kstate, ctx->cur_prof);
                plato_keymap_save_all(ctx->mgr);
                k_refresh_prof();
                k_refresh_bind();
                if (ctx->hMenu) {
                    win_update_keyboard_menu(ctx->hMenu, ctx->kstate);
                    DrawMenuBar(ctx->hParent);
                }
            } else if (wmId == IDC_K_BTN_DUP) {
                if (ctx->cur_prof >= 0) {
                    char nn[64]; snprintf(nn, sizeof(nn), "%s Copy", ctx->mgr->profiles[ctx->cur_prof].name);
                    if (plato_keymap_duplicate_profile(ctx->mgr, ctx->cur_prof, nn)) {
                        ctx->cur_prof = ctx->mgr->profile_count - 1;
                        plato_keymap_save_profile(&ctx->mgr->profiles[ctx->cur_prof]);
                        k_refresh_prof(); k_refresh_bind();
                        if (ctx->hMenu) {
                            win_update_keyboard_menu(ctx->hMenu, ctx->kstate);
                            DrawMenuBar(ctx->hParent);
                        }
                    }
                }
            } else if (wmId == IDC_K_BTN_NEW) {
                plato_keymap_profile_t p = {0};
                snprintf(p.name, sizeof(p.name), "Custom Profile %d", ctx->mgr->profile_count);
                snprintf(p.description, sizeof(p.description), "User-defined key mappings");
                int idx = plato_keymap_add_profile(ctx->mgr, &p);
                if (idx >= 0) {
                    ctx->cur_prof = idx;
                    plato_keymap_save_profile(&ctx->mgr->profiles[idx]);
                    k_refresh_prof(); k_refresh_bind();
                    if (ctx->hMenu) {
                        win_update_keyboard_menu(ctx->hMenu, ctx->kstate);
                        DrawMenuBar(ctx->hParent);
                    }
                }
            } else if (wmId == IDC_K_BTN_REN) {
                if (ctx->cur_prof >= 0 && !ctx->mgr->profiles[ctx->cur_prof].is_builtin) {
                    char nname[64] = {0};
                    char ndesc[128] = {0};
                    if (win_rename_show(hwnd, ctx->mgr->profiles[ctx->cur_prof].name, ctx->mgr->profiles[ctx->cur_prof].description, nname, ndesc)) {
                        plato_keymap_update_profile_info(ctx->mgr, ctx->cur_prof, nname, ndesc);
                        k_refresh_prof(); k_refresh_bind();
                        if (ctx->hMenu) {
                            win_update_keyboard_menu(ctx->hMenu, ctx->kstate);
                            DrawMenuBar(ctx->hParent);
                        }
                    }
                }
            } else if (wmId == IDC_K_BTN_DEL) {
                if (ctx->cur_prof >= 0 && !ctx->mgr->profiles[ctx->cur_prof].is_builtin) {
                    if (MessageBoxW(hwnd, L"Are you sure you want to delete this profile?", L"PlatoLives", MB_YESNO | MB_ICONWARNING) == IDYES) {
                        plato_keymap_delete_profile(ctx->mgr, ctx->cur_prof);
                        ctx->cur_prof = ctx->mgr->active_profile_index;
                        k_refresh_prof(); k_refresh_bind();
                        if (ctx->hMenu) {
                            win_update_keyboard_menu(ctx->hMenu, ctx->kstate);
                            DrawMenuBar(ctx->hParent);
                        }
                    }
                }
            } else if (wmId == IDC_K_BTN_REM) {
                if (ctx->cur_prof >= 0 && ctx->cur_bind >= 0 && !ctx->mgr->profiles[ctx->cur_prof].is_builtin) {
                    plato_keymap_profile_remove_binding(&ctx->mgr->profiles[ctx->cur_prof], ctx->cur_bind);
                    plato_keymap_save_profile(&ctx->mgr->profiles[ctx->cur_prof]);
                    ctx->cur_bind = -1;
                    k_refresh_bind();
                }
            } else if (wmId == IDC_K_BTN_REC) {
                if (ctx->cur_prof >= 0 && !ctx->mgr->profiles[ctx->cur_prof].is_builtin) {
                    ctx->is_recording = true;
                    SetWindowTextW(ctx->hLblStat, L"\x25C9 PRESS ANY KEY OR COMBINATION (Esc to Cancel)...");
                    SetFocus(hwnd);
                }
            }
            return 0;
        }

        case WM_DESTROY: {
            if (ctx->hMenu && ctx->kstate) {
                win_update_keyboard_menu(ctx->hMenu, ctx->kstate);
                DrawMenuBar(ctx->hParent);
            }
            DeleteObject(ctx->hbrBg);
            DeleteObject(ctx->hbrBox);
            DeleteObject(ctx->hbrHover);
            DeleteObject(ctx->hPenBorder);
            DeleteObject(ctx->hPenHighlight);
            DeleteObject(ctx->hFontUI);
            DeleteObject(ctx->hFontMono);
            DeleteObject(ctx->hFontBold);
            break;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void win_keymap_show(HWND hParent, plato_keyboard_state_t *kstate, HMENU hMenu) {
    if (!kstate) return;
    if (IsWindow(g_kdlg.hwnd)) { SetForegroundWindow(g_kdlg.hwnd); return; }

    g_kdlg.hParent = hParent;
    g_kdlg.hMenu = hMenu;
    g_kdlg.kstate = kstate;
    g_kdlg.mgr = &kstate->keymap_mgr;
    g_kdlg.cur_prof = g_kdlg.mgr->active_profile_index;
    g_kdlg.cur_bind = -1;
    g_kdlg.is_recording = false;

    g_kdlg.hbrBg = CreateSolidBrush(PLATO_BG_DARK);
    g_kdlg.hbrBox = CreateSolidBrush(PLATO_BG_BOX);
    g_kdlg.hbrHover = CreateSolidBrush(PLATO_BG_HOVER);
    g_kdlg.hPenBorder = CreatePen(PS_SOLID, 1, PLATO_PLASMA_COLOR);
    g_kdlg.hPenHighlight = CreatePen(PS_SOLID, 1, PLATO_PLASMA_BRIGHT);

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = KeymapDlgProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_kdlg.hbrBg;
    wc.lpszClassName = L"PlatoKeymapClass";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        wc.lpszClassName, L"Manage Keyboard Profiles - PlatoLives",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 960, 640,
        hParent, NULL, wc.hInstance, NULL
    );
    if (!hwnd) return;
    g_kdlg.hwnd = hwnd;

    /* DWM Dark Caption */
    HMODULE hDwm = LoadLibraryA("dwmapi.dll");
    if (hDwm) {
        typedef HRESULT (WINAPI *fnDwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
        fnDwmSetWindowAttribute pDwm = (fnDwmSetWindowAttribute)GetProcAddress(hDwm, "DwmSetWindowAttribute");
        if (pDwm) {
            BOOL dark_mode = TRUE; COLORREF cbg = PLATO_BG_DARK, cfg = PLATO_PLASMA_COLOR;
            pDwm(hwnd, 20, &dark_mode, sizeof(dark_mode));
            pDwm(hwnd, 19, &dark_mode, sizeof(dark_mode));
            pDwm(hwnd, 35, &cbg, sizeof(cbg));
            pDwm(hwnd, 36, &cfg, sizeof(cfg));
        }
        FreeLibrary(hDwm);
    }

    g_kdlg.hFontUI = CreateFontW(15,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    g_kdlg.hFontBold = CreateFontW(16,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    g_kdlg.hFontMono = CreateFontW(14,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,FIXED_PITCH,L"Consolas");

    CreateWindowW(L"STATIC", L"PROFILES", WS_CHILD | WS_VISIBLE, 20, 15, 200, 20, hwnd, NULL, wc.hInstance, NULL);
    g_kdlg.hListProf = CreateWindowExW(0, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS, 20, 40, 240, 490, hwnd, (HMENU)IDC_K_LIST_PROF, wc.hInstance, NULL);

    g_kdlg.hBtnNew = CreateWindowW(L"BUTTON", L"New", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 20, 540, 55, 28, hwnd, (HMENU)IDC_K_BTN_NEW, wc.hInstance, NULL);
    g_kdlg.hBtnDup = CreateWindowW(L"BUTTON", L"Duplicate", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 80, 540, 75, 28, hwnd, (HMENU)IDC_K_BTN_DUP, wc.hInstance, NULL);
    g_kdlg.hBtnRen = CreateWindowW(L"BUTTON", L"Rename", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 160, 540, 60, 28, hwnd, (HMENU)IDC_K_BTN_REN, wc.hInstance, NULL);
    g_kdlg.hBtnDel = CreateWindowW(L"BUTTON", L"Del", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 225, 540, 35, 28, hwnd, (HMENU)IDC_K_BTN_DEL, wc.hInstance, NULL);

    CreateWindowW(L"STATIC", L"MAPPINGS & SHORTCUTS", WS_CHILD | WS_VISIBLE, 280, 15, 250, 20, hwnd, NULL, wc.hInstance, NULL);
    g_kdlg.hLblTitle = CreateWindowW(L"STATIC", L"Profile", WS_CHILD | WS_VISIBLE, 280, 35, 640, 22, hwnd, (HMENU)IDC_K_LBL_TITLE, wc.hInstance, NULL);
    g_kdlg.hLblDesc = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 280, 57, 640, 20, hwnd, (HMENU)IDC_K_LBL_DESC, wc.hInstance, NULL);

    g_kdlg.hListBind = CreateWindowExW(0, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS, 280, 80, 640, 410, hwnd, (HMENU)IDC_K_LIST_BIND, wc.hInstance, NULL);

    g_kdlg.hBtnRec = CreateWindowW(L"BUTTON", L"◉ Press Key to Bind...", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 280, 500, 175, 30, hwnd, (HMENU)IDC_K_BTN_REC, wc.hInstance, NULL);
    g_kdlg.hBtnEdit = CreateWindowW(L"BUTTON", L"✎ Edit Binding...", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 465, 500, 130, 30, hwnd, (HMENU)IDC_K_BTN_EDIT, wc.hInstance, NULL);
    g_kdlg.hBtnRem = CreateWindowW(L"BUTTON", L"- Remove Binding", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 605, 500, 140, 30, hwnd, (HMENU)IDC_K_BTN_REM, wc.hInstance, NULL);

    g_kdlg.hLblStat = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 280, 545, 380, 25, hwnd, (HMENU)IDC_K_LBL_STAT, wc.hInstance, NULL);

    g_kdlg.hBtnSet = CreateWindowW(L"BUTTON", L"Set as Active Profile", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 670, 540, 155, 30, hwnd, (HMENU)IDC_K_BTN_SET, wc.hInstance, NULL);
    g_kdlg.hBtnClose = CreateWindowW(L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 835, 540, 85, 30, hwnd, (HMENU)IDC_K_BTN_CLOSE, wc.hInstance, NULL);

    HWND hChild = GetWindow(hwnd, GW_CHILD);
    while (hChild) {
        if (hChild == g_kdlg.hListBind) {
            SendMessage(hChild, WM_SETFONT, (WPARAM)g_kdlg.hFontMono, TRUE);
        } else if (hChild == g_kdlg.hLblTitle) {
            SendMessage(hChild, WM_SETFONT, (WPARAM)g_kdlg.hFontBold, TRUE);
        } else {
            SendMessage(hChild, WM_SETFONT, (WPARAM)g_kdlg.hFontUI, TRUE);
        }
        hChild = GetWindow(hChild, GW_HWNDNEXT);
    }

    k_refresh_prof();
    k_refresh_bind();

    RECT pr, dr;
    GetWindowRect(hParent, &pr);
    GetWindowRect(hwnd, &dr);
    int cx = pr.left + ((pr.right - pr.left) - (dr.right - dr.left)) / 2;
    int cy = pr.top + ((pr.bottom - pr.top) - (dr.bottom - dr.top)) / 2;
    SetWindowPos(hwnd, HWND_TOP, cx, cy, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
}
