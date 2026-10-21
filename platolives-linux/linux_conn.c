/*
 * PlatoLives - Connecting Status Window (SDL2)
 * High-fidelity Retro Phosphor Dialog with Asynchronous Connect, Cancel & Retry
 */

#include "linux_conn.h"
#include "linux_theme.h"
#include "plato/stb_truetype.h"
#include "fonts/roboto_font_data.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONN_WIN_W 500
#define CONN_WIN_H 230

typedef enum {
    CONN_STATE_IDLE = 0,
    CONN_STATE_CONNECTING,
    CONN_STATE_SUCCESS,
    CONN_STATE_FAILED
} conn_state_t;

static SDL_Window *g_conn_win = NULL;
static SDL_Renderer *g_conn_ren = NULL;

static stbtt_bakedchar g_conn_cdata[96];
static unsigned char *g_conn_atlas = NULL;
static SDL_Texture *g_conn_atlas_tex = NULL;
static bool g_conn_font_ready = false;

static plato_profile_t g_profile;
static plato_transport_t *g_transport = NULL;
static linux_conn_done_cb g_done_cb = NULL;
static void *g_cb_data = NULL;

static volatile conn_state_t g_state = CONN_STATE_IDLE;
static pthread_t g_worker_th;
static bool g_worker_active = false;
static uint32_t g_anim_start_tick = 0;
static int g_hover_btn_idx = 0; /* 0: nessuno, 1: Cancel o Retry, 2: Close */

static void init_conn_font(void) {
    if (g_conn_font_ready || !g_conn_ren) return;
    if (!g_conn_atlas) {
        g_conn_atlas = (unsigned char *)malloc(512 * 512);
    }
    stbtt_BakeFontBitmap(g_roboto_ttf, 0, 16.5f, g_conn_atlas, 512, 512, 32, 96, g_conn_cdata);

    uint32_t *rgba = (uint32_t *)malloc(512 * 512 * sizeof(uint32_t));
    if (!rgba) return;

    for (int i = 0; i < 512 * 512; i++) {
        uint8_t a = g_conn_atlas[i];
        rgba[i] = ((uint32_t)a << 24) | 0x00FFFFFF;
    }

    g_conn_atlas_tex = SDL_CreateTexture(g_conn_ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 512, 512);
    SDL_UpdateTexture(g_conn_atlas_tex, NULL, rgba, 512 * sizeof(uint32_t));
    SDL_SetTextureBlendMode(g_conn_atlas_tex, SDL_BLENDMODE_BLEND);

    free(rgba);
    g_conn_font_ready = true;
}

static float get_string_width_f(const char *str, int count) {
    if (!str) return 0.0f;
    float w = 0.0f;
    int len = (count < 0) ? (int)strlen(str) : count;
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        if (c < 32 || c >= 128) c = '?';
        w += g_conn_cdata[c - 32].xadvance;
    }
    return w;
}

static void draw_text_line(float x, float y, const char *str, linux_color_t color) {
    if (!g_conn_ren || !g_conn_atlas_tex || !str) return;
    SDL_SetTextureColorMod(g_conn_atlas_tex, color.r, color.g, color.b);

    float cur_x = x;
    size_t len = strlen(str);
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        if (c < 32 || c >= 128) c = '?';
        stbtt_bakedchar *bc = &g_conn_cdata[c - 32];
        SDL_Rect src = { bc->x0, bc->y0, bc->x1 - bc->x0, bc->y1 - bc->y0 };
        SDL_Rect dst = { (int)(cur_x + bc->xoff), (int)(y + bc->yoff), src.w, src.h };
        SDL_RenderCopy(g_conn_ren, g_conn_atlas_tex, &src, &dst);
        cur_x += bc->xadvance;
    }
}

static void* conn_worker(void *arg) {
    (void)arg;
    bool ok = plato_transport_connect(g_transport, g_profile.host, g_profile.port);
    g_state = ok ? CONN_STATE_SUCCESS : CONN_STATE_FAILED;
    return NULL;
}

