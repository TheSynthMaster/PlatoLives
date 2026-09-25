#define WIN32_LEAN_AND_MEAN
#include "win_about.h"
#include <stdio.h>
#include <stdbool.h>

#define IDC_ABOUT_BTN_CLOSE 601

#define PLATO_PLASMA_COLOR  RGB(255, 110, 0)
#define PLATO_PLASMA_BRIGHT RGB(255, 160, 40)
#define PLATO_PLASMA_MUTED  RGB(190, 85, 10)
#define PLATO_BG_DARK       RGB(16, 12, 10)
#define PLATO_BTN_BG        RGB(46, 20, 8)
#define PLATO_BTN_PUSHED    RGB(75, 32, 12)

typedef struct {
    HWND hwnd;
    HWND hBtnClose;
    HICON hIcon;
    HBRUSH hBrushBg;
    HBRUSH hBrushBtn;
    HBRUSH hBrushBtnPushed;
    HPEN   hPenBtn;
    HFONT  hFontTitle;
    HFONT  hFontSub;
    HFONT  hFontBody;
    HFONT  hFontMuted;
} about_ctx_t;

static about_ctx_t g_about;

static void about_draw_button(about_ctx_t *ctx, DRAWITEMSTRUCT *dis) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    bool pushed = (dis->itemState & ODS_SELECTED) != 0;

    HBRUSH bg = pushed ? ctx->hBrushBtnPushed : ctx->hBrushBtn;
    FillRect(hdc, &rc, bg);

    HPEN oldPen = (HPEN)SelectObject(hdc, ctx->hPenBtn);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, pushed ? RGB(255, 200, 70) : PLATO_PLASMA_COLOR);
    HFONT oldFont = (HFONT)SelectObject(hdc, ctx->hFontSub);
    DrawTextW(hdc, L"Close", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);
}

