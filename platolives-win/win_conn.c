#include "win_theme.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>
#include "win_conn.h"

#define IDC_CONN_BTN_ACTION 701
#define IDC_CONN_BTN_RETRY  702
#define WM_CONN_DONE (WM_APP + 101)

typedef enum {
    CONN_ST_IDLE = 0,
    CONN_ST_CONNECTING,
    CONN_ST_SUCCESS,
    CONN_ST_FAILED
} conn_st_t;

typedef struct {
    HWND hwnd;
    HWND hBtnAction;
    HWND hBtnRetry;
    HFONT hFontTitle;
    HFONT hFontBody;
    HFONT hFontMuted;
    plato_profile_t profile;
    plato_transport_t *transport;
    win_conn_done_cb done_cb;
    void *cb_data;
    volatile conn_st_t state;
    pthread_t worker_th;
    bool worker_active;
    int anim_step;
} win_conn_ctx_t;

static win_conn_ctx_t g_wc;

static void* win_conn_worker(void *arg) {
    (void)arg;
    bool ok = plato_transport_connect(g_wc.transport, g_wc.profile.host, g_wc.profile.port);
    if (g_wc.hwnd && IsWindow(g_wc.hwnd)) {
        PostMessageW(g_wc.hwnd, WM_CONN_DONE, ok ? 1 : 0, 0);
    }
    return NULL;
}

static void start_win_worker(void) {
    if (g_wc.worker_active) {
        pthread_join(g_wc.worker_th, NULL);
        g_wc.worker_active = false;
    }
    g_wc.state = CONN_ST_CONNECTING;
    g_wc.anim_step = 0;

    if (g_wc.hBtnRetry && IsWindow(g_wc.hBtnRetry)) {
        ShowWindow(g_wc.hBtnRetry, SW_HIDE);
    }
    if (g_wc.hBtnAction && IsWindow(g_wc.hBtnAction)) {
        MoveWindow(g_wc.hBtnAction, (480 - 150) / 2, 155, 150, 32, TRUE);
        SetWindowTextW(g_wc.hBtnAction, L"Cancel (ESC)");
        InvalidateRect(g_wc.hBtnAction, NULL, TRUE);
    }

    g_wc.worker_active = true;
    pthread_create(&g_wc.worker_th, NULL, win_conn_worker, NULL);
    if (g_wc.hwnd && IsWindow(g_wc.hwnd)) {
        InvalidateRect(g_wc.hwnd, NULL, TRUE);
    }
}

static void draw_retro_button(DRAWITEMSTRUCT *dis, const WCHAR *txt) {
    win_theme_colors_t tc = win_get_current_theme_colors();
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    bool pushed = (dis->itemState & ODS_SELECTED) != 0;

    HBRUSH bg = CreateSolidBrush(pushed ? tc.hover_bg : tc.bg_dark);
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    HPEN pen = CreatePen(PS_SOLID, 1, tc.border);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, pushed ? tc.bright : tc.normal);
    HFONT oldFont = (HFONT)SelectObject(hdc, g_wc.hFontBody);

    DrawTextW(hdc, txt, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);
}

