#define WIN32_LEAN_AND_MEAN
#include "win_scriptref.h"
#include "plato/plato_script.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define IDC_SCRIPTREF_EDIT      601
#define IDC_SCRIPTREF_BTN_CLOSE 602

typedef struct {
    HWND hwnd;
    HWND hEdit;
    HWND hBtnClose;

    HBRUSH hBrushEditBg;
    HBRUSH hBrushBarBg;
    HBRUSH hBrushBtnBg;
    HBRUSH hBrushBtnPushed;
    HPEN   hPenBtnBorder;
    HFONT  hFontMono;
    HFONT  hFontUi;
} scriptref_ctx_t;

static scriptref_ctx_t g_sr;

static wchar_t* sr_convert_manual_text(void) {
    const char *src = plato_script_get_manual_text();
    if (!src) return NULL;

    /* Calcolo dimensione necessaria con conversione da \n a \r\n */
    size_t count = 0;
    for (size_t i = 0; src[i]; i++) {
        if (src[i] == '\n' && (i == 0 || src[i - 1] != '\r')) count += 2;
        else count++;
    }

    char *crlf_buf = (char *)malloc(count + 1);
    if (!crlf_buf) return NULL;

    size_t out_idx = 0;
    for (size_t i = 0; src[i]; i++) {
        if (src[i] == '\n' && (i == 0 || src[i - 1] != '\r')) {
            crlf_buf[out_idx++] = '\r';
            crlf_buf[out_idx++] = '\n';
        } else {
            crlf_buf[out_idx++] = src[i];
        }
    }
    crlf_buf[out_idx] = '\0';

    int wlen = MultiByteToWideChar(CP_UTF8, 0, crlf_buf, -1, NULL, 0);
    if (wlen <= 0) { free(crlf_buf); return NULL; }

    wchar_t *wbuf = (wchar_t *)malloc(wlen * sizeof(wchar_t));
    if (wbuf) {
        MultiByteToWideChar(CP_UTF8, 0, crlf_buf, -1, wbuf, wlen);
    }
    free(crlf_buf);
    return wbuf;
}

static void sr_draw_button(scriptref_ctx_t *ctx, DRAWITEMSTRUCT *dis, const wchar_t *text) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    bool pushed = (dis->itemState & ODS_SELECTED) != 0;

    HBRUSH hbg = pushed ? ctx->hBrushBtnPushed : ctx->hBrushBtnBg;
    FillRect(hdc, &rc, hbg);

    HPEN hOldPen = (HPEN)SelectObject(hdc, ctx->hPenBtnBorder);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 4, 4);
    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 110, 0));
    HFONT hOldFont = (HFONT)SelectObject(hdc, ctx->hFontUi);
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, hOldFont);
}

