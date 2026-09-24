#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <initguid.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

#include "win_menu.h"
#include "win_profiles.h"
#include "win_text_buffer.h"
#include "win_keyref.h"
#include "win_about.h"
#include "plato/console_runner.h"
#include "plato/plato_terminal.h"
#include "plato/plato_transport.h"
#include "plato/plato_protocol.h"
#include "plato/plato_keyboard.h"
#include "plato/plato_ringbuf.h"
#include "plato/plato_graphics.h"
#include "plato/plato_optical.h"
#include "plato/plato_profile.h"
#include "plato/plato_script.h"

typedef HRESULT (WINAPI *pfn_D3DCompile)(
    LPCVOID pSrcData, SIZE_T SrcDataSize, LPCSTR pSourceName,
    const D3D_SHADER_MACRO *pDefines, ID3DInclude *pInclude,
    LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, UINT Flags2,
    ID3DBlob **ppCode, ID3DBlob **ppErrorMsgs
);

static pfn_D3DCompile g_D3DCompile = NULL;

static const char *g_hlsl_source =
"struct VS_OUTPUT {\n"
"    float4 pos : SV_POSITION;\n"
"    float2 uv  : TEXCOORD0;\n"
"};\n"
"\n"
"VS_OUTPUT vs_main(uint id : SV_VertexID) {\n"
"    VS_OUTPUT output;\n"
"    output.uv = float2((id << 1) & 2, id & 2);\n"
"    output.pos = float4(output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);\n"
"    return output;\n"
"}\n"
"\n"
"Texture2D g_texture : register(t0);\n"
"SamplerState g_sampler : register(s0);\n"
"\n"
"float4 ps_crisp(VS_OUTPUT input) : SV_TARGET {\n"
"    float4 c = g_texture.Sample(g_sampler, input.uv);\n"
"    return g_texture.Sample(g_sampler, input.uv);\n"
"}\n";

typedef struct {
    HWND hwnd;
    UINT width;
    UINT height;
    ID3D11Device *device;
    ID3D11DeviceContext *context;
    IDXGISwapChain *swapchain;
    ID3D11RenderTargetView *rtv;
    ID3D11VertexShader *vs;
    ID3D11PixelShader *ps_crisp;
    ID3D11Texture2D *texture;
    ID3D11ShaderResourceView *srv;
    ID3D11SamplerState *sampler;
    ID3D11RasterizerState *rasterizer;

    plato_terminal_t terminal;
    plato_transport_t *transport;
    plato_ringbuf_t ringbuf;
    plato_keyboard_state_t keyboard_state;
    plato_profile_list_t profile_list;
    plato_script_runner_t *script_runner;
    int current_profile_idx;
    pthread_t net_thread;
    volatile bool running;

    uint8_t feed_buf[4096];
    size_t feed_len;
    size_t feed_pos;

    HMENU menu;
    int display_mode;
    int distortion;
    bool diag_log;
    bool renderer_log;
    bool paste_cancelled;
    bool is_fullscreen;
    bool show_fps_hud;
    WINDOWPLACEMENT prev_placement;

    uint32_t *optical_fb;
    plato_optical_t *optical;
    ID3D11SamplerState *sampler_linear;
    ID3D11SamplerState *sampler_point;

    /* Risorse D3D11 dedicate per HUD Overlay indipendente */
    ID3D11Texture2D *hud_texture;
    ID3D11ShaderResourceView *hud_srv;
    ID3D11BlendState *blend_alpha;
    uint32_t hud_fb[320 * 160];
} win_app_t;

#define WIN_HUD_WIDTH  320
#define WIN_HUD_HEIGHT 160

static win_app_t g_app;
static void win_apply_dark_theme(HWND hwnd) {
    /* 1. uxtheme.dll: Attiva la modalita' scura per il processo, la barra menu e i popup */
    HMODULE hUxTheme = LoadLibraryW(L"uxtheme.dll");
    if (hUxTheme) {
        typedef enum { APPMODE_DEFAULT = 0, APPMODE_ALLOWDARK = 1, APPMODE_FORCEDARK = 2 } PreferredAppMode;
        typedef PreferredAppMode (WINAPI *fnSetPreferredAppMode)(PreferredAppMode);
        typedef BOOL (WINAPI *fnAllowDarkModeForWindow)(HWND, BOOL);
        typedef void (WINAPI *fnFlushMenuThemes)(void);

        fnSetPreferredAppMode pSetPreferredAppMode = (fnSetPreferredAppMode)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(135));
        fnAllowDarkModeForWindow pAllowDarkModeForWindow = (fnAllowDarkModeForWindow)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(132));
        fnFlushMenuThemes pFlushMenuThemes = (fnFlushMenuThemes)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(136));

        if (pSetPreferredAppMode) pSetPreferredAppMode(APPMODE_FORCEDARK);
        if (pAllowDarkModeForWindow && hwnd) pAllowDarkModeForWindow(hwnd, TRUE);
        if (pFlushMenuThemes) pFlushMenuThemes();

        typedef HRESULT (WINAPI *fnSetWindowTheme)(HWND, LPCWSTR, LPCWSTR);
        fnSetWindowTheme pSetWindowTheme = (fnSetWindowTheme)GetProcAddress(hUxTheme, "SetWindowTheme");
        if (pSetWindowTheme && hwnd) pSetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);

        FreeLibrary(hUxTheme);
    }

    if (!hwnd) return;

    /* 2. dwmapi.dll: Barra del titolo immersiva scura con accenti Ambra PLATO */
    HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
    if (hDwm) {
        typedef HRESULT (WINAPI *fnDwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
        fnDwmSetWindowAttribute pDwmSetWindowAttribute = (fnDwmSetWindowAttribute)GetProcAddress(hDwm, "DwmSetWindowAttribute");
        if (pDwmSetWindowAttribute) {
            BOOL darkMode = TRUE;
            /* DWMWA_USE_IMMERSIVE_DARK_MODE (20 su Win11/Win10 2004+, 19 su Win10 1809) */
            pDwmSetWindowAttribute(hwnd, 20, &darkMode, sizeof(darkMode));
            pDwmSetWindowAttribute(hwnd, 19, &darkMode, sizeof(darkMode));

            /* Colori personalizzati Windows 11 (Build 22000+) */
            COLORREF captionColor = RGB(16, 12, 10);      /* Sfondo barra del titolo: Nero profondo */
            COLORREF textColor    = RGB(255, 110, 0);     /* Testo del titolo: Orange Plasma CDC PLATO IV */

            pDwmSetWindowAttribute(hwnd, 35, &captionColor, sizeof(captionColor)); /* DWMWA_CAPTION_COLOR */
            pDwmSetWindowAttribute(hwnd, 36, &textColor, sizeof(textColor));       /* DWMWA_TEXT_COLOR */
        }
        FreeLibrary(hDwm);
    }
}

static void win_toggle_fullscreen(win_app_t *app);
static void win_connect_profile(const plato_profile_t *p, void *user_data);
static void win_launch_instance(const char *profile_name);
static void win_setup_ownerdraw_menu(HMENU hMenu, bool is_top_bar);

static void win_beep_callback(void *context) {
    (void)context;
    MessageBeep(MB_OK);
}

static double win_get_time_sec(void) {
    static LARGE_INTEGER freq;
    static BOOL init = FALSE;
    if (!init) {
        QueryPerformanceFrequency(&freq);
        init = TRUE;
    }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart / (double)freq.QuadPart;
}

static void* win_net_worker(void *arg) {
    win_app_t *app = (win_app_t *)arg;
    uint8_t temp_buf[4096];

    while (app->running) {
        if (app->transport && app->transport->connected) {
            int n = plato_transport_recv(app->transport, temp_buf, sizeof(temp_buf));
            if (n > 0) {
                plato_ringbuf_write(&app->ringbuf, temp_buf, (size_t)n);
            } else if (n == 0) {
                plato_transport_disconnect(app->transport);
                Sleep(30);
            }
        } else {
            Sleep(20);
        }
    }
    return NULL;
}