static void start_connect_worker(void) {
    if (g_worker_active) {
        pthread_join(g_worker_th, NULL);
        g_worker_active = false;
    }
    g_state = CONN_STATE_CONNECTING;
    g_anim_start_tick = SDL_GetTicks();
    g_worker_active = true;
    pthread_create(&g_worker_th, NULL, conn_worker, NULL);
}

void linux_conn_show(const plato_profile_t *p, plato_transport_t *t, linux_conn_done_cb cb, void *user_data) {
    if (!p || !t) return;
    linux_conn_close();

    g_profile = *p;
    g_transport = t;
    g_done_cb = cb;
    g_cb_data = user_data;
    g_hover_btn_idx = 0;

    g_conn_win = SDL_CreateWindow(
        "PlatoLives - Connecting",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        CONN_WIN_W, CONN_WIN_H,
        SDL_WINDOW_SHOWN
    );
    if (!g_conn_win) return;

    g_conn_ren = SDL_CreateRenderer(g_conn_win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_conn_ren) {
        g_conn_ren = SDL_CreateRenderer(g_conn_win, -1, SDL_RENDERER_SOFTWARE);
    }
    init_conn_font();

    start_connect_worker();
}

void linux_conn_close(void) {
    if (g_worker_active) {
        if (g_transport && g_state == CONN_STATE_CONNECTING) {
            plato_transport_cancel_connect(g_transport);
        }
        pthread_join(g_worker_th, NULL);
        g_worker_active = false;
    }

    if (g_conn_atlas_tex) {
        SDL_DestroyTexture(g_conn_atlas_tex);
        g_conn_atlas_tex = NULL;
    }
    if (g_conn_ren) {
        SDL_DestroyRenderer(g_conn_ren);
        g_conn_ren = NULL;
    }
    if (g_conn_win) {
        SDL_DestroyWindow(g_conn_win);
        g_conn_win = NULL;
    }
    if (g_conn_atlas) {
        free(g_conn_atlas);
        g_conn_atlas = NULL;
    }
    g_conn_font_ready = false;
    g_state = CONN_STATE_IDLE;
}

bool linux_conn_is_open(void) {
    return (g_conn_win != NULL);
}