static LRESULT CALLBACK ConnWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HINSTANCE hInst = GetModuleHandleW(NULL);
            g_wc.hBtnRetry = CreateWindowExW(
                0, L"BUTTON", L"Retry (Enter)",
                WS_CHILD | BS_OWNERDRAW,
                85, 155, 145, 32,
                hwnd, (HMENU)IDC_CONN_BTN_RETRY, hInst, NULL
            );

            g_wc.hBtnAction = CreateWindowExW(
                0, L"BUTTON", L"Cancel (ESC)",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                (480 - 150) / 2, 155, 150, 32,
                hwnd, (HMENU)IDC_CONN_BTN_ACTION, hInst, NULL
            );

            SetTimer(hwnd, 1, 220, NULL);
            return 0;
        }

        case WM_TIMER: {
            g_wc.anim_step = (g_wc.anim_step + 1) % 4;
            if (g_wc.state == CONN_ST_CONNECTING) {
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
            if (dis->CtlID == IDC_CONN_BTN_RETRY) {
                draw_retro_button(dis, L"Retry (Enter)");
                return TRUE;
            } else if (dis->CtlID == IDC_CONN_BTN_ACTION) {
                const WCHAR *txt = (g_wc.state == CONN_ST_FAILED) ? L"Close (ESC)" : L"Cancel (ESC)";
                draw_retro_button(dis, txt);
                return TRUE;
            }
            break;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_CONN_BTN_RETRY) {
                start_win_worker();
                return 0;
            } else if (id == IDC_CONN_BTN_ACTION) {
                bool ok = (g_wc.state == CONN_ST_SUCCESS);
                win_conn_done_cb cb = g_wc.done_cb;
                void *data = g_wc.cb_data;
                win_conn_close();
                if (cb) cb(ok, data);
                return 0;
            }
            break;
        }

        case WM_CONN_DONE: {
            bool ok = (wParam != 0);
            if (ok) {
                g_wc.state = CONN_ST_SUCCESS;
                win_conn_done_cb cb = g_wc.done_cb;
                void *data = g_wc.cb_data;
                win_conn_close();
                if (cb) cb(true, data);
            } else {
                g_wc.state = CONN_ST_FAILED;

                /* Mostra i due tasti: Retry a sinistra, Close a destra */
                if (g_wc.hBtnRetry && IsWindow(g_wc.hBtnRetry)) {
                    ShowWindow(g_wc.hBtnRetry, SW_SHOW);
                    InvalidateRect(g_wc.hBtnRetry, NULL, TRUE);
                }
                if (g_wc.hBtnAction && IsWindow(g_wc.hBtnAction)) {
                    MoveWindow(g_wc.hBtnAction, 250, 155, 145, 32, TRUE);
                    SetWindowTextW(g_wc.hBtnAction, L"Close (ESC)");
                    InvalidateRect(g_wc.hBtnAction, NULL, TRUE);
                }
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT clientRc;
            GetClientRect(hwnd, &clientRc);
            win_theme_colors_t tc = win_get_current_theme_colors();

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, clientRc.right, clientRc.bottom);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

            HBRUSH bgDark = CreateSolidBrush(tc.bg_dark);
            FillRect(memDC, &clientRc, bgDark);
            DeleteObject(bgDark);

            RECT boxRc = { 16, 16, clientRc.right - 16, clientRc.bottom - 16 };
            HBRUSH bgBox = CreateSolidBrush(tc.bg_box);
            FillRect(memDC, &boxRc, bgBox);
            DeleteObject(bgBox);

            HPEN penBorder = CreatePen(PS_SOLID, 1, tc.border);
            HPEN oldPen = (HPEN)SelectObject(memDC, penBorder);
            HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, GetStockObject(NULL_BRUSH));
            Rectangle(memDC, boxRc.left, boxRc.top, boxRc.right, boxRc.bottom);
            SelectObject(memDC, oldBrush);
            SelectObject(memDC, oldPen);
            DeleteObject(penBorder);

            SetBkMode(memDC, TRANSPARENT);

            if (g_wc.state == CONN_ST_CONNECTING) {
                SelectObject(memDC, g_wc.hFontTitle);
                SetTextColor(memDC, tc.bright);
                RECT rTitle = { 20, 28, clientRc.right - 20, 56 };
                DrawTextW(memDC, L"[ CONNECTING TO PLATO ]", -1, &rTitle, DT_CENTER | DT_SINGLELINE);

                SelectObject(memDC, g_wc.hFontBody);
                SetTextColor(memDC, tc.normal);
                WCHAR pLine[128];
                swprintf(pLine, 128, L"Profile: %hs", g_wc.profile.name);
                TextOutW(memDC, 36, 68, pLine, (int)wcslen(pLine));

                SetTextColor(memDC, tc.muted);
                WCHAR sLine[128];
                swprintf(sLine, 128, L"Target:  %hs:%d", g_wc.profile.host, g_wc.profile.port);
                TextOutW(memDC, 36, 94, sLine, (int)wcslen(sLine));

                const WCHAR *animDots[4] = { L"[ . . . ]", L"[ * . . ]", L"[ * * . ]", L"[ * * * ]" };
                SetTextColor(memDC, tc.bright);
                TextOutW(memDC, 36, 120, animDots[g_wc.anim_step], (int)wcslen(animDots[g_wc.anim_step]));
            } else if (g_wc.state == CONN_ST_FAILED) {
                SelectObject(memDC, g_wc.hFontTitle);
                SetTextColor(memDC, tc.bright);
                RECT rTitle = { 20, 28, clientRc.right - 20, 56 };
                DrawTextW(memDC, L"[ CONNECTION FAILED ]", -1, &rTitle, DT_CENTER | DT_SINGLELINE);

                SelectObject(memDC, g_wc.hFontBody);
                SetTextColor(memDC, tc.muted);
                WCHAR sLine[128];
                swprintf(sLine, 128, L"Target: %hs:%d", g_wc.profile.host, g_wc.profile.port);
                TextOutW(memDC, 36, 68, sLine, (int)wcslen(sLine));

                SetTextColor(memDC, tc.normal);
                WCHAR eLine[128];
                const char *err = (g_wc.transport && g_wc.transport->last_error[0]) ? g_wc.transport->last_error : "Connection refused";
                swprintf(eLine, 128, L"Error:  %hs", err);
                TextOutW(memDC, 36, 94, eLine, (int)wcslen(eLine));

                SelectObject(memDC, g_wc.hFontMuted);
                SetTextColor(memDC, tc.dim);
                TextOutW(memDC, 36, 120, L"Press Retry or Close to select another profile.", 46);
            }

            BitBlt(hdc, 0, 0, clientRc.right, clientRc.bottom, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_KEYDOWN: {
            if (g_wc.state == CONN_ST_FAILED && (wParam == VK_RETURN)) {
                start_win_worker();
                return 0;
            } else if (wParam == VK_ESCAPE) {
                bool ok = (g_wc.state == CONN_ST_SUCCESS);
                win_conn_done_cb cb = g_wc.done_cb;
                void *data = g_wc.cb_data;
                win_conn_close();
                if (cb) cb(ok, data);
                return 0;
            }
            break;
        }

        case WM_CLOSE: {
            bool ok = (g_wc.state == CONN_ST_SUCCESS);
            win_conn_done_cb cb = g_wc.done_cb;
            void *data = g_wc.cb_data;
            win_conn_close();
            if (cb) cb(ok, data);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void win_conn_show(HWND hParent, const plato_profile_t *p, plato_transport_t *t, win_conn_done_cb cb, void *user_data) {
    if (!p || !t) return;
    win_conn_close();

    g_wc.profile = *p;
    g_wc.transport = t;
    g_wc.done_cb = cb;
    g_wc.cb_data = user_data;

    HINSTANCE hInst = GetModuleHandleW(NULL);
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ConnWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"PlatoLivesConnWnd";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    RECT pr;
    if (hParent && IsWindow(hParent)) {
        GetWindowRect(hParent, &pr);
    } else {
        pr.left = 100; pr.top = 100; pr.right = 800; pr.bottom = 700;
    }

    int w = 480;
    int h = 230;
    int x = pr.left + (pr.right - pr.left - w) / 2;
    int y = pr.top + (pr.bottom - pr.top - h) / 2;

    g_wc.hFontTitle = CreateFontW(19, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_wc.hFontBody  = CreateFontW(16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_wc.hFontMuted = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        wc.lpszClassName, L"PlatoLives - Connecting",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, w, h,
        hParent, NULL, hInst, NULL
    );
    if (!hwnd) return;
    g_wc.hwnd = hwnd;

    HMODULE hDwm = LoadLibraryA("dwmapi.dll");
    if (hDwm) {
        typedef HRESULT (WINAPI *fnDwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
        fnDwmSetWindowAttribute pDwm = (fnDwmSetWindowAttribute)GetProcAddress(hDwm, "DwmSetWindowAttribute");
        if (pDwm) {
            BOOL dark = TRUE;
            pDwm(hwnd, 20, &dark, sizeof(dark));
        }
        FreeLibrary(hDwm);
    }

    start_win_worker();
}

void win_conn_close(void) {
    if (g_wc.worker_active) {
        if (g_wc.transport && g_wc.state == CONN_ST_CONNECTING) {
            plato_transport_cancel_connect(g_wc.transport);
        }
        pthread_join(g_wc.worker_th, NULL);
        g_wc.worker_active = false;
    }

    if (g_wc.hwnd && IsWindow(g_wc.hwnd)) {
        DestroyWindow(g_wc.hwnd);
        g_wc.hwnd = NULL;
    }

    if (g_wc.hFontTitle) { DeleteObject(g_wc.hFontTitle); g_wc.hFontTitle = NULL; }
    if (g_wc.hFontBody)  { DeleteObject(g_wc.hFontBody);  g_wc.hFontBody = NULL; }
    if (g_wc.hFontMuted) { DeleteObject(g_wc.hFontMuted); g_wc.hFontMuted = NULL; }

    g_wc.state = CONN_ST_IDLE;
}

bool win_conn_is_open(void) {
    return (g_wc.hwnd != NULL && IsWindow(g_wc.hwnd));
}