static LRESULT CALLBACK AboutWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    about_ctx_t *ctx = &g_about;
    switch (msg) {
        case WM_DRAWITEM: {
            DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
            if (dis->CtlID == IDC_ABOUT_BTN_CLOSE) {
                about_draw_button(ctx, dis);
                return TRUE;
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);

            FillRect(hdc, &rcClient, ctx->hBrushBg);

            int cx = rcClient.right / 2;

            /* 1. Icona PLATO a 64x64 pixel centrata in alto */
            if (ctx->hIcon) {
                DrawIconEx(hdc, cx - 32, 18, ctx->hIcon, 64, 64, 0, NULL, DI_NORMAL);
            }

            SetBkMode(hdc, TRANSPARENT);

            /* 2. Titolo PlatoLives v4.2 */
            SetTextColor(hdc, PLATO_PLASMA_BRIGHT);
            SelectObject(hdc, ctx->hFontTitle);
            RECT rcTitle = { 0, 92, rcClient.right, 122 };
            DrawTextW(hdc, L"PlatoLives v4.2", -1, &rcTitle, DT_CENTER | DT_SINGLELINE);

            /* 3. Sottotitolo */
            SetTextColor(hdc, PLATO_PLASMA_COLOR);
            SelectObject(hdc, ctx->hFontSub);
            RECT rcSub = { 0, 122, rcClient.right, 144 };
            DrawTextW(hdc, L"CDC PLATO IV & Cyber1 Terminal Emulator", -1, &rcSub, DT_CENTER | DT_SINGLELINE);

            /* Linea separatrice ambra */
            HPEN hSep = CreatePen(PS_SOLID, 1, RGB(70, 32, 10));
            HPEN oldPen = (HPEN)SelectObject(hdc, hSep);
            MoveToEx(hdc, 35, 154, NULL);
            LineTo(hdc, rcClient.right - 35, 154);
            SelectObject(hdc, oldPen);
            DeleteObject(hSep);

            /* 4. Descrizione e crediti */
            SetTextColor(hdc, PLATO_PLASMA_COLOR);
            SelectObject(hdc, ctx->hFontBody);

            const wchar_t *desc =
                L"A modern, native PLATO terminal emulator for Windows.\n"
                L"Zero-Bloatware Pure C11 & Direct3D 11 Architecture.\n"
                L"Hardware-authentic plasma and CRT phosphor simulation.\n\n"
                L"Created by Fabio Montarsolo  (fabio.montarsolo@gmail.com)\n"
                L"Open-source software keeping the PLATO experience alive.";

            RECT rcDesc = { 35, 168, rcClient.right - 35, 290 };
            DrawTextW(hdc, desc, -1, &rcDesc, DT_CENTER);

            /* 5. Copyright */
            SetTextColor(hdc, PLATO_PLASMA_MUTED);
            SelectObject(hdc, ctx->hFontMuted);
            RECT rcCopy = { 0, 294, rcClient.right, 314 };
            DrawTextW(hdc, L"(C) 2026 Fabio Montarsolo - All Rights Reserved.", -1, &rcCopy, DT_CENTER | DT_SINGLELINE);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_ABOUT_BTN_CLOSE) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE || wParam == VK_RETURN || wParam == VK_SPACE) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;

        case WM_DESTROY:
            if (ctx->hBrushBg) DeleteObject(ctx->hBrushBg);
            if (ctx->hBrushBtn) DeleteObject(ctx->hBrushBtn);
            if (ctx->hBrushBtnPushed) DeleteObject(ctx->hBrushBtnPushed);
            if (ctx->hPenBtn) DeleteObject(ctx->hPenBtn);
            if (ctx->hFontTitle) DeleteObject(ctx->hFontTitle);
            if (ctx->hFontSub) DeleteObject(ctx->hFontSub);
            if (ctx->hFontBody) DeleteObject(ctx->hFontBody);
            if (ctx->hFontMuted) DeleteObject(ctx->hFontMuted);
            ctx->hwnd = NULL;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void win_about_show(HWND hParent) {
    if (g_about.hwnd && IsWindow(g_about.hwnd)) {
        ShowWindow(g_about.hwnd, SW_RESTORE);
        SetForegroundWindow(g_about.hwnd);
        return;
    }

    memset(&g_about, 0, sizeof(g_about));
    HINSTANCE hInst = GetModuleHandle(NULL);

    g_about.hBrushBg        = CreateSolidBrush(PLATO_BG_DARK);
    g_about.hBrushBtn       = CreateSolidBrush(PLATO_BTN_BG);
    g_about.hBrushBtnPushed = CreateSolidBrush(PLATO_BTN_PUSHED);
    g_about.hPenBtn         = CreatePen(PS_SOLID, 1, PLATO_PLASMA_COLOR);

    g_about.hFontTitle = CreateFontW(-20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                    DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_about.hFontSub   = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                    DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_about.hFontBody  = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                    DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_about.hFontMuted = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                    DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    /* Carica icona ad alta risoluzione 64x64 */
    g_about.hIcon = (HICON)LoadImage(hInst, MAKEINTRESOURCE(1), IMAGE_ICON, 64, 64, LR_DEFAULTCOLOR);

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = AboutWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_about.hBrushBg;
    wc.lpszClassName = L"PlatoAboutDialogClass";
    RegisterClassExW(&wc);

    /* Area client pura garantita: 480 x 440 px */
    int client_w = 480;
    int client_h = 440;
    RECT wr = { 0, 0, client_w, client_h };
    AdjustWindowRectEx(&wr, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    int win_w = wr.right - wr.left;
    int win_h = wr.bottom - wr.top;

    RECT rcParent;
    GetWindowRect(hParent, &rcParent);
    int px = rcParent.left + (rcParent.right - rcParent.left - win_w) / 2;
    int py = rcParent.top + (rcParent.bottom - rcParent.top - win_h) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        wc.lpszClassName, L"About PlatoLives",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        px, py, win_w, win_h,
        hParent, NULL, hInst, NULL
    );
    if (!hwnd) return;
    g_about.hwnd = hwnd;

    /* DWM Immersive Dark Mode con titolo Orange Plasma */
    HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
    if (hDwm) {
        typedef HRESULT (WINAPI *fnDwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
        fnDwmSetWindowAttribute pDwm = (fnDwmSetWindowAttribute)GetProcAddress(hDwm, "DwmSetWindowAttribute");
        if (pDwm) {
            BOOL dark = TRUE;
            COLORREF cap = PLATO_BG_DARK;
            COLORREF txt = PLATO_PLASMA_COLOR;
            pDwm(hwnd, 20, &dark, sizeof(dark));
            pDwm(hwnd, 35, &cap, sizeof(cap));
            pDwm(hwnd, 36, &txt, sizeof(txt));
        }
        FreeLibrary(hDwm);
    }

    g_about.hBtnClose = CreateWindowExW(
        0, L"BUTTON", L"Close",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        (client_w - 120) / 2, 375, 120, 32,
        hwnd, (HMENU)(INT_PTR)IDC_ABOUT_BTN_CLOSE, hInst, NULL
    );

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
}