void linux_conn_handle_event(const SDL_Event *ev) {
    if (!g_conn_win || !ev) return;

    uint32_t win_id = SDL_GetWindowID(g_conn_win);
    if (ev->type == SDL_WINDOWEVENT && ev->window.windowID == win_id) {
        if (ev->window.event == SDL_WINDOWEVENT_CLOSE) {
            bool success = (g_state == CONN_STATE_SUCCESS);
            linux_conn_done_cb cb = g_done_cb;
            void *data = g_cb_data;
            linux_conn_close();
            if (cb) cb(success, data);
            return;
        }
    }

    /* Coordinate pulsanti:
     * In CONN_STATE_CONNECTING: 1 pulsante centrato: x=180, w=140, h=32
     * In CONN_STATE_FAILED: 2 pulsanti: Retry (x=90, w=140), Close (x=270, w=140)
     */
    SDL_Rect rc_single = { 180, 165, 140, 32 };
    SDL_Rect rc_retry  = {  90, 165, 140, 32 };
    SDL_Rect rc_close  = { 270, 165, 140, 32 };

    if (ev->type == SDL_MOUSEMOTION && ev->motion.windowID == win_id) {
        int mx = ev->motion.x;
        int my = ev->motion.y;
        if (g_state == CONN_STATE_CONNECTING) {
            g_hover_btn_idx = (mx >= rc_single.x && mx <= rc_single.x + rc_single.w &&
                               my >= rc_single.y && my <= rc_single.y + rc_single.h) ? 1 : 0;
        } else if (g_state == CONN_STATE_FAILED) {
            if (mx >= rc_retry.x && mx <= rc_retry.x + rc_retry.w &&
                my >= rc_retry.y && my <= rc_retry.y + rc_retry.h) {
                g_hover_btn_idx = 1; /* Retry */
            } else if (mx >= rc_close.x && mx <= rc_close.x + rc_close.w &&
                       my >= rc_close.y && my <= rc_close.y + rc_close.h) {
                g_hover_btn_idx = 2; /* Close */
            } else {
                g_hover_btn_idx = 0;
            }
        }
    } else if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.windowID == win_id) {
        if (ev->button.button == SDL_BUTTON_LEFT) {
            int mx = ev->button.x;
            int my = ev->button.y;

            if (g_state == CONN_STATE_CONNECTING) {
                if (mx >= rc_single.x && mx <= rc_single.x + rc_single.w &&
                    my >= rc_single.y && my <= rc_single.y + rc_single.h) {
                    /* Cancel */
                    linux_conn_done_cb cb = g_done_cb;
                    void *data = g_cb_data;
                    linux_conn_close();
                    if (cb) cb(false, data);
                    return;
                }
            } else if (g_state == CONN_STATE_FAILED) {
                if (mx >= rc_retry.x && mx <= rc_retry.x + rc_retry.w &&
                    my >= rc_retry.y && my <= rc_retry.y + rc_retry.h) {
                    /* Retry */
                    start_connect_worker();
                    return;
                } else if (mx >= rc_close.x && mx <= rc_close.x + rc_close.w &&
                           my >= rc_close.y && my <= rc_close.y + rc_close.h) {
                    /* Close */
                    linux_conn_done_cb cb = g_done_cb;
                    void *data = g_cb_data;
                    linux_conn_close();
                    if (cb) cb(false, data);
                    return;
                }
            }
        }
    } else if (ev->type == SDL_KEYDOWN && ev->key.windowID == win_id) {
        if (g_state == CONN_STATE_FAILED && (ev->key.keysym.sym == SDLK_RETURN || ev->key.keysym.sym == SDLK_KP_ENTER)) {
            /* Tasto Enter: Retry immediato */
            start_connect_worker();
            return;
        } else if (ev->key.keysym.sym == SDLK_ESCAPE) {
            /* Tasto ESC: Cancel o Close */
            linux_conn_done_cb cb = g_done_cb;
            void *data = g_cb_data;
            linux_conn_close();
            if (cb) cb(false, data);
            return;
        }
    }
}

