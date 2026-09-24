#define WIN32_LEAN_AND_MEAN
#include "win_keyref.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define IDC_KEYREF_EDIT      501
#define IDC_KEYREF_BTN_CLOSE 502

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
} keyref_ctx_t;

static keyref_ctx_t g_kr;

static const wchar_t *g_keyref_text =
    L"======================================================================\r\n"
    L"                     PLATO KEYBOARD REFERENCE\r\n"
    L"======================================================================\r\n\r\n"
    L"PLATO KEY           WINDOWS SHORTCUT\r\n"
    L"----------------------------------------------------------------------\r\n"
    L"NEXT                Return / Enter / Ctrl+N\r\n"
    L"SHIFT-NEXT          Shift+Return / Shift+Ctrl+N\r\n"
    L"BACK                Esc / Backspace / F8 / Ctrl+B\r\n"
    L"SHIFT-BACK          Shift+Esc / Shift+F8 / Shift+Ctrl+B\r\n"
    L"STOP                F10 / F4 / Ctrl+S / Alt+S\r\n"
    L"SHIFT-STOP          Shift + any STOP shortcut\r\n"
    L"ERASE               Backspace / Delete / Left Arrow / Ctrl+R\r\n"
    L"SHIFT-ERASE         Shift + any ERASE shortcut\r\n"
    L"HELP                F1 / F6 / Ctrl+H / Alt+H\r\n"
    L"SHIFT-HELP          Shift + any HELP shortcut\r\n"
    L"LAB                 F2 / F7 / Ctrl+L / Alt+L\r\n"
    L"SHIFT-LAB           Shift + any LAB shortcut\r\n"
    L"DATA                F3 / F9 / Ctrl+D / Alt+D\r\n"
    L"SHIFT-DATA          Shift + any DATA shortcut\r\n"
    L"EDIT                F5 / Ctrl+E / Alt+E\r\n"
    L"SHIFT-EDIT          Shift+F5 / Shift+Ctrl+E\r\n"
    L"COPY                Ctrl+C / Alt+C\r\n"
    L"SHIFT-COPY          Shift+Ctrl+C / Shift+Alt+C\r\n"
    L"MICRO               Ctrl+M / Alt+M\r\n"
    L"FONT                Ctrl+F / Alt+F\r\n"
    L"SUPER               Up Arrow / Page Up / Ctrl+P\r\n"
    L"SHIFT-SUPER         Shift + any SUPER shortcut (Ascend)\r\n"
    L"SUB                 Down Arrow / Page Down / Ctrl+Y\r\n"
    L"SHIFT-SUB           Shift + any SUB shortcut (Descend)\r\n"
    L"ANS                 Ctrl+A / Ctrl+/ / Alt+A\r\n"
    L"TERM                Ctrl+T / Alt+T / Shift+Ctrl+A\r\n"
    L"SQUARE              Ctrl+Q / Alt+Q\r\n"
    L"ACCESS              Shift+Ctrl+Q / Shift+Alt+Q\r\n"
    L"TAB                 Tab / Right Arrow\r\n"
    L"ASSIGN (<=)         Alt + Left Arrow\r\n"
    L"MULTIPLY (*)        Ctrl+X / Alt+X\r\n"
    L"DIVIDE (/)          Ctrl+G / Alt+G\r\n\r\n"
    L"======================================================================\r\n"
    L"                     APPLICATION SHORTCUTS\r\n"
    L"======================================================================\r\n\r\n"
    L"ACTION                   WINDOWS SHORTCUT\r\n"
    L"----------------------------------------------------------------------\r\n"
    L"New Window               Ctrl+N\r\n"
    L"Close Window             Ctrl+W\r\n"
    L"Exit                     Alt+F4\r\n"
    L"Connection Profiles      Menu Connection -> Manage Profiles...\r\n"
    L"Copy Screen Image        Menu Edit -> Copy Screen Image\r\n"
    L"Paste Text (Paced)       Ctrl+V\r\n"
    L"Cancel Paste             Menu Edit -> Cancel Paste\r\n"
    L"Live Text Buffer         Ctrl+Shift+T\r\n"
    L"Keyboard Reference       Ctrl+K\r\n"
    L"Full Screen Toggle       F11 / Alt+Enter\r\n"
    L"Performance HUD          F12\r\n";