static void win_update_render_target(win_app_t *app, UINT width, UINT height) {
    if (app->rtv) {
        ID3D11RenderTargetView_Release(app->rtv);
        app->rtv = NULL;
    }
    if (app->swapchain) {
        IDXGISwapChain_ResizeBuffers(app->swapchain, 0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        ID3D11Texture2D *backBuffer = NULL;
        IDXGISwapChain_GetBuffer(app->swapchain, 0, &IID_ID3D11Texture2D, (void**)&backBuffer);
        if (backBuffer) {
            ID3D11Device_CreateRenderTargetView(app->device, (ID3D11Resource*)backBuffer, NULL, &app->rtv);
            ID3D11Texture2D_Release(backBuffer);
        }
    }
    app->width = width;
    app->height = height;
}

static bool win_init_d3d11(win_app_t *app, HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    DXGI_SWAP_CHAIN_DESC scd = {0};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = width;
    scd.BufferDesc.Height = height;
    scd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
        featureLevels, 2, D3D11_SDK_VERSION,
        &scd, &app->swapchain, &app->device, NULL, &app->context
    );
    if (FAILED(hr)) return false;

    win_update_render_target(app, width, height);

    HMODULE d3d_mod = LoadLibraryA("d3dcompiler_47.dll");
    if (!d3d_mod) return false;
    g_D3DCompile = (pfn_D3DCompile)(void*)GetProcAddress(d3d_mod, "D3DCompile");
    if (!g_D3DCompile) return false;

    ID3DBlob *vsBlob = NULL, *errBlob = NULL;
    hr = g_D3DCompile(g_hlsl_source, strlen(g_hlsl_source), NULL, NULL, NULL, "vs_main", "vs_4_0", 0, 0, &vsBlob, &errBlob);
    if (FAILED(hr)) {
        if (errBlob) ID3D10Blob_Release(errBlob);
        return false;
    }
    ID3D11Device_CreateVertexShader(app->device, ID3D10Blob_GetBufferPointer(vsBlob), ID3D10Blob_GetBufferSize(vsBlob), NULL, &app->vs);
    ID3D10Blob_Release(vsBlob);

    ID3DBlob *psBlob = NULL;
    hr = g_D3DCompile(g_hlsl_source, strlen(g_hlsl_source), NULL, NULL, NULL, "ps_crisp", "ps_4_0", 0, 0, &psBlob, &errBlob);
    if (FAILED(hr)) {
        if (errBlob) ID3D10Blob_Release(errBlob);
        return false;
    }
    ID3D11Device_CreatePixelShader(app->device, ID3D10Blob_GetBufferPointer(psBlob), ID3D10Blob_GetBufferSize(psBlob), NULL, &app->ps_crisp);
    ID3D10Blob_Release(psBlob);

    D3D11_TEXTURE2D_DESC td = {0};
    td.Width = PLATO_OPTICAL_WIDTH;
    td.Height = PLATO_OPTICAL_HEIGHT;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DYNAMIC;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = ID3D11Device_CreateTexture2D(app->device, &td, NULL, &app->texture);
    if (FAILED(hr)) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {0};
    srvd.Format = td.Format;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels = 1;
    ID3D11Device_CreateShaderResourceView(app->device, (ID3D11Resource*)app->texture, &srvd, &app->srv);

    D3D11_SAMPLER_DESC smpd = {0};
    smpd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    smpd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    smpd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    smpd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    smpd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    ID3D11Device_CreateSamplerState(app->device, &smpd, &app->sampler_point);

    smpd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    ID3D11Device_CreateSamplerState(app->device, &smpd, &app->sampler_linear);

    D3D11_RASTERIZER_DESC rd = {0};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    ID3D11Device_CreateRasterizerState(app->device, &rd, &app->rasterizer);

    /* Creazione Texture Overlay HUD e Alpha Blend State */
    D3D11_TEXTURE2D_DESC htd = {0};
    htd.Width = WIN_HUD_WIDTH;
    htd.Height = WIN_HUD_HEIGHT;
    htd.MipLevels = 1;
    htd.ArraySize = 1;
    htd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    htd.SampleDesc.Count = 1;
    htd.Usage = D3D11_USAGE_DYNAMIC;
    htd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    htd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ID3D11Device_CreateTexture2D(app->device, &htd, NULL, &app->hud_texture);
    if (app->hud_texture) {
        ID3D11Device_CreateShaderResourceView(app->device, (ID3D11Resource*)app->hud_texture, NULL, &app->hud_srv);
    }

    D3D11_BLEND_DESC bd = {0};
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    ID3D11Device_CreateBlendState(app->device, &bd, &app->blend_alpha);

    return true;
}


static double win_get_cpu_usage(void) {
    static ULARGE_INTEGER prev_idle, prev_kernel, prev_user;
    static bool first = true;
    FILETIME idle_ft, kernel_ft, user_ft;
    if (!GetSystemTimes(&idle_ft, &kernel_ft, &user_ft)) return 0.0;

    ULARGE_INTEGER idle, kernel, user;
    idle.LowPart = idle_ft.dwLowDateTime; idle.HighPart = idle_ft.dwHighDateTime;
    kernel.LowPart = kernel_ft.dwLowDateTime; kernel.HighPart = kernel_ft.dwHighDateTime;
    user.LowPart = user_ft.dwLowDateTime; user.HighPart = user_ft.dwHighDateTime;

    if (first) {
        prev_idle = idle; prev_kernel = kernel; prev_user = user;
        first = false;
        return 0.0;
    }

    ULONGLONG sys = (kernel.QuadPart - prev_kernel.QuadPart) + (user.QuadPart - prev_user.QuadPart);
    ULONGLONG idl = idle.QuadPart - prev_idle.QuadPart;

    prev_idle = idle; prev_kernel = kernel; prev_user = user;
    if (sys == 0) return 0.0;
    return ((double)(sys - idl) / (double)sys) * 100.0;
}

static void win_draw_hud_char(win_app_t *app, int x0, int y0, char c, uint32_t color) {
    uint8_t ch = (uint8_t)c;
    if (ch >= 128) ch = '?';
    const uint8_t *glyph = plato_font_get_glyph(&app->terminal.font, PLATO_CHARSET_M0, ch);
    if (!glyph) return;

    for (int r = 0; r < 16; r++) {
        uint8_t rowBits = glyph[r];
        if (rowBits == 0) continue;
        for (int b = 0; b < 8; b++) {
            if (rowBits & (0x80 >> b)) {
                int px = x0 + b;
                int py = y0 + r;
                if (px >= 0 && px < WIN_HUD_WIDTH && py >= 0 && py < WIN_HUD_HEIGHT) {
                    app->hud_fb[py * WIN_HUD_WIDTH + px] = color;
                }
            }
        }
    }
}

static void win_draw_hud_string(win_app_t *app, int x0, int y0, const char *str, uint32_t color) {
    int cx = x0;
    for (size_t i = 0; str[i]; i++) {
        if (str[i] == '\n') {
            cx = x0;
            y0 += 18;
        } else {
            win_draw_hud_char(app, cx, y0, str[i], color);
            cx += 9;
        }
    }
}