void linux_conn_render(void) {
    if (!g_conn_win || !g_conn_ren) return;

    if (g_state == CONN_STATE_SUCCESS) {
        linux_conn_done_cb cb = g_done_cb;
        void *data = g_cb_data;
        linux_conn_close();
        if (cb) cb(true, data);
        return;
    }

    linux_theme_colors_t c = linux_theme_current();

    SDL_SetRenderDrawColor(g_conn_ren, c.bg_dark.r, c.bg_dark.g, c.bg_dark.b, 255);
    SDL_RenderClear(g_conn_ren);

    /* Riquadro contenitore retro */
    SDL_Rect box_rc = { 16, 16, CONN_WIN_W - 32, CONN_WIN_H - 32 };
    SDL_SetRenderDrawColor(g_conn_ren, c.bg_box.r, c.bg_box.g, c.bg_box.b, 255);
    SDL_RenderFillRect(g_conn_ren, &box_rc);
    SDL_SetRenderDrawColor(g_conn_ren, c.border.r, c.border.g, c.border.b, 255);
    SDL_RenderDrawRect(g_conn_ren, &box_rc);

    if (g_state == CONN_STATE_CONNECTING) {
        const char *title = "[ CONNECTING TO PLATO ]";
        float tw = get_string_width_f(title, -1);
        draw_text_line((CONN_WIN_W - tw) / 2.0f, 48.0f, title, c.bright);

        char prof_line[128];
        snprintf(prof_line, sizeof(prof_line), "Profile: %s", g_profile.name);
        draw_text_line(40.0f, 85.0f, prof_line, c.normal);

        char srv_line[128];
        snprintf(srv_line, sizeof(srv_line), "Target:  %s:%d", g_profile.host, g_profile.port);
        draw_text_line(40.0f, 112.0f, srv_line, c.muted);

        /* Indicatore di avanzamento retro animato */
        uint32_t step = ((SDL_GetTicks() - g_anim_start_tick) / 200) % 4;
        const char *dots[4] = { "[ . . . ]", "[ * . . ]", "[ * * . ]", "[ * * * ]" };
        draw_text_line(40.0f, 138.0f, dots[step], c.bright);

        /* Pulsante [ Cancel (ESC) ] */
        SDL_Rect btn_rc = { 180, 165, 140, 32 };
        if (g_hover_btn_idx == 1) {
            SDL_SetRenderDrawColor(g_conn_ren, c.hover_bg.r, c.hover_bg.g, c.hover_bg.b, 255);
        } else {
            SDL_SetRenderDrawColor(g_conn_ren, c.bg_dark.r, c.bg_dark.g, c.bg_dark.b, 255);
        }
        SDL_RenderFillRect(g_conn_ren, &btn_rc);
        SDL_SetRenderDrawColor(g_conn_ren, c.border.r, c.border.g, c.border.b, 255);
        SDL_RenderDrawRect(g_conn_ren, &btn_rc);

        const char *btn_txt = "Cancel (ESC)";
        float bw = get_string_width_f(btn_txt, -1);
        draw_text_line(btn_rc.x + (btn_rc.w - bw) / 2.0f, btn_rc.y + 22.0f, btn_txt, c.bright);
    } else if (g_state == CONN_STATE_FAILED) {
        const char *title = "[ CONNECTION FAILED ]";
        float tw = get_string_width_f(title, -1);
        draw_text_line((CONN_WIN_W - tw) / 2.0f, 48.0f, title, c.bright);

        char srv_line[128];
        snprintf(srv_line, sizeof(srv_line), "Target: %s:%d", g_profile.host, g_profile.port);
        draw_text_line(40.0f, 85.0f, srv_line, c.muted);

        char err_line[128];
        const char *err = (g_transport && g_transport->last_error[0]) ? g_transport->last_error : "Connection refused";
        snprintf(err_line, sizeof(err_line), "Error:  %s", err);
        draw_text_line(40.0f, 112.0f, err_line, c.normal);

        draw_text_line(40.0f, 138.0f, "Press Retry or Close to select another profile.", c.dim);

        /* Tasto 1: [ Retry (Enter) ] */
        SDL_Rect rc_retry = { 90, 165, 140, 32 };
        if (g_hover_btn_idx == 1) {
            SDL_SetRenderDrawColor(g_conn_ren, c.hover_bg.r, c.hover_bg.g, c.hover_bg.b, 255);
        } else {
            SDL_SetRenderDrawColor(g_conn_ren, c.bg_dark.r, c.bg_dark.g, c.bg_dark.b, 255);
        }
        SDL_RenderFillRect(g_conn_ren, &rc_retry);
        SDL_SetRenderDrawColor(g_conn_ren, c.border.r, c.border.g, c.border.b, 255);
        SDL_RenderDrawRect(g_conn_ren, &rc_retry);

        const char *txt_retry = "Retry (Enter)";
        float rw = get_string_width_f(txt_retry, -1);
        draw_text_line(rc_retry.x + (rc_retry.w - rw) / 2.0f, rc_retry.y + 22.0f, txt_retry, c.bright);

        /* Tasto 2: [ Close (ESC) ] */
        SDL_Rect rc_close = { 270, 165, 140, 32 };
        if (g_hover_btn_idx == 2) {
            SDL_SetRenderDrawColor(g_conn_ren, c.hover_bg.r, c.hover_bg.g, c.hover_bg.b, 255);
        } else {
            SDL_SetRenderDrawColor(g_conn_ren, c.bg_dark.r, c.bg_dark.g, c.bg_dark.b, 255);
        }
        SDL_RenderFillRect(g_conn_ren, &rc_close);
        SDL_SetRenderDrawColor(g_conn_ren, c.border.r, c.border.g, c.border.b, 255);
        SDL_RenderDrawRect(g_conn_ren, &rc_close);

        const char *txt_close = "Close (ESC)";
        float cw = get_string_width_f(txt_close, -1);
        draw_text_line(rc_close.x + (rc_close.w - cw) / 2.0f, rc_close.y + 22.0f, txt_close, c.bright);
    }

    SDL_RenderPresent(g_conn_ren);
}