static LRESULT CALLBACK ScriptRefWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    scriptref_ctx_t *ctx = &g_sr;

    switch (msg) {
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND hCtrl = (HWND)lParam;
            if (hCtrl == ctx->hEdit) {
                SetTextColor(hdc, RGB(255, 110, 0)); /* Plasma Orange */
                SetBkColor(hdc, RGB(9, 3, 1));       /* Dark background */
                return (LRESULT)ctx->hBrushEditBg;
            }
            break;
        }

        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hwnd, &rc);
            RECT rcBar = {0, rc.bottom - 46, rc.right, rc.bottom};
            FillRect(hdc, &rcBar, ctx->hBrushBarBg);
            return 1;
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
            if (dis->CtlID == IDC_SCRIPTREF_BTN_CLOSE) {
                sr_draw_button(ctx, dis, L"Close");
                return TRUE;
            }
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == IDC_SCRIPTREF_BTN_CLOSE || wmId == IDCANCEL) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }

        case WM_SIZE: {
            int w = LOWORD(lParam);
            int h = HIWORD(lParam);
            int barH = 46;
            int editH = h - barH;
            if (editH < 50) editH = 50;

            if (ctx->hEdit) {
                SetWindowPos(ctx->hEdit, NULL, 0, 0, w, editH, SWP_NOZORDER);
            }
            if (ctx->hBtnClose) {
                int btnW = 90, btnH = 28;
                int bx = (w - btnW) / 2;
                int by = h - barH + (barH - btnH) / 2;
                SetWindowPos(ctx->hBtnClose, NULL, bx, by, btnW, btnH, SWP_NOZORDER);
            }
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (ctx->hBrushEditBg)    { DeleteObject(ctx->hBrushEditBg);    ctx->hBrushEditBg = NULL; }
            if (ctx->hBrushBarBg)     { DeleteObject(ctx->hBrushBarBg);     ctx->hBrushBarBg = NULL; }
            if (ctx->hBrushBtnBg)     { DeleteObject(ctx->hBrushBtnBg);     ctx->hBrushBtnBg = NULL; }
            if (ctx->hBrushBtnPushed) { DeleteObject(ctx->hBrushBtnPushed); ctx->hBrushBtnPushed = NULL; }
            if (ctx->hPenBtnBorder)   { DeleteObject(ctx->hPenBtnBorder);   ctx->hPenBtnBorder = NULL; }
            if (ctx->hFontMono)       { DeleteObject(ctx->hFontMono);       ctx->hFontMono = NULL; }
            if (ctx->hFontUi)         { DeleteObject(ctx->hFontUi);         ctx->hFontUi = NULL; }
            ctx->hwnd = NULL;
            ctx->hEdit = NULL;
            ctx->hBtnClose = NULL;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void win_scriptref_show(HWND hwndParent) {
    scriptref_ctx_t *ctx = &g_sr;
    if (ctx->hwnd && IsWindow(ctx->hwnd)) {
        SetForegroundWindow(ctx->hwnd);
        return;
    }

    HINSTANCE hInst = GetModuleHandle(NULL);
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ScriptRefWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(1));
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"PlatoScriptRefClass";
    RegisterClassExW(&wc);

    ctx->hBrushEditBg    = CreateSolidBrush(RGB(9, 3, 1));
    ctx->hBrushBarBg     = CreateSolidBrush(RGB(10, 4, 1));
    ctx->hBrushBtnBg     = CreateSolidBrush(RGB(46, 20, 8));
    ctx->hBrushBtnPushed = CreateSolidBrush(RGB(75, 32, 12));
    ctx->hPenBtnBorder   = CreatePen(PS_SOLID, 1, RGB(80, 35, 12));

    ctx->hFontMono = CreateFontW(
        -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas"
    );

    ctx->hFontUi = CreateFontW(
        -12, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_SWISS, L"Segoe UI"
    );

    int winW = 860;
    int winH = 780;
    int posX = CW_USEDEFAULT;
    int posY = CW_USEDEFAULT;

    if (hwndParent) {
        RECT rcP;
        GetWindowRect(hwndParent, &rcP);
        posX = rcP.left + (rcP.right - rcP.left - winW) / 2;
        posY = rcP.top + (rcP.bottom - rcP.top - winH) / 2;
        if (posX < 0) posX = 0;
        if (posY < 0) posY = 0;
    }

    ctx->hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"PlatoScriptRefClass",
        L"PLATO Scripting Engine Reference",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX,
        posX, posY, winW, winH,
        hwndParent, NULL, hInst, NULL
    );

    if (!ctx->hwnd) return;

    wchar_t *wmanual = sr_convert_manual_text();

    ctx->hEdit = CreateWindowExW(
        0, L"EDIT", wmanual ? wmanual : L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL |
        ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_NOHIDESEL,
        0, 0, winW, winH - 46,
        ctx->hwnd, (HMENU)(INT_PTR)IDC_SCRIPTREF_EDIT, hInst, NULL
    );

    if (wmanual) free(wmanual);

    if (ctx->hFontMono && ctx->hEdit) {
        SendMessageW(ctx->hEdit, WM_SETFONT, (WPARAM)ctx->hFontMono, TRUE);
    }

    ctx->hBtnClose = CreateWindowExW(
        0, L"BUTTON", L"Close",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        (winW - 90) / 2, winH - 46 + (46 - 28) / 2, 90, 28,
        ctx->hwnd, (HMENU)(INT_PTR)IDC_SCRIPTREF_BTN_CLOSE, hInst, NULL
    );

    ShowWindow(ctx->hwnd, SW_SHOW);
    UpdateWindow(ctx->hwnd);
}
