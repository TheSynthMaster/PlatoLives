#define WIN32_LEAN_AND_MEAN
#include "win_text_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define IDC_TB_TEXT         301
#define IDC_TB_BTN_SELALL   302
#define IDC_TB_BTN_LIVE     303
#define IDC_TB_CHK_COMPACT  304
#define IDC_TB_BTN_COPY     305
#define IDC_TB_LBL_STATUS   306
#define IDT_TB_REFRESH      307

typedef struct {
    HWND hwnd;
    HWND hEdit;
    HWND hBtnSelAll, hBtnLive, hChkCompact, hBtnCopy, hLblStatus;
    plato_terminal_t *terminal;
    bool is_frozen;
    bool is_compact;
    wchar_t last_text[PLATO_ROWS * (PLATO_COLS + 4) + 1];

    HBRUSH hBrushEditBg;
    HBRUSH hBrushBarBg;
    HBRUSH hBrushBtnBg;
    HBRUSH hBrushBtnPushed;
    HPEN   hPenBtnBorder;
    HFONT  hFontMono;
    HFONT  hFontUi;
} tb_ctx_t;

static tb_ctx_t g_tb;

static void tb_refresh_text(tb_ctx_t *ctx) {
    if (!ctx->terminal || ctx->is_frozen) return;

    char utf8_buf[PLATO_ROWS * (PLATO_COLS + 4) + 1];
    size_t len = plato_terminal_get_text_area(ctx->terminal, 0, 0, PLATO_COLS - 1, PLATO_ROWS - 1, ctx->is_compact, utf8_buf, sizeof(utf8_buf));
    if (len == 0) return;

    wchar_t wbuf[sizeof(utf8_buf)];
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8_buf, (int)len, wbuf, (int)(sizeof(wbuf)/sizeof(wbuf[0]) - 1));
    if (wlen <= 0) return;
    wbuf[wlen] = L'\0';

    wchar_t crlf_wbuf[sizeof(wbuf) * 2];
    size_t c_idx = 0;
    for (int i = 0; i < wlen && c_idx < (sizeof(crlf_wbuf)/sizeof(crlf_wbuf[0]) - 3); i++) {
        if (wbuf[i] == L'\n') {
            crlf_wbuf[c_idx++] = L'\r';
            crlf_wbuf[c_idx++] = L'\n';
        } else {
            crlf_wbuf[c_idx++] = wbuf[i];
        }
    }
    crlf_wbuf[c_idx] = L'\0';

    if (wcscmp(crlf_wbuf, ctx->last_text) != 0) {
        wcsncpy(ctx->last_text, crlf_wbuf, sizeof(ctx->last_text)/sizeof(ctx->last_text[0]) - 1);

        /* Salva la riga visibile per non far scattare il testo in alto */
        int first_visible = (int)SendMessage(ctx->hEdit, EM_GETFIRSTVISIBLELINE, 0, 0);

        SetWindowTextW(ctx->hEdit, crlf_wbuf);

        /* Ripristina la riga visibile */
        int new_visible = (int)SendMessage(ctx->hEdit, EM_GETFIRSTVISIBLELINE, 0, 0);
        if (first_visible != new_visible) {
            SendMessage(ctx->hEdit, EM_LINESCROLL, 0, (LPARAM)(first_visible - new_visible));
        }
    }
}