static void kr_draw_button(keyref_ctx_t *ctx, DRAWITEMSTRUCT *dis, const wchar_t *text) {
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

static LRESULT CALLBACK KeyRefWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    keyref_ctx_t *ctx = &g_kr;
    switch (msg) {
        case WM_MOUSEWHEEL: {
            if (ctx && ctx->hEdit) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                int lines = -(delta / WHEEL_DELTA) * 3;
                SendMessageW(ctx->hEdit, EM_LINESCROLL, 0, lines);
                return 0;
            }
            break;
        }
        case WM_GETMINMAXINFO: {
            MINMAXINFO *mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = 540;
            mmi->ptMinTrackSize.y = 520;
            return 0;
        }

        case WM_SIZE: {
            int w = LOWORD(lParam);
            int h = HIWORD(lParam);
            int bar_h = 52;
            int edit_h = (h > bar_h) ? (h - bar_h) : 0;

            if (ctx->hEdit) {
                SetWindowPos(ctx->hEdit, NULL, 0, 0, w, edit_h, SWP_NOZORDER);
            }
            if (ctx->hBtnClose) {
                int btn_w = 110, btn_h = 32;
                int btn_x = w - btn_w - 16;
                int btn_y = edit_h + (bar_h - btn_h) / 2;
                SetWindowPos(ctx->hBtnClose, NULL, btn_x, btn_y, btn_w, btn_h, SWP_NOZORDER);
            }
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            int bar_h = 52;
            int edit_h = (rc.bottom > bar_h) ? (rc.bottom - bar_h) : 0;

            RECT bar_rc = { 0, edit_h, rc.right, rc.bottom };
            FillRect(hdc, &bar_rc, ctx->hBrushBarBg);

            HPEN hPenLine = CreatePen(PS_SOLID, 1, RGB(46, 20, 8));
            HPEN oldPen = (HPEN)SelectObject(hdc, hPenLine);
            MoveToEx(hdc, 0, edit_h, NULL);
            LineTo(hdc, rc.right, edit_h);
            SelectObject(hdc, oldPen);
            DeleteObject(hPenLine);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND hCtrl = (HWND)lParam;
            if (hCtrl == ctx->hEdit) {
                SetTextColor(hdc, RGB(255, 110, 0)); /* Orange Plasma Identico */
                SetBkColor(hdc, RGB(9, 3, 1));        /* Sfondo Scuro Gas */
                return (LRESULT)ctx->hBrushEditBg;
            }
            break;
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT*)lParam;
            if (dis->CtlID == IDC_KEYREF_BTN_CLOSE) {
                kr_draw_button(ctx, dis, L"Close (Esc)");
                return TRUE;
            }
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == IDC_KEYREF_BTN_CLOSE || wmId == IDCANCEL) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            ctx->hwnd = NULL;
            ctx->hEdit = NULL;
            ctx->hBtnClose = NULL;
            if (ctx->hBrushEditBg)   { DeleteObject(ctx->hBrushEditBg);   ctx->hBrushEditBg = NULL; }
            if (ctx->hBrushBarBg)    { DeleteObject(ctx->hBrushBarBg);    ctx->hBrushBarBg = NULL; }
            if (ctx->hBrushBtnBg)    { DeleteObject(ctx->hBrushBtnBg);    ctx->hBrushBtnBg = NULL; }
            if (ctx->hBrushBtnPushed){ DeleteObject(ctx->hBrushBtnPushed);ctx->hBrushBtnPushed = NULL; }
            if (ctx->hPenBtnBorder)  { DeleteObject(ctx->hPenBtnBorder);  ctx->hPenBtnBorder = NULL; }
            if (ctx->hFontMono)      { DeleteObject(ctx->hFontMono);      ctx->hFontMono = NULL; }
            if (ctx->hFontUi)        { DeleteObject(ctx->hFontUi);        ctx->hFontUi = NULL; }
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void win_keyref_show(HWND hwndParent) {
    keyref_ctx_t *ctx = &g_kr;
    if (ctx->hwnd && IsWindow(ctx->hwnd)) {
        SetForegroundWindow(ctx->hwnd);
        return;
    }

    HINSTANCE hInst = GetModuleHandle(NULL);
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = KeyRefWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(1));
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"PlatoKeyRefClass";
    RegisterClassExW(&wc);

    ctx->hBrushEditBg   = CreateSolidBrush(RGB(9, 3, 1));
    ctx->hBrushBarBg    = CreateSolidBrush(RGB(10, 4, 1));
    ctx->hBrushBtnBg    = CreateSolidBrush(RGB(46, 20, 8));
    ctx->hBrushBtnPushed= CreateSolidBrush(RGB(75, 32, 12));
    ctx->hPenBtnBorder  = CreatePen(PS_SOLID, 1, RGB(80, 35, 12));

    ctx->hFontMono = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas"
    );

    ctx->hFontUi = CreateFontW(
        -12, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_SWISS, L"Segoe UI"
    );

    RECT pr = {0};
    if (hwndParent) GetWindowRect(hwndParent, &pr);
    int dw = 630, dh = 880;
    int px = pr.left + ((pr.right - pr.left) - dw) / 2;
    int py = pr.top + ((pr.bottom - pr.top) - dh) / 2;
    if (px < 40) px = 40;
    if (py < 40) py = 40;

    ctx->hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        wc.lpszClassName,
        L"PLATO Keyboard Reference",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN | WS_THICKFRAME,
        px, py, dw, dh,
        hwndParent, NULL, hInst, NULL
    );
    if (!ctx->hwnd) return;

    /* Immersive Dark Mode per barra del titolo Windows 10/11 */
    HMODULE hDwm = LoadLibraryA("dwmapi.dll");
    if (hDwm) {
        typedef HRESULT (WINAPI *DwmSetWindowAttributeFunc)(HWND, DWORD, LPCVOID, DWORD);
        DwmSetWindowAttributeFunc set_attr = (DwmSetWindowAttributeFunc)(void*)GetProcAddress(hDwm, "DwmSetWindowAttribute");
        if (set_attr) {
            BOOL dark = TRUE;
            COLORREF cap = RGB(16, 12, 10);
            COLORREF txt = RGB(255, 110, 0);
            set_attr(ctx->hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
            set_attr(ctx->hwnd, 35, &cap, sizeof(cap));
            set_attr(ctx->hwnd, 36, &txt, sizeof(txt));
        }
        FreeLibrary(hDwm);
    }

    ctx->hEdit = CreateWindowExW(
        0, L"EDIT", g_keyref_text,
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        0, 0, dw, dh - 52,
        ctx->hwnd, (HMENU)(INT_PTR)IDC_KEYREF_EDIT, hInst, NULL
    );
    SendMessage(ctx->hEdit, WM_SETFONT, (WPARAM)ctx->hFontMono, TRUE);
    SendMessage(ctx->hEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(16, 16));

    ctx->hBtnClose = CreateWindowExW(
        0, L"BUTTON", L"Close (Esc)",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        dw - 126, dh - 42, 110, 32,
        ctx->hwnd, (HMENU)(INT_PTR)IDC_KEYREF_BTN_CLOSE, hInst, NULL
    );

    ShowWindow(ctx->hwnd, SW_SHOW);
    UpdateWindow(ctx->hwnd);
    SetFocus(ctx->hBtnClose);
}