static void win_render_hud_overlay(win_app_t *app, double now_sec, bool is_animating, bool is_full_frame) {
    if (!app->show_fps_hud || !app->hud_texture) return;

    static double last_sample = 0.0;
    static uint32_t total_frames = 0, partial_frames = 0, full_frames = 0;
    static double fps_total = 0.0, fps_partial = 0.0, fps_full = 0.0;
    static double cpu_val = 0.0;

    if (is_animating) {
        total_frames++;
        if (is_full_frame) full_frames++; else partial_frames++;
    }

    if (last_sample == 0.0) {
        last_sample = now_sec;
    }

    if (now_sec - last_sample >= 0.5) {
        double elapsed = now_sec - last_sample;
        if (elapsed > 0.0) {
            fps_total = (double)total_frames / elapsed;
            fps_partial = (double)partial_frames / elapsed;
            fps_full = (double)full_frames / elapsed;
        }
        total_frames = partial_frames = full_frames = 0;
        last_sample = now_sec;
        cpu_val = win_get_cpu_usage();
    }

    char hud_text[512];
    char user_info[128] = "";
    if (app->terminal.user_name[0] && app->terminal.user_group[0]) {
        snprintf(user_info, sizeof(user_info), "USER    %s/%s (%s)\n",
                 app->terminal.user_name, app->terminal.user_group, app->terminal.user_station);
    } else if (app->terminal.user_station[0]) {
        snprintf(user_info, sizeof(user_info), "SLOT    %s\n", app->terminal.user_station);
    }

    int mode = app->profile_list.profiles[app->current_profile_idx].display_mode;
    if (mode == 1 || mode == 3) {
        snprintf(hud_text, sizeof(hud_text), "%sFPS     %5.1f\nCPU     %5.1f%%",
                 user_info, fps_total, cpu_val);
    } else {
        snprintf(hud_text, sizeof(hud_text), "%sFPS     %5.1f\nPARTIAL %5.1f\nFULL    %5.1f\nCPU     %5.1f%%",
                 user_info, fps_total, fps_partial, fps_full, cpu_val);
    }

    int max_cols = 0, lines = 1, cur_cols = 0;
    for (size_t i = 0; hud_text[i]; i++) {
        if (hud_text[i] == '\n') {
            if (cur_cols > max_cols) max_cols = cur_cols;
            cur_cols = 0;
            lines++;
        } else {
            cur_cols++;
        }
    }
    if (cur_cols > max_cols) max_cols = cur_cols;

    int box_w = max_cols * 9 + 24;
    int box_h = lines * 18 + 18;
    if (box_w > WIN_HUD_WIDTH) box_w = WIN_HUD_WIDTH;
    if (box_h > WIN_HUD_HEIGHT) box_h = WIN_HUD_HEIGHT;

    /* Pulisci texture HUD con trasparenza completa */
    memset(app->hud_fb, 0, sizeof(app->hud_fb));

    /* Sfondo scuro semitrasparente (Alpha = ~78%) */
    uint32_t bg_color = 0xC8060301u;
    for (int y = 0; y < box_h; y++) {
        for (int x = 0; x < box_w; x++) {
            app->hud_fb[y * WIN_HUD_WIDTH + x] = bg_color;
        }
    }

    /* Bordo sottile ambra scuro */
    uint32_t borderColor = 0xFF50280Au;
    for (int x = 0; x < box_w; x++) {
        app->hud_fb[x] = borderColor;
        app->hud_fb[(box_h - 1) * WIN_HUD_WIDTH + x] = borderColor;
    }
    for (int y = 0; y < box_h; y++) {
        app->hud_fb[y * WIN_HUD_WIDTH] = borderColor;
        app->hud_fb[y * WIN_HUD_WIDTH + box_w - 1] = borderColor;
    }

    /* Testo Oro/Ambra Caldo BGRA (#FFB428) */
    uint32_t textColor = 0xFFFFB428u;
    win_draw_hud_string(app, 12, 10, hud_text, textColor);

    /* Upload rapido alla GPU */
    D3D11_MAPPED_SUBRESOURCE hmap;
    if (SUCCEEDED(ID3D11DeviceContext_Map(app->context, (ID3D11Resource*)app->hud_texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &hmap))) {
        for (int y = 0; y < WIN_HUD_HEIGHT; y++) {
            memcpy((uint8_t*)hmap.pData + y * hmap.RowPitch,
                   app->hud_fb + y * WIN_HUD_WIDTH,
                   WIN_HUD_WIDTH * sizeof(uint32_t));
        }
        ID3D11DeviceContext_Unmap(app->context, (ID3D11Resource*)app->hud_texture, 0);
    }
}
static void win_render_frame(win_app_t *app) {
    if (!app->context || !app->rtv) return;

    while (1) {
        if (app->feed_pos >= app->feed_len) {
            app->feed_pos = 0;
            app->feed_len = plato_ringbuf_read(&app->ringbuf, app->feed_buf, sizeof(app->feed_buf));
            if (app->feed_len == 0) break;
        }
        app->terminal.delay_requested = false;
        size_t remaining = app->feed_len - app->feed_pos;
        size_t consumed = plato_terminal_feed(&app->terminal, &app->feed_buf[app->feed_pos], remaining);
        app->feed_pos += consumed;

        if (app->terminal.delay_requested) {
            app->terminal.delay_requested = false;
            break;
        }
    }

    plato_keyboard_poll(&app->keyboard_state, &app->terminal, win_get_time_sec());
    
    plato_profile_t *cur_p = &app->profile_list.profiles[app->current_profile_idx];
    double now = win_get_time_sec();
    bool is_animating = plato_optical_render(app->optical, &app->terminal, cur_p, now, app->optical_fb);

    static bool s_first_frame = true;
    if (is_animating || s_first_frame) {
        s_first_frame = false;
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(ID3D11DeviceContext_Map(app->context, (ID3D11Resource*)app->texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            for (int y = 0; y < PLATO_OPTICAL_HEIGHT; y++) {
                memcpy((uint8_t*)mapped.pData + y * mapped.RowPitch,
                       app->optical_fb + y * PLATO_OPTICAL_WIDTH,
                       PLATO_OPTICAL_WIDTH * sizeof(uint32_t));
            }
            ID3D11DeviceContext_Unmap(app->context, (ID3D11Resource*)app->texture, 0);
        }
    }

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    ID3D11DeviceContext_ClearRenderTargetView(app->context, app->rtv, clearColor);

    float scale = (float)app->width / (float)PLATO_OPTICAL_WIDTH;
    if ((float)app->height / (float)PLATO_OPTICAL_HEIGHT < scale) {
        scale = (float)app->height / (float)PLATO_OPTICAL_HEIGHT;
    }
    float viewW = PLATO_OPTICAL_WIDTH * scale;
    float viewH = PLATO_OPTICAL_HEIGHT * scale;
    float viewX = ((float)app->width - viewW) * 0.5f;
    float viewY = ((float)app->height - viewH) * 0.5f;

    D3D11_VIEWPORT vp = {0};
    vp.TopLeftX = viewX;
    vp.TopLeftY = viewY;
    vp.Width = viewW;
    vp.Height = viewH;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    ID3D11DeviceContext_RSSetViewports(app->context, 1, &vp);
    ID3D11DeviceContext_RSSetState(app->context, app->rasterizer);
    ID3D11DeviceContext_OMSetRenderTargets(app->context, 1, &app->rtv, NULL);

    ID3D11DeviceContext_IASetPrimitiveTopology(app->context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(app->context, app->vs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(app->context, app->ps_crisp, NULL, 0);
    ID3D11DeviceContext_PSSetShaderResources(app->context, 0, 1, &app->srv);
    int cur_mode = app->profile_list.profiles[app->current_profile_idx].display_mode;
    ID3D11SamplerState *cur_sampler = (cur_mode == 1 || cur_mode == 3) ? app->sampler_point : app->sampler_linear;
    ID3D11DeviceContext_PSSetSamplers(app->context, 0, 1, &cur_sampler);

    ID3D11DeviceContext_Draw(app->context, 3, 0);

    /* Pass di rendering dedicato per l'HUD Overlay (Disaccoppiato e fuori dall'area terminale) */
    if (app->show_fps_hud && app->hud_texture && app->hud_srv) {
        win_render_hud_overlay(app, now, is_animating, plato_optical_is_full_frame(app->optical));

        float freeRight = (float)app->width - (viewX + viewW);
        float hud_x = (freeRight >= (float)WIN_HUD_WIDTH + 20.0f)
                      ? ((float)app->width - (float)WIN_HUD_WIDTH - 20.0f)
                      : ((float)app->width - (float)WIN_HUD_WIDTH - 16.0f);
        float hud_y = (freeRight >= (float)WIN_HUD_WIDTH + 20.0f) ? 24.0f : 16.0f;

        D3D11_VIEWPORT hud_vp = {
            .TopLeftX = hud_x,
            .TopLeftY = hud_y,
            .Width = (float)WIN_HUD_WIDTH,
            .Height = (float)WIN_HUD_HEIGHT,
            .MinDepth = 0.0f,
            .MaxDepth = 1.0f
        };
        ID3D11DeviceContext_RSSetViewports(app->context, 1, &hud_vp);

        float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        if (app->blend_alpha) {
            ID3D11DeviceContext_OMSetBlendState(app->context, app->blend_alpha, blendFactor, 0xffffffff);
        }
        ID3D11DeviceContext_PSSetShaderResources(app->context, 0, 1, &app->hud_srv);
        ID3D11DeviceContext_PSSetSamplers(app->context, 0, 1, &app->sampler_point);
        ID3D11DeviceContext_Draw(app->context, 3, 0);
        ID3D11DeviceContext_OMSetBlendState(app->context, NULL, NULL, 0xffffffff);
    }

    IDXGISwapChain_Present(app->swapchain, 1, 0);
}

static void win_toggle_fullscreen(win_app_t *app) {
    DWORD dwStyle = GetWindowLong(app->hwnd, GWL_STYLE);
    if (!app->is_fullscreen) {
        MONITORINFO mi = { sizeof(mi) };
        app->prev_placement.length = sizeof(WINDOWPLACEMENT);
        if (GetWindowPlacement(app->hwnd, &app->prev_placement) &&
            GetMonitorInfo(MonitorFromWindow(app->hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            /* Rimuove la barra dei menu in Full Screen */
            SetMenu(app->hwnd, NULL);
            SetWindowLong(app->hwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(app->hwnd, HWND_TOP,
                         mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            app->is_fullscreen = true;
        }
    } else {
        SetWindowLong(app->hwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(app->hwnd, &app->prev_placement);
        /* Ripristina la barra dei menu in modalità finestra */
        SetMenu(app->hwnd, app->menu);
        SetWindowPos(app->hwnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        app->is_fullscreen = false;
    }
    plato_profile_t *cur_p = &app->profile_list.profiles[app->current_profile_idx];
    win_menu_update_state(app->menu, cur_p,
                         app->transport && app->transport->connected,
                         app->diag_log, app->renderer_log, app->is_fullscreen,
                         &app->profile_list, app->current_profile_idx);
}

static void win_copy_text_to_clipboard(win_app_t *app, bool compact) {
    char text[4096];
    size_t len = plato_terminal_get_text_area(&app->terminal, 0, 0, PLATO_COLS - 1, PLATO_ROWS - 1, compact, text, sizeof(text));
    if (len == 0) return;

    FILE *f = fopen("screen.txt", "w");
    if (f) {
        fwrite(text, 1, len, f);
        fclose(f);
    }

    if (OpenClipboard(app->hwnd)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len + 1);
        if (hMem) {
            char *p = (char*)GlobalLock(hMem);
            memcpy(p, text, len);
            p[len] = '\0';
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
        CloseClipboard();
    }
}

static void win_copy_screen_text(win_app_t *app) {
    win_copy_text_to_clipboard(app, false);
}

static void win_copy_screen_image(win_app_t *app) {
    if (!app || !app->optical_fb) return;
    size_t header_size = sizeof(BITMAPINFOHEADER);
    size_t image_size = PLATO_OPTICAL_WIDTH * PLATO_OPTICAL_HEIGHT * sizeof(uint32_t);
    size_t total_size = header_size + image_size;

    if (OpenClipboard(app->hwnd)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, total_size);
        if (hMem) {
            uint8_t *p = (uint8_t*)GlobalLock(hMem);
            BITMAPINFOHEADER *bih = (BITMAPINFOHEADER*)p;
            bih->biSize = sizeof(BITMAPINFOHEADER);
            bih->biWidth = PLATO_OPTICAL_WIDTH;
            bih->biHeight = -((LONG)PLATO_OPTICAL_HEIGHT); /* top-down DIB */
            bih->biPlanes = 1;
            bih->biBitCount = 32;
            bih->biCompression = BI_RGB;
            bih->biSizeImage = (DWORD)image_size;
            bih->biXPelsPerMeter = 0;
            bih->biYPelsPerMeter = 0;
            bih->biClrUsed = 0;
            bih->biClrImportant = 0;
            memcpy(p + header_size, app->optical_fb, image_size);
            GlobalUnlock(hMem);
            SetClipboardData(CF_DIB, hMem);
        }
        CloseClipboard();
    }
}

/* Copia Grafica dello Schermo (CF_DIB) per incollare l'immagine reale */
static void win_copy_screenshot_graphic(win_app_t *app) {
    size_t pixel_bytes = PLATO_OPTICAL_WIDTH * PLATO_OPTICAL_HEIGHT * sizeof(uint32_t);
    size_t dib_size = sizeof(BITMAPINFOHEADER) + pixel_bytes;

    if (!OpenClipboard(app->hwnd)) return;
    EmptyClipboard();

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, dib_size);
    if (hMem) {
        uint8_t *p = (uint8_t*)GlobalLock(hMem);
        BITMAPINFOHEADER *bih = (BITMAPINFOHEADER*)p;
        bih->biSize = sizeof(BITMAPINFOHEADER);
        bih->biWidth = PLATO_OPTICAL_WIDTH;
        bih->biHeight = PLATO_OPTICAL_HEIGHT; /* Bottom-up */
        bih->biPlanes = 1;
        bih->biBitCount = 32;
        bih->biCompression = BI_RGB;
        bih->biSizeImage = (DWORD)pixel_bytes;
        bih->biXPelsPerMeter = 2835;
        bih->biYPelsPerMeter = 2835;
        bih->biClrUsed = 0;
        bih->biClrImportant = 0;

        uint32_t *dest_pixels = (uint32_t*)(p + sizeof(BITMAPINFOHEADER));
        for (int y = 0; y < PLATO_OPTICAL_HEIGHT; y++) {
            memcpy(dest_pixels + y * PLATO_OPTICAL_WIDTH,
                   app->optical_fb + (PLATO_OPTICAL_HEIGHT - 1 - y) * PLATO_OPTICAL_WIDTH,
                   PLATO_OPTICAL_WIDTH * sizeof(uint32_t));
        }

        GlobalUnlock(hMem);
        SetClipboardData(CF_DIB, hMem);
    }
    CloseClipboard();
}

static void win_save_screenshot_bmp(win_app_t *app) {
    size_t pixel_bytes = PLATO_OPTICAL_WIDTH * PLATO_OPTICAL_HEIGHT * sizeof(uint32_t);
    BITMAPFILEHEADER bfh = {0};
    bfh.bfType = 0x4D42; /* "BM" */
    bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + pixel_bytes;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    BITMAPINFOHEADER bih = {0};
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = PLATO_OPTICAL_WIDTH;
    bih.biHeight = PLATO_OPTICAL_HEIGHT;
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;

    FILE *f = fopen("screen.bmp", "wb");
    if (!f) return;
    fwrite(&bfh, sizeof(bfh), 1, f);
    fwrite(&bih, sizeof(bih), 1, f);

    for (int y = 0; y < PLATO_OPTICAL_HEIGHT; y++) {
        fwrite(app->optical_fb + (PLATO_OPTICAL_HEIGHT - 1 - y) * PLATO_OPTICAL_WIDTH, sizeof(uint32_t), PLATO_OPTICAL_WIDTH, f);
    }
    fclose(f);
}

typedef struct {
    win_app_t *app;
    char *text;
} paste_task_t;

static void* win_paste_worker(void *arg) {
    paste_task_t *task = (paste_task_t *)arg;
    win_app_t *app = task->app;
    char *text = task->text;

    size_t len = strlen(text);
    for (size_t i = 0; i < len && app->running && !app->paste_cancelled && app->transport && app->transport->connected; i++) {
        char ch = text[i];
        if (ch == '\r') continue;
        if (ch == '\n') {
            plato_protocol_send_key(&app->terminal, plato_keyboard_keycode(PLATO_KEY_NEXT, false));
            for (int s = 0; s < 10 && !app->paste_cancelled && app->running; s++) {
                Sleep(50);
            }
        } else if ((unsigned char)ch >= 32 && (unsigned char)ch <= 126) {
            uint8_t b = (uint8_t)ch;
            plato_transport_send(app->transport, &b, 1);
            for (int s = 0; s < 5 && !app->paste_cancelled && app->running; s++) {
                Sleep(50);
            }
        }
    }

    free(text);
    free(task);
    return NULL;
}

static void win_paste_clipboard_text(win_app_t *app) {
    if (!app->transport || !app->transport->connected) return;
    if (OpenClipboard(app->hwnd)) {
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData) {
            char *p = (char*)GlobalLock(hData);
            if (p && strlen(p) > 0) {
                paste_task_t *task = (paste_task_t *)malloc(sizeof(paste_task_t));
                task->app = app;
                task->text = strdup(p);
                GlobalUnlock(hData);
                CloseClipboard();

                pthread_t paste_th;
                pthread_create(&paste_th, NULL, win_paste_worker, task);
                pthread_detach(paste_th);
                return;
            }
            GlobalUnlock(hData);
        }
        CloseClipboard();
    }
}

static void win_launch_instance(const char *profile_name) {
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(NULL, exe_path, MAX_PATH);

    wchar_t cmd[MAX_PATH * 2];
    if (profile_name && strlen(profile_name) > 0) {
        wchar_t wprof[128];
        MultiByteToWideChar(CP_UTF8, 0, profile_name, -1, wprof, 128);
        swprintf(cmd, MAX_PATH * 2, L"\"%s\" --profile \"%s\"", exe_path, wprof);
    } else {
        swprintf(cmd, MAX_PATH * 2, L"\"%s\"", exe_path);
    }

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {0};
    CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

static void win_clear_screen(win_app_t *app) {
    if (!app) return;
    plato_fb_clear(&app->terminal.fb);
    app->terminal.x = 0;
    app->terminal.y = 496;
    app->terminal.margin_x = 0;
    if (app->optical_fb) {
        memset(app->optical_fb, 0, PLATO_OPTICAL_WIDTH * PLATO_OPTICAL_HEIGHT * sizeof(uint32_t));
    }
    if (app->optical) {
        plato_optical_invalidate(app->optical);
    }
    app->feed_pos = 0;
    app->feed_len = 0;
    pthread_mutex_lock(&app->ringbuf.mutex);
    app->ringbuf.head = 0;
    app->ringbuf.tail = 0;
    pthread_mutex_unlock(&app->ringbuf.mutex);
}

static void win_connect_profile(const plato_profile_t *p, void *user_data) {
    win_app_t *app = (win_app_t *)user_data;
    if (!app || !p) return;

    if (app->transport && app->transport->connected) {
        plato_script_stop(app->script_runner);
        plato_transport_disconnect(app->transport);
    }
    win_clear_screen(app);

    for (size_t i = 0; i < app->profile_list.count; i++) {
        if (&app->profile_list.profiles[i] == p || strcmp(app->profile_list.profiles[i].name, p->name) == 0) {
            app->current_profile_idx = (int)i;
            break;
        }
    }

    app->display_mode = p->display_mode;
    app->distortion = p->plasma_distortion;
    plato_terminal_set_color_mode(&app->terminal, (p->display_mode == 3 || p->display_mode == 4));
    plato_optical_invalidate(app->optical);

    if (p->full_screen != app->is_fullscreen) {
        win_toggle_fullscreen(app);
    }

    plato_transport_connect(app->transport, p->host, p->port);

    win_menu_update_state(app->menu, &app->profile_list.profiles[app->current_profile_idx],
                         app->transport && app->transport->connected,
                         app->diag_log, app->renderer_log, app->is_fullscreen,
                         &app->profile_list, app->current_profile_idx);
    win_setup_ownerdraw_menu(app->menu, true);

    if (p->startup_script_enabled && strlen(p->startup_script) > 0) {
        plato_script_start(app->script_runner, p->startup_script);
    }
}

static void win_handle_keydown(win_app_t *app, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    plato_key_event_t ev = {0};
    ev.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    ev.ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    ev.alt   = (GetKeyState(VK_MENU) & 0x8000) != 0;

    if (ev.alt && wParam == VK_RETURN) { win_toggle_fullscreen(app); return; }
    if (ev.ctrl && !ev.shift && !ev.alt) {
        if (wParam == 'N') { win_launch_instance(NULL); return; }
        if (wParam == 'C') { win_copy_text_to_clipboard(app, false); return; }
        if (wParam == 'Y') { win_copy_screen_text(app); return; }
        if (wParam == 'K') { win_keyref_show(app->hwnd); return; }
        if (wParam == 'V') { app->paste_cancelled = false; win_paste_clipboard_text(app); return; }
        if (wParam == VK_OEM_PERIOD) { app->paste_cancelled = true; return; }
    }
    if (ev.ctrl && ev.shift && !ev.alt) {
        if (wParam == 'C') { win_copy_text_to_clipboard(app, true); return; }
        if (wParam == 'T') { win_text_buffer_show(app->hwnd, &app->terminal); return; }
    }

    switch (wParam) {
        case VK_F1:  ev.vkey = PLATO_VKEY_F1; break;
        case VK_F2:  ev.vkey = PLATO_VKEY_F2; break;
        case VK_F3:  ev.vkey = PLATO_VKEY_F3; break;
        case VK_F4:  ev.vkey = PLATO_VKEY_F4; break;
        case VK_F5:  ev.vkey = PLATO_VKEY_F5; break;
        case VK_F6:  ev.vkey = PLATO_VKEY_F6; break;
        case VK_F7:  ev.vkey = PLATO_VKEY_F7; break;
        case VK_F8:  ev.vkey = PLATO_VKEY_F8; break;
        case VK_F9:  ev.vkey = PLATO_VKEY_F9; break;
        case VK_F10: ev.vkey = PLATO_VKEY_F10; break;
        case VK_F11: win_toggle_fullscreen(app); return;
        case VK_F12: app->show_fps_hud = !app->show_fps_hud; plato_optical_invalidate(app->optical); return;
        case VK_LEFT:   ev.vkey = PLATO_VKEY_LEFT; break;
        case VK_RIGHT:  ev.vkey = PLATO_VKEY_RIGHT; break;
        case VK_UP:     ev.vkey = PLATO_VKEY_UP; break;
        case VK_DOWN:   ev.vkey = PLATO_VKEY_DOWN; break;
        case VK_PRIOR:  ev.vkey = PLATO_VKEY_PAGEUP; break;
        case VK_NEXT:   ev.vkey = PLATO_VKEY_PAGEDOWN; break;
        case VK_RETURN: ev.vkey = PLATO_VKEY_RETURN; break;
        case VK_BACK:   ev.vkey = PLATO_VKEY_BACKSPACE; break;
        case VK_ESCAPE: ev.vkey = PLATO_VKEY_ESCAPE; break;
        case VK_TAB:    ev.vkey = PLATO_VKEY_TAB; break;
        default:
            if (ev.ctrl && wParam >= 'A' && wParam <= 'Z') {
                ev.unmod_codepoint = (uint32_t)(wParam - 'A' + 'a');
            } else if (ev.ctrl && wParam >= '0' && wParam <= '9') {
                ev.unmod_codepoint = (uint32_t)wParam;
            }
            break;
    }

    if (ev.vkey != PLATO_VKEY_NONE || ev.unmod_codepoint != 0) {
        plato_keyboard_dispatch(&app->keyboard_state, &app->terminal, &ev, win_get_time_sec());
    }
}

static void win_handle_char(win_app_t *app, WPARAM wParam) {
    if (wParam >= 32 && wParam <= 126) {
        plato_key_event_t ev = {0};
        ev.codepoint = (uint32_t)wParam;
        plato_keyboard_dispatch(&app->keyboard_state, &app->terminal, &ev, win_get_time_sec());
    }
}

static void win_handle_lbuttondown(win_app_t *app, LPARAM lParam) {
    if (!app->transport || !app->transport->connected) return;
    int mx = GET_X_LPARAM(lParam);
    int my = GET_Y_LPARAM(lParam);

    float scale = (float)app->width / (float)PLATO_WIDTH;
    if ((float)app->height / (float)PLATO_HEIGHT < scale) {
        scale = (float)app->height / (float)PLATO_HEIGHT;
    }
    float viewW = PLATO_WIDTH * scale;
    float viewH = PLATO_HEIGHT * scale;
    float viewX = ((float)app->width - viewW) * 0.5f;
    float viewY = ((float)app->height - viewH) * 0.5f;

    if (mx >= viewX && mx < viewX + viewW && my >= viewY && my < viewY + viewH) {
        int px = (int)((mx - viewX) / scale);
        int py = (int)((my - viewY) / scale);
        int plato_x = px;
        int plato_y = 511 - py;

        if (plato_x < 0) plato_x = 0;
        if (plato_x >= PLATO_WIDTH) plato_x = PLATO_WIDTH - 1;
        if (plato_y < 0) plato_y = 0;
        if (plato_y >= PLATO_HEIGHT) plato_y = PLATO_HEIGHT - 1;

        plato_protocol_send_touch(&app->terminal, (int16_t)plato_x, (int16_t)plato_y);
    }
}

/* Costanti cromatiche unificate Orange Plasma (#FF6E00) identiche al terminale */
#define PLATO_PLASMA_COLOR         RGB(255, 110, 0)
#define PLATO_PLASMA_BRIGHT        RGB(255, 160, 40)
#define PLATO_PLASMA_DISABLED      RGB(120, 55, 0)
#define PLATO_BG_DARK              RGB(16, 12, 10)
#define PLATO_BG_HOVER             RGB(45, 20, 0)

typedef struct {
    UINT id;
    HMENU hSubMenu;
    bool is_separator;
    wchar_t text[96];
} win_menu_item_data_t;

static HFONT win_get_menu_font(void) {
    static HFONT s_font = NULL;
    if (!s_font) {
        NONCLIENTMETRICSW ncm = { sizeof(ncm) };
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        s_font = CreateFontIndirectW(&ncm.lfMenuFont);
    }
    return s_font;
}

static void win_paint_menu_border_now(HWND hwndMenu);

static LRESULT CALLBACK win_popup_menu_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WNDPROC old = (WNDPROC)GetPropW(hwnd, L"PlatoOldProc");
    if (!old) old = DefWindowProcW;

    if (msg == WM_NCPAINT) {
        LRESULT lr = CallWindowProcW(old, hwnd, msg, wParam, lParam);
        win_paint_menu_border_now(hwnd);
        return lr;
    }
    if (msg == WM_NCDESTROY) {
        RemovePropW(hwnd, L"PlatoOldProc");
    }
    return CallWindowProcW(old, hwnd, msg, wParam, lParam);
}

static void win_paint_menu_border_now(HWND hwndMenu) {
    if (!hwndMenu) return;
    HDC hdc = GetWindowDC(hwndMenu);
    if (hdc) {
        RECT rc;
        GetWindowRect(hwndMenu, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        HBRUSH hPlasma = CreateSolidBrush(PLATO_PLASMA_COLOR);
        RECT rcTop = { 0, 0, w, 1 };
        RECT rcBottom = { 0, h - 1, w, h };
        RECT rcLeft = { 0, 0, 1, h };
        RECT rcRight = { w - 1, 0, w, h };

        FillRect(hdc, &rcTop, hPlasma);
        FillRect(hdc, &rcBottom, hPlasma);
        FillRect(hdc, &rcLeft, hPlasma);
        FillRect(hdc, &rcRight, hPlasma);

        DeleteObject(hPlasma);
        ReleaseDC(hwndMenu, hdc);
    }
}

static void win_apply_menu_border(void) {
    HWND hwndMenu = NULL;
    while ((hwndMenu = FindWindowExW(NULL, hwndMenu, L"#32768", NULL)) != NULL) {
        win_paint_menu_border_now(hwndMenu);
        if (!GetPropW(hwndMenu, L"PlatoOldProc")) {
            WNDPROC cur = (WNDPROC)GetWindowLongPtrW(hwndMenu, GWLP_WNDPROC);
            if (cur && cur != win_popup_menu_proc) {
                SetPropW(hwndMenu, L"PlatoOldProc", (HANDLE)cur);
                SetWindowLongPtrW(hwndMenu, GWLP_WNDPROC, (LONG_PTR)win_popup_menu_proc);
            }
        }
    }
}

static void win_setup_ownerdraw_menu(HMENU hMenu, bool is_top_bar) {
    if (!hMenu) return;
    int count = GetMenuItemCount(hMenu);
    for (int i = 0; i < count; i++) {
        MENUITEMINFOW mii = { sizeof(mii) };
        wchar_t textBuf[96] = {0};
        mii.fMask = MIIM_FTYPE | MIIM_ID | MIIM_SUBMENU | MIIM_DATA | MIIM_STRING;
        mii.dwTypeData = textBuf;
        mii.cch = sizeof(textBuf) / sizeof(textBuf[0]);

        if (GetMenuItemInfoW(hMenu, (UINT)i, TRUE, &mii)) {
            if (!is_top_bar) {
                win_menu_item_data_t *data = (win_menu_item_data_t *)mii.dwItemData;
                if (!data) {
                    data = (win_menu_item_data_t *)calloc(1, sizeof(win_menu_item_data_t));
                    data->id = mii.wID;
                    data->hSubMenu = mii.hSubMenu;
                    data->is_separator = ((mii.fType & MFT_SEPARATOR) != 0);
                    if (!data->is_separator) {
                        wcsncpy(data->text, textBuf, sizeof(data->text) / sizeof(wchar_t) - 1);
                    }
                    mii.dwItemData = (ULONG_PTR)data;
                }
                mii.fMask = MIIM_FTYPE | MIIM_DATA;
                mii.fType = MFT_OWNERDRAW;
                if (data->is_separator) mii.fType |= MFT_SEPARATOR;
                SetMenuItemInfoW(hMenu, (UINT)i, TRUE, &mii);
            }
            if (mii.hSubMenu) {
                win_setup_ownerdraw_menu(mii.hSubMenu, false);
            }
        }
    }
}

#ifndef WM_UAHDRAWMENU
#define WM_UAHDRAWMENU 0x0091
#endif
#ifndef WM_UAHDRAWMENUITEM
#define WM_UAHDRAWMENUITEM 0x0092
#endif
#ifndef OBJID_MENU
#define OBJID_MENU ((LONG)0xFFFFFFFD)
#endif

typedef struct {
    HMENU hmenu;
    HDC hdc;
    DWORD dwFlags;
} UAHMENU;

typedef struct {
    DWORD iPosition;
    UAHMENU uahMenu;
    DWORD dwStateId;
    DWORD dwFlags;
} UAHMENUITEM;

typedef struct {
    DRAWITEMSTRUCT dis;
    UAHMENU uahMenu;
    UAHMENUITEM umi;
} UAHDRAWMENUITEM;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
                        case WM_ENTERIDLE: {
            if (wParam == MSGF_MENU && lParam) {
                HWND hwndMenu = (HWND)lParam;
                win_paint_menu_border_now(hwndMenu);
                if (!GetPropW(hwndMenu, L"PlatoOldProc")) {
                    WNDPROC cur = (WNDPROC)GetWindowLongPtrW(hwndMenu, GWLP_WNDPROC);
                    if (cur && cur != win_popup_menu_proc) {
                        SetPropW(hwndMenu, L"PlatoOldProc", (HANDLE)cur);
                        SetWindowLongPtrW(hwndMenu, GWLP_WNDPROC, (LONG_PTR)win_popup_menu_proc);
                    }
                }
            }
            return 0;
        }

        case WM_MENUSELECT: {
            win_apply_menu_border();
            break;
        }

        case WM_INITMENUPOPUP: {
            HMENU hMenu = (HMENU)wParam;
            if (hMenu) {
                MENUINFO mi = { sizeof(mi) };
                mi.fMask = MIM_BACKGROUND;
                mi.hbrBack = CreateSolidBrush(PLATO_BG_DARK);
                SetMenuInfo(hMenu, &mi);
                win_setup_ownerdraw_menu(hMenu, false);
            }
            win_apply_menu_border();
            return 0;
        }

        case WM_MEASUREITEM: {
            MEASUREITEMSTRUCT *pmis = (MEASUREITEMSTRUCT *)lParam;
            if (pmis && pmis->CtlType == ODT_MENU) {
                win_menu_item_data_t *data = (win_menu_item_data_t *)pmis->itemData;
                if (data && data->is_separator) {
                    pmis->itemHeight = 9;
                    pmis->itemWidth = 120;
                    return TRUE;
                }
                pmis->itemHeight = 24;
                pmis->itemWidth = 200;
                if (data && data->text[0]) {
                    HDC hdc = GetDC(hwnd);
                    HFONT oldFont = (HFONT)SelectObject(hdc, win_get_menu_font());
                    SIZE sz;
                    GetTextExtentPoint32W(hdc, data->text, (int)wcslen(data->text), &sz);
                    pmis->itemWidth = (UINT)(sz.cx + 64);
                    if (pmis->itemWidth < 190) pmis->itemWidth = 190;
                    SelectObject(hdc, oldFont);
                    ReleaseDC(hwnd, hdc);
                }
                return TRUE;
            }
            break;
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT *pdis = (DRAWITEMSTRUCT *)lParam;
            if (pdis && pdis->CtlType == ODT_MENU) {
                win_apply_menu_border();

                HDC hdc = pdis->hDC;
                RECT rc = pdis->rcItem;
                win_menu_item_data_t *data = (win_menu_item_data_t *)pdis->itemData;

                bool is_sel = (pdis->itemState & ODS_SELECTED) != 0;
                bool is_dis = (pdis->itemState & (ODS_GRAYED | ODS_DISABLED)) != 0;
                bool is_chk = (pdis->itemState & ODS_CHECKED) != 0;

                COLORREF bgCol = is_sel ? PLATO_BG_HOVER : PLATO_BG_DARK;
                COLORREF txtCol = is_dis ? PLATO_PLASMA_DISABLED : (is_sel ? PLATO_PLASMA_BRIGHT : PLATO_PLASMA_COLOR);

                HBRUSH hbr = CreateSolidBrush(bgCol);
                FillRect(hdc, &rc, hbr);
                DeleteObject(hbr);

                /* Separatore orizzontale in Orange Plasma */
                if (data && data->is_separator) {
                    RECT rcSep = rc;
                    rcSep.top = rc.top + (rc.bottom - rc.top) / 2;
                    rcSep.bottom = rcSep.top + 1;
                    rcSep.left += 8;
                    rcSep.right -= 8;
                    HBRUSH hAmber = CreateSolidBrush(PLATO_PLASMA_COLOR);
                    FillRect(hdc, &rcSep, hAmber);
                    DeleteObject(hAmber);
                    return TRUE;
                }

                /* Spunta o indicatore selezione ambra */
                if (is_chk) {
                    RECT rcBullet = { rc.left + 8, rc.top + (rc.bottom - rc.top - 8) / 2, rc.left + 16, 0 };
                    rcBullet.bottom = rcBullet.top + 8;
                    HBRUSH hBullet = CreateSolidBrush(PLATO_PLASMA_COLOR);
                    FillRect(hdc, &rcBullet, hBullet);
                    DeleteObject(hBullet);
                }

                if (data && data->text[0]) {
                    HFONT oldFont = (HFONT)SelectObject(hdc, win_get_menu_font());
                    SetBkMode(hdc, TRANSPARENT);
                    SetTextColor(hdc, txtCol);

                    wchar_t leftText[96] = {0};
                    wchar_t rightText[96] = {0};
                    wchar_t *tabPos = wcschr(data->text, L'\t');
                    if (tabPos) {
                        size_t lLen = (size_t)(tabPos - data->text);
                        wcsncpy(leftText, data->text, lLen);
                        wcsncpy(rightText, tabPos + 1, sizeof(rightText) / sizeof(wchar_t) - 1);
                    } else {
                        wcsncpy(leftText, data->text, sizeof(leftText) / sizeof(wchar_t) - 1);
                    }

                    RECT rcText = rc;
                    rcText.left += 26;
                    rcText.right -= 20;
                    DrawTextW(hdc, leftText, -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                    if (rightText[0]) {
                        COLORREF scCol = is_dis ? PLATO_PLASMA_DISABLED : (is_sel ? PLATO_PLASMA_BRIGHT : PLATO_PLASMA_COLOR);
                        SetTextColor(hdc, scCol);
                        DrawTextW(hdc, rightText, -1, &rcText, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
                    }

                    if (data->hSubMenu) {
                        RECT rcArrow = rc;
                        rcArrow.right -= 10;
                        SetTextColor(hdc, PLATO_PLASMA_COLOR);
                        DrawTextW(hdc, L"\x203A", -1, &rcArrow, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
                    }

                    SelectObject(hdc, oldFont);
                }
                return TRUE;
            }
            break;
        }

        case WM_UAHDRAWMENU: {
            UAHMENU *pudms = (UAHMENU *)lParam;
            if (pudms && pudms->hdc) {
                MENUBARINFO mbi = { sizeof(mbi) };
                if (GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi)) {
                    RECT rc = mbi.rcBar;
                    RECT rcWin;
                    GetWindowRect(hwnd, &rcWin);
                    OffsetRect(&rc, -rcWin.left, -rcWin.top);

                    rc.bottom += 2;

                    HBRUSH hbr = CreateSolidBrush(PLATO_BG_DARK);
                    FillRect(pudms->hdc, &rc, hbr);
                    DeleteObject(hbr);

                    RECT rcLine = rc;
                    rcLine.top = rcLine.bottom - 1;
                    HBRUSH hLine = CreateSolidBrush(PLATO_PLASMA_COLOR);
                    FillRect(pudms->hdc, &rcLine, hLine);
                    DeleteObject(hLine);
                }
            }
            return 0;
        }

        case WM_UAHDRAWMENUITEM: {
            UAHDRAWMENUITEM *pudmi = (UAHDRAWMENUITEM *)lParam;
            HDC hdc = pudmi->dis.hDC ? pudmi->dis.hDC : pudmi->uahMenu.hdc;
            if (hdc) {
                RECT rc = pudmi->dis.rcItem;

                const wchar_t *defaults[] = { L"File", L"Edit", L"Connection", L"View", L"Tools", L"Help" };
                const wchar_t *item_name = L"";

                DWORD pos = pudmi->umi.iPosition;
                if (pos < 6) {
                    item_name = defaults[pos];
                } else {
                    if (rc.left < 35) item_name = defaults[0];
                    else if (rc.left < 75) item_name = defaults[1];
                    else if (rc.left < 155) item_name = defaults[2];
                    else if (rc.left < 205) item_name = defaults[3];
                    else if (rc.left < 255) item_name = defaults[4];
                    else item_name = defaults[5];
                }

                bool is_hover = (pudmi->umi.dwStateId == 2) || ((pudmi->dis.itemState & ODS_HOTLIGHT) != 0);
                bool is_selected = (pudmi->umi.dwStateId == 3) || ((pudmi->dis.itemState & ODS_SELECTED) != 0);
                bool is_disabled = (pudmi->umi.dwStateId == 4) || ((pudmi->dis.itemState & (ODS_GRAYED | ODS_DISABLED)) != 0);

                COLORREF bgCol = is_selected ? PLATO_BG_HOVER : (is_hover ? PLATO_BG_HOVER : PLATO_BG_DARK);
                COLORREF txtCol = is_disabled ? PLATO_PLASMA_DISABLED : (is_selected ? PLATO_PLASMA_BRIGHT : (is_hover ? PLATO_PLASMA_BRIGHT : PLATO_PLASMA_COLOR));

                HBRUSH hbr = CreateSolidBrush(bgCol);
                FillRect(hdc, &rc, hbr);
                DeleteObject(hbr);

                HFONT oldFont = (HFONT)SelectObject(hdc, win_get_menu_font());
                int oldBkMode = SetBkMode(hdc, TRANSPARENT);
                COLORREF oldTxtCol = SetTextColor(hdc, txtCol);

                DrawTextW(hdc, item_name, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                SelectObject(hdc, oldFont);
                SetTextColor(hdc, oldTxtCol);
                SetBkMode(hdc, oldBkMode);
            }
            return 0;
        }

        case WM_NCPAINT: {
            LRESULT lr = DefWindowProcW(hwnd, msg, wParam, lParam);
                        MENUBARINFO mbi = { sizeof(mbi) };
            if (GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi)) {
                HDC hdc = GetWindowDC(hwnd);
                if (hdc) {
                    RECT rcWin;
                    GetWindowRect(hwnd, &rcWin);
                    RECT rcLine = mbi.rcBar;
                    OffsetRect(&rcLine, -rcWin.left, -rcWin.top);
                    rcLine.top = rcLine.bottom;
                    rcLine.bottom = rcLine.top + 2;
                    HBRUSH hbr = CreateSolidBrush(RGB(16, 12, 10));
                    FillRect(hdc, &rcLine, hbr);
                    DeleteObject(hbr);

                    RECT rcBorder = rcLine;
                    rcBorder.top = rcBorder.bottom - 1;
                    HBRUSH hAmber = CreateSolidBrush(RGB(50, 25, 8));
                    FillRect(hdc, &rcBorder, hAmber);
                    DeleteObject(hAmber);

                    ReleaseDC(hwnd, hdc);
                }
            }
            return lr;
        }

        case WM_NCACTIVATE: {
            LRESULT lr = DefWindowProcW(hwnd, msg, wParam, lParam);
            DrawMenuBar(hwnd);
                        return lr;
        }
        case WM_SIZE:
            if (g_app.device && wParam != SIZE_MINIMIZED) {
                win_update_render_target(&g_app, LOWORD(lParam), HIWORD(lParam));
            }
            return 0;

                case WM_COMMAND: {
            int cmdId = LOWORD(wParam);
            plato_profile_t *cur_p = &g_app.profile_list.profiles[g_app.current_profile_idx];

            if (cmdId >= IDM_PROFILE_BASE && cmdId <= IDM_PROFILE_MAX) {
                size_t idx = (size_t)(cmdId - IDM_PROFILE_BASE);
                if (idx < g_app.profile_list.count) {
                    win_connect_profile(&g_app.profile_list.profiles[idx], &g_app);
                }
                return 0;
            }

            if (cmdId >= IDM_FILE_NEW_WIN_PROFILE_BASE && cmdId <= IDM_FILE_NEW_WIN_PROFILE_MAX) {
                size_t idx = (size_t)(cmdId - IDM_FILE_NEW_WIN_PROFILE_BASE);
                if (idx < g_app.profile_list.count) {
                    win_launch_instance(g_app.profile_list.profiles[idx].name);
                }
                return 0;
            }

            /* Menu Display Modes: 0: Real Plasma, 1: Crisp Mono, 2: Split Mono, 3: Crisp Color, 4: Real CRT */
            if (cmdId >= IDM_VIEW_MODE_REAL_PLASMA && cmdId <= IDM_VIEW_MODE_REAL_CRT) {
                int new_mode = cur_p->display_mode;
                if (cmdId == IDM_VIEW_MODE_REAL_PLASMA) new_mode = 0;
                else if (cmdId == IDM_VIEW_MODE_CRISP_MONO) new_mode = 1;
                else if (cmdId == IDM_VIEW_MODE_SPLIT_MONO) new_mode = 2;
                else if (cmdId == IDM_VIEW_MODE_CRISP_COLOR) new_mode = 3;
                else if (cmdId == IDM_VIEW_MODE_REAL_CRT) new_mode = 4;

                if (new_mode == cur_p->display_mode) {
                    return 0;
                }

                bool cur_is_color = (cur_p->display_mode == 3 || cur_p->display_mode == 4);
                bool new_is_color = (new_mode == 3 || new_mode == 4);
                bool is_connected = (g_app.transport && g_app.transport->connected);

                if (is_connected && (cur_is_color != new_is_color)) {
                    int resp = MessageBoxW(hwnd,
                        L"Switching between Monochrome and Color requires reconnecting to negotiate terminal capabilities with the PLATO mainframe.\n\nDo you want to reconnect now?",
                        L"Reconnect Required for Display Mode Change",
                        MB_YESNO | MB_ICONQUESTION);
                    if (resp != IDYES) {
                        return 0;
                    }
                    cur_p->display_mode = new_mode;
                    g_app.display_mode = new_mode;
                    plato_profiles_save(&g_app.profile_list);
                    win_connect_profile(cur_p, &g_app);
                    plato_optical_invalidate(g_app.optical);
                    return 0;
                }

                cur_p->display_mode = new_mode;
                g_app.display_mode = new_mode;
                plato_profiles_save(&g_app.profile_list);
                plato_terminal_set_color_mode(&g_app.terminal, new_is_color);
                plato_optical_invalidate(g_app.optical);
                win_menu_update_state(g_app.menu, cur_p, g_app.transport && g_app.transport->connected,
                                     g_app.diag_log, g_app.renderer_log, g_app.is_fullscreen, &g_app.profile_list, g_app.current_profile_idx);
                return 0;
            }

            /* Menu Plasma Decay */
            if (cmdId >= IDM_VIEW_PLASMA_DECAY_100 && cmdId <= IDM_VIEW_PLASMA_DECAY_5000) {
                const int vals[] = { 100, 200, 500, 1000, 2000, 5000 };
                cur_p->persistence_ms = vals[cmdId - IDM_VIEW_PLASMA_DECAY_100];
                plato_profiles_save(&g_app.profile_list);
                win_menu_update_state(g_app.menu, cur_p, g_app.transport && g_app.transport->connected,
                                     g_app.diag_log, g_app.renderer_log, g_app.is_fullscreen, &g_app.profile_list, g_app.current_profile_idx);
                return 0;
            }

            /* Menu Plasma Distortion */
            if (cmdId >= IDM_VIEW_PLASMA_DIST_NONE && cmdId <= IDM_VIEW_PLASMA_DIST_CYL) {
                cur_p->plasma_distortion = cmdId - IDM_VIEW_PLASMA_DIST_NONE;
                g_app.distortion = cur_p->plasma_distortion;
                plato_profiles_save(&g_app.profile_list);
                win_menu_update_state(g_app.menu, cur_p, g_app.transport && g_app.transport->connected,
                                     g_app.diag_log, g_app.renderer_log, g_app.is_fullscreen, &g_app.profile_list, g_app.current_profile_idx);
                return 0;
            }

            /* Menu CRT Beam */
            if (cmdId >= IDM_VIEW_CRT_BEAM_STD && cmdId <= IDM_VIEW_CRT_BEAM_ULTRA) {
                cur_p->crt_beam_level = cmdId - IDM_VIEW_CRT_BEAM_STD;
                plato_profiles_save(&g_app.profile_list);
                win_menu_update_state(g_app.menu, cur_p, g_app.transport && g_app.transport->connected,
                                     g_app.diag_log, g_app.renderer_log, g_app.is_fullscreen, &g_app.profile_list, g_app.current_profile_idx);
                return 0;
            }

            /* Menu CRT Decay */
            if (cmdId >= IDM_VIEW_CRT_DECAY_20 && cmdId <= IDM_VIEW_CRT_DECAY_5000) {
                const int vals[] = { 20, 200, 500, 1000, 2000, 5000 };
                cur_p->crt_persistence_ms = vals[cmdId - IDM_VIEW_CRT_DECAY_20];
                plato_profiles_save(&g_app.profile_list);
                win_menu_update_state(g_app.menu, cur_p, g_app.transport && g_app.transport->connected,
                                     g_app.diag_log, g_app.renderer_log, g_app.is_fullscreen, &g_app.profile_list, g_app.current_profile_idx);
                return 0;
            }

            /* Menu CRT Distortion */
            if (cmdId >= IDM_VIEW_CRT_DIST_NONE && cmdId <= IDM_VIEW_CRT_DIST_CYL) {
                cur_p->crt_distortion = cmdId - IDM_VIEW_CRT_DIST_NONE;
                plato_profiles_save(&g_app.profile_list);
                win_menu_update_state(g_app.menu, cur_p, g_app.transport && g_app.transport->connected,
                                     g_app.diag_log, g_app.renderer_log, g_app.is_fullscreen, &g_app.profile_list, g_app.current_profile_idx);
                return 0;
            }

                        switch (cmdId) {
                case IDM_FILE_NEW_WINDOW:
                    win_launch_instance(NULL);
                    return 0;
                case IDM_FILE_CLOSE_WINDOW:
                case IDM_FILE_EXIT:
                    PostQuitMessage(0);
                    return 0;
                case IDM_EDIT_COPY_TEXT_VERBATIM:
                    win_copy_text_to_clipboard(&g_app, false);
                    return 0;
                case IDM_EDIT_COPY_TEXT_COMPACT:
                    win_copy_text_to_clipboard(&g_app, true);
                    return 0;
                case IDM_EDIT_COPY_SELECTED_TEXT:
                    win_text_buffer_show(hwnd, &g_app.terminal);
                    return 0;
                case IDM_EDIT_COPY_SCREEN_IMAGE:
                    win_copy_screen_image(&g_app);
                    return 0;
                case IDM_EDIT_PASTE:
                    g_app.paste_cancelled = false;
                    win_paste_clipboard_text(&g_app);
                    return 0;
                case IDM_EDIT_CANCEL_PASTE:
                    g_app.paste_cancelled = true;
                    return 0;
                case IDM_CONN_CONNECT_DEFAULT: {
                    plato_profile_t *def = plato_profiles_get_default(&g_app.profile_list);
                    if (def) win_connect_profile(def, &g_app);
                    return 0;
                }
                case IDM_CONN_MANAGE_PROFILES:
                    win_profiles_dialog_show(hwnd, &g_app.profile_list, win_connect_profile, &g_app);
                    if (g_app.profile_list.default_index >= 0 && (size_t)g_app.profile_list.default_index < g_app.profile_list.count) {
                        g_app.current_profile_idx = g_app.profile_list.default_index;
                    }
                    cur_p = &g_app.profile_list.profiles[g_app.current_profile_idx];
                    g_app.display_mode = cur_p->display_mode;
                    g_app.distortion = cur_p->plasma_distortion;
                    plato_terminal_set_color_mode(&g_app.terminal, (cur_p->display_mode == 3 || cur_p->display_mode == 4));
                    plato_optical_invalidate(g_app.optical);
                    DestroyMenu(g_app.menu);
                    g_app.menu = win_menu_create(&g_app.profile_list);
                    win_setup_ownerdraw_menu(g_app.menu, true);
                    SetMenu(hwnd, g_app.menu);
                    win_menu_update_state(g_app.menu, cur_p,
                                         g_app.transport && g_app.transport->connected,
                                         g_app.diag_log, g_app.renderer_log, g_app.is_fullscreen,
                                         &g_app.profile_list, g_app.current_profile_idx);
                    return 0;
                case IDM_CONN_DISCONNECT:
                    if (g_app.transport) {
                        plato_script_stop(g_app.script_runner);
                        if (g_app.transport->connected) {
                            plato_transport_disconnect(g_app.transport);
                        }
                        win_clear_screen(&g_app);
                        win_menu_update_state(g_app.menu, cur_p,
                                             false, g_app.diag_log, g_app.renderer_log, g_app.is_fullscreen,
                                             &g_app.profile_list, g_app.current_profile_idx);
                    }
                    return 0;
                case IDM_VIEW_KEYBOARD_REF:
                    win_keyref_show(hwnd);
                    return 0;
                case IDM_VIEW_FULLSCREEN:
                    win_toggle_fullscreen(&g_app);
                    return 0;
                case IDM_VIEW_FPS_HUD:
                    g_app.show_fps_hud = !g_app.show_fps_hud;
                    plato_optical_invalidate(g_app.optical);
                    return 0;
                case IDM_TOOLS_SAVE_SCREEN_TEXT:
                    win_copy_screen_text(&g_app);
                    MessageBoxW(hwnd, L"Screen text saved to screen.txt and clipboard.", L"PlatoLives", MB_OK | MB_ICONINFORMATION);
                    return 0;
                case IDM_TOOLS_DIAG_LOG:
                    g_app.diag_log = !g_app.diag_log;
                    plato_transport_set_logging(g_app.transport, g_app.diag_log);
                    win_menu_update_state(g_app.menu, cur_p,
                                         g_app.transport && g_app.transport->connected,
                                         g_app.diag_log, g_app.renderer_log, g_app.is_fullscreen,
                                         &g_app.profile_list, g_app.current_profile_idx);
                    return 0;
                case IDM_TOOLS_RENDERER_LOG:
                    g_app.renderer_log = !g_app.renderer_log;
                    win_menu_update_state(g_app.menu, cur_p,
                                         g_app.transport && g_app.transport->connected,
                                         g_app.diag_log, g_app.renderer_log, g_app.is_fullscreen,
                                         &g_app.profile_list, g_app.current_profile_idx);
                    return 0;
                case IDM_HELP_ABOUT:
                    win_about_show(hwnd);
                    return 0;
            }
            break;
        }

        case WM_LBUTTONDOWN:
            win_handle_lbuttondown(&g_app, lParam);
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            win_handle_keydown(&g_app, wParam, lParam);
            return 0;
        case WM_CHAR:
            win_handle_char(&g_app, wParam);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;

    /* Controllo modalita' Console (--console, -c, --c) */
    if (lpCmdLine && (strstr(lpCmdLine, "--console") || strstr(lpCmdLine, "-c") || strstr(lpCmdLine, "--c"))) {
        const char *host = NULL;
        int port = 0;
        char host_buf[128] = {0};

        const char *p = strstr(lpCmdLine, "--console");
        if (!p) p = strstr(lpCmdLine, "-c");
        if (!p) p = strstr(lpCmdLine, "--c");
        if (p) {
            while (*p && *p != ' ') p++;
            while (*p == ' ') p++;
            if (*p && *p != '-') {
                size_t h_idx = 0;
                while (*p && *p != ' ' && *p != '\r' && *p != '\n' && h_idx < sizeof(host_buf) - 1) {
                    host_buf[h_idx++] = *p++;
                }
                host_buf[h_idx] = '\0';
                if (h_idx > 0) host = host_buf;

                while (*p == ' ') p++;
                if (*p && *p != '-') {
                    port = atoi(p);
                }
            }
        }

        return plato_console_run(host, port);
    }

    /* Modalita' GUI: stacca e nascondi la console se aperta automaticamente da Explorer */
    DWORD procs[2];
    if (GetConsoleProcessList(procs, 2) <= 1) {
        HWND hwndConsole = GetConsoleWindow();
        if (hwndConsole) ShowWindow(hwndConsole, SW_HIDE);
    }
    FreeConsole();

    memset(&g_app, 0, sizeof(g_app));
    g_app.running = true;

    plato_terminal_init(&g_app.terminal);
    g_app.terminal.beep_callback = win_beep_callback;
    plato_ringbuf_init(&g_app.ringbuf);
    g_app.transport = plato_transport_create();
    g_app.terminal.transport = g_app.transport;
    plato_keyboard_state_init(&g_app.keyboard_state);
    g_app.optical = plato_optical_create();
    g_app.optical_fb = (uint32_t *)calloc(PLATO_OPTICAL_WIDTH * PLATO_OPTICAL_HEIGHT, sizeof(uint32_t));

    plato_profiles_load(&g_app.profile_list);
    g_app.current_profile_idx = g_app.profile_list.default_index;
    g_app.script_runner = plato_script_create(&g_app.terminal);

    /* Controllo argomento --profile <nome> */
    if (lpCmdLine && strstr(lpCmdLine, "--profile")) {
        const char *p = strstr(lpCmdLine, "--profile");
        p += 9;
        while (*p == ' ' || *p == '"') p++;
        char target_name[64];
        size_t t_idx = 0;
        while (*p && *p != '"' && *p != '\r' && *p != '\n' && t_idx < sizeof(target_name) - 1) {
            target_name[t_idx++] = *p++;
        }
        target_name[t_idx] = '\0';

        for (size_t i = 0; i < g_app.profile_list.count; i++) {
            if (strcmp(g_app.profile_list.profiles[i].name, target_name) == 0) {
                g_app.current_profile_idx = (int)i;
                break;
            }
        }
    }

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    wc.hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(1), IMAGE_ICON,
                                  GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    wc.lpszClassName = L"PlatoLivesWinClass";
    RegisterClassExW(&wc);

    RECT wr = { 0, 0, 768, 768 };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    win_apply_dark_theme(NULL);
    g_app.menu = win_menu_create(&g_app.profile_list);
    win_setup_ownerdraw_menu(g_app.menu, true);
    g_app.distortion = 2;

    g_app.hwnd = CreateWindowExW(
        0, wc.lpszClassName, L"PlatoLives (PLATO IV / Cyber1)",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
        NULL, g_app.menu, hInstance, NULL
    );

    if (!g_app.hwnd) return 1;
    win_apply_dark_theme(g_app.hwnd);

    if (!win_init_d3d11(&g_app, g_app.hwnd)) {
        MessageBoxW(g_app.hwnd, L"Errore inizializzazione Direct3D 11", L"Errore PlatoLives", MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_app.hwnd, nCmdShow);
    UpdateWindow(g_app.hwnd);

    if (g_app.current_profile_idx >= 0 && (size_t)g_app.current_profile_idx < g_app.profile_list.count) {
        win_connect_profile(&g_app.profile_list.profiles[g_app.current_profile_idx], &g_app);
    }

    pthread_create(&g_app.net_thread, NULL, win_net_worker, &g_app);

    MSG msg = {0};
    while (msg.message != WM_QUIT) {
        if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        } else {
            win_render_frame(&g_app);
        }
    }

    g_app.running = false;
    plato_script_stop(g_app.script_runner);
    plato_script_destroy(g_app.script_runner);
    plato_transport_disconnect(g_app.transport);
    pthread_join(g_app.net_thread, NULL);
    plato_transport_destroy(g_app.transport);
    plato_ringbuf_destroy(&g_app.ringbuf);

    return (int)msg.wParam;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    return WinMain(GetModuleHandleW(NULL), NULL, GetCommandLineA(), SW_SHOWDEFAULT);
}