static void tb_copy_selection_or_all(tb_ctx_t *ctx) {
    DWORD start = 0, end = 0;
    SendMessage(ctx->hEdit, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);

    int text_len = GetWindowTextLengthW(ctx->hEdit);
    if (text_len <= 0) return;

    wchar_t *full_text = (wchar_t*)malloc((text_len + 1) * sizeof(wchar_t));
    GetWindowTextW(ctx->hEdit, full_text, text_len + 1);

    wchar_t *copy_buf = NULL;
    size_t copy_chars = 0;

    if (start != end && end > start && (int)end <= text_len) {
        copy_chars = end - start;
        copy_buf = (wchar_t*)malloc((copy_chars + 1) * sizeof(wchar_t));
        wmemcpy(copy_buf, full_text + start, copy_chars);
        copy_buf[copy_chars] = L'\0';
    } else {
        copy_chars = text_len;
        copy_buf = full_text;
        full_text = NULL;
    }
    if (full_text) free(full_text);

    if (copy_buf && OpenClipboard(ctx->hwnd)) {
        EmptyClipboard();
        size_t byte_count = (copy_chars + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, byte_count);
        if (hMem) {
            wchar_t *p = (wchar_t*)GlobalLock(hMem);
            wmemcpy(p, copy_buf, copy_chars + 1);
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
        CloseClipboard();
    }
    if (copy_buf) free(copy_buf);
}


static WNDPROC g_old_edit_proc = NULL;

static void tb_set_frozen(tb_ctx_t *ctx, bool frozen) {
    if (ctx->is_frozen == frozen) return;
    ctx->is_frozen = frozen;
    if (ctx->is_frozen) {
        SetWindowTextW(ctx->hLblStatus, L"[⏸ FROZEN]");
        SetWindowTextW(ctx->hwnd, L"PLATO Live Text Buffer [⏸ FROZEN]");
    } else {
        SetWindowTextW(ctx->hLblStatus, L"[● LIVE]");
        SetWindowTextW(ctx->hwnd, L"PLATO Live Text Buffer [● LIVE]");
    }
    InvalidateRect(ctx->hLblStatus, NULL, TRUE);
}

static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    tb_ctx_t *ctx = &g_tb;
    LRESULT res = CallWindowProcW(g_old_edit_proc, hwnd, msg, wParam, lParam);

    if (msg == WM_LBUTTONUP || msg == WM_KEYUP || (msg == WM_MOUSEMOVE && (wParam & MK_LBUTTON))) {
        DWORD start = 0, end = 0;
        SendMessage(hwnd, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
        if (start != end && !ctx->is_frozen) {
            tb_set_frozen(ctx, true);
        }
    }
    return res;
}

static void tb_draw_custom_button(tb_ctx_t *ctx, DRAWITEMSTRUCT *dis, const wchar_t *text) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    bool is_pushed = (dis->itemState & ODS_SELECTED);

    HBRUSH bg = is_pushed ? ctx->hBrushBtnPushed : ctx->hBrushBtnBg;
    FillRect(hdc, &rc, bg);

    HPEN oldPen = (HPEN)SelectObject(hdc, ctx->hPenBtnBorder);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 4, 4);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 110, 0));
    SelectObject(hdc, ctx->hFontUi);

    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static LRESULT CALLBACK TextBufferWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    tb_ctx_t *ctx = &g_tb;
    switch (msg) {
        case WM_TIMER:
            if (wParam == IDT_TB_REFRESH) {
                tb_refresh_text(ctx);
            }
            return 0;

        case WM_GETMINMAXINFO: {
            MINMAXINFO *mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = 440;
            mmi->ptMinTrackSize.y = 400;
            return 0;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, RGB(255, 110, 0)); /* Orange Plasma Identico */
            SetBkColor(hdc, RGB(9, 3, 1));        /* Sfondo Scuro Mac */
            return (LRESULT)ctx->hBrushEditBg;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND hCtrl = (HWND)lParam;
            if (hCtrl == ctx->hLblStatus) {
                SetBkMode(hdc, TRANSPARENT);
                if (ctx->is_frozen) {
                    SetTextColor(hdc, RGB(255, 110, 0));
                } else {
                    SetTextColor(hdc, RGB(51, 230, 77));
                }
                return (LRESULT)ctx->hBrushBarBg;
            }
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 110, 0));
            return (LRESULT)ctx->hBrushBarBg;
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT*)lParam;
            if (dis->CtlID == IDC_TB_BTN_SELALL) {
                tb_draw_custom_button(ctx, dis, L"Select All");
                return TRUE;
            } else if (dis->CtlID == IDC_TB_BTN_LIVE) {
                tb_draw_custom_button(ctx, dis, L"Deselect / Live");
                return TRUE;
            } else if (dis->CtlID == IDC_TB_BTN_COPY) {
                tb_draw_custom_button(ctx, dis, L"Copy to Clipboard");
                return TRUE;
            }
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);

            if (wmId == IDC_TB_TEXT && (wmEvent == EN_SETFOCUS || wmEvent == EN_CHANGE)) {
                DWORD start = 0, end = 0;
                SendMessage(ctx->hEdit, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
                if (start != end && !ctx->is_frozen) {
                    tb_set_frozen(ctx, true);
                }
                return 0;
            }

            switch (wmId) {
                case IDC_TB_BTN_SELALL:
                    SendMessage(ctx->hEdit, EM_SETSEL, 0, -1);
                    tb_set_frozen(ctx, true);
                    SetFocus(ctx->hEdit);
                    return 0;

                case IDC_TB_BTN_LIVE:
                    SendMessage(ctx->hEdit, EM_SETSEL, 0, 0);
                    tb_set_frozen(ctx, false);
                    tb_refresh_text(ctx);
                    return 0;

                case IDC_TB_CHK_COMPACT:
                    ctx->is_compact = (SendMessage(ctx->hChkCompact, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    ctx->last_text[0] = L'\0';
                    tb_refresh_text(ctx);
                    return 0;

                case IDC_TB_BTN_COPY:
                    tb_copy_selection_or_all(ctx);
                    return 0;
            }
            break;
        }

        case WM_SIZE: {
            UINT w = LOWORD(lParam), h = HIWORD(lParam);
            if (ctx->hEdit && h > 48) {
                SetWindowPos(ctx->hEdit, NULL, 0, 0, w, h - 48, SWP_NOZORDER);
                SetWindowPos(ctx->hBtnSelAll, NULL, 8, h - 38, 68, 26, SWP_NOZORDER);
                SetWindowPos(ctx->hBtnLive, NULL, 82, h - 38, 104, 26, SWP_NOZORDER);
                SetWindowPos(ctx->hChkCompact, NULL, 192, h - 35, 72, 20, SWP_NOZORDER);
                SetWindowPos(ctx->hBtnCopy, NULL, 270, h - 38, 130, 26, SWP_NOZORDER);
                SetWindowPos(ctx->hLblStatus, NULL, w - 88, h - 35, 80, 20, SWP_NOZORDER);
            }
            return 0;
        }

        case WM_CLOSE:
            KillTimer(hwnd, IDT_TB_REFRESH);
            if (ctx->hBrushEditBg) DeleteObject(ctx->hBrushEditBg);
            if (ctx->hBrushBarBg) DeleteObject(ctx->hBrushBarBg);
            if (ctx->hBrushBtnBg) DeleteObject(ctx->hBrushBtnBg);
            if (ctx->hBrushBtnPushed) DeleteObject(ctx->hBrushBtnPushed);
            if (ctx->hPenBtnBorder) DeleteObject(ctx->hPenBtnBorder);
            if (ctx->hFontMono) DeleteObject(ctx->hFontMono);
            DestroyWindow(hwnd);
            ctx->hwnd = NULL;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void win_text_buffer_show(HWND hParent, plato_terminal_t *terminal) {
    if (g_tb.hwnd && IsWindow(g_tb.hwnd)) {
        ShowWindow(g_tb.hwnd, SW_RESTORE);
        SetForegroundWindow(g_tb.hwnd);
        return;
    }

    memset(&g_tb, 0, sizeof(g_tb));
    g_tb.terminal = terminal;
    g_tb.is_compact = false;
    g_tb.is_frozen = false;

    g_tb.hBrushEditBg = CreateSolidBrush(RGB(9, 3, 1));
    g_tb.hBrushBarBg = CreateSolidBrush(RGB(10, 4, 1));
    g_tb.hBrushBtnBg = CreateSolidBrush(RGB(46, 20, 8));
    g_tb.hBrushBtnPushed = CreateSolidBrush(RGB(75, 32, 12));
    g_tb.hPenBtnBorder = CreatePen(PS_SOLID, 1, RGB(80, 35, 12));

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = TextBufferWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_tb.hBrushBarBg;
    wc.lpszClassName = L"PlatoLiveTextBufferClass";
    RegisterClassExW(&wc);

    /* Calcolo esatto del frame: 680x624 px di area client pura per ospitare 64x32 caratteri */
    RECT wr = { 0, 0, 500, 624 };
    AdjustWindowRectEx(&wr, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_TOOLWINDOW);

    HWND hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        wc.lpszClassName, L"PLATO Live Text Buffer [● LIVE]",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        hParent, NULL, wc.hInstance, NULL
    );
    if (!hwnd) return;
    g_tb.hwnd = hwnd;

    HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
    if (hDwm) {
        typedef HRESULT (WINAPI *fnDwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
        fnDwmSetWindowAttribute pDwm = (fnDwmSetWindowAttribute)GetProcAddress(hDwm, "DwmSetWindowAttribute");
        if (pDwm) {
            BOOL dark = TRUE;
            COLORREF cap = RGB(16, 12, 10);
            COLORREF txt = RGB(255, 110, 0);
            pDwm(hwnd, 20, &dark, sizeof(dark));
            pDwm(hwnd, 35, &cap, sizeof(cap));
            pDwm(hwnd, 36, &txt, sizeof(txt));
        }
        FreeLibrary(hDwm);
    }

    g_tb.hFontUi = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    g_tb.hFontMono = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 FIXED_PITCH | FF_MODERN, L"Consolas");

    g_tb.hEdit = CreateWindowW(L"EDIT", L"",
                              WS_CHILD | WS_VISIBLE |
                              ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                              0, 0, 500, 576, hwnd, (HMENU)IDC_TB_TEXT, wc.hInstance, NULL);
    SendMessage(g_tb.hEdit, WM_SETFONT, (WPARAM)g_tb.hFontMono, TRUE);
    g_old_edit_proc = (WNDPROC)SetWindowLongPtrW(g_tb.hEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

    /* Margini interni 10 px come textContainerInset di macOS */
    SendMessage(g_tb.hEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(10, 10));

    g_tb.hBtnSelAll = CreateWindowW(L"BUTTON", L"Select All", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 8, 584, 68, 26, hwnd, (HMENU)IDC_TB_BTN_SELALL, wc.hInstance, NULL);
    g_tb.hBtnLive = CreateWindowW(L"BUTTON", L"Deselect / Live", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 82, 584, 104, 26, hwnd, (HMENU)IDC_TB_BTN_LIVE, wc.hInstance, NULL);
    g_tb.hChkCompact = CreateWindowW(L"BUTTON", L"Compact", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 192, 587, 72, 20, hwnd, (HMENU)IDC_TB_CHK_COMPACT, wc.hInstance, NULL);
    g_tb.hBtnCopy = CreateWindowW(L"BUTTON", L"Copy to Clipboard", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 270, 584, 130, 26, hwnd, (HMENU)IDC_TB_BTN_COPY, wc.hInstance, NULL);
    g_tb.hLblStatus = CreateWindowW(L"STATIC", L"[● LIVE]", WS_CHILD | WS_VISIBLE | SS_RIGHT, 408, 587, 80, 20, hwnd, (HMENU)IDC_TB_LBL_STATUS, wc.hInstance, NULL);

    SendMessage(g_tb.hChkCompact, WM_SETFONT, (WPARAM)g_tb.hFontUi, TRUE);
    SendMessage(g_tb.hLblStatus, WM_SETFONT, (WPARAM)g_tb.hFontUi, TRUE);

    tb_refresh_text(&g_tb);
    SetTimer(hwnd, IDT_TB_REFRESH, 100, NULL);
}
