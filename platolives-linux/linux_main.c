#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#include <sys/resource.h>
#include <unistd.h>
#include <sys/types.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
/*
 * PlatoLives - Desktop GUI Client (SDL2 + OpenGL 3.3 Core Profile)
 * Native Linux GUI with 1:1 Desktop Menus, Clipboard, Dynamic Profiles & Shaders.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define GL_SILENCE_DEPRECATION
#include <SDL2/SDL.h>

#if defined(__APPLE__)
#else
#define GL_GLEXT_PROTOTYPES
#endif

#include "plato/plato_terminal.h"
#include "plato/plato_optical.h"
#include "plato/plato_transport.h"
#include "plato/plato_profile.h"
#include "plato/plato_keyboard.h"
#include "plato/plato_protocol.h"
#include "linux_menu.h"
#include "linux_keyref.h"
#include "linux_profiles.h"
#include "linux_scriptref.h"
#include "linux_scripts.h"
#include "linux_livetext.h"
#include "linux_about.h"
#include "linux_hud.h"
#include "linux_audio.h"
#include "platolives_icon_data.h"
#include "plato/plato_version.h"
#include "plato/plato_script.h"
#include "linux_gl.h"

#define TERMINAL_SIDE_INIT  896
#define WINDOW_WIDTH_INIT   TERMINAL_SIDE_INIT
#define WINDOW_HEIGHT_INIT  (TERMINAL_SIDE_INIT + MENU_BAR_HEIGHT)
#define TEXTURE_WIDTH       PLATO_OPTICAL_WIDTH   /* 2048 */
#define TEXTURE_HEIGHT      PLATO_OPTICAL_HEIGHT  /* 2048 */

static const char *vertex_shader_source =
    "#version 330 core\n"
    "layout (location = 0) in vec2 aPos;\n"
    "layout (location = 1) in vec2 aTexCoord;\n"
    "out vec2 TexCoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "    TexCoord = aTexCoord;\n"
    "}\n";

static const char *fragment_shader_source =
    "#version 330 core\n"
    "in vec2 TexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D screenTexture;\n"
    "void main() {\n"
    "    FragColor = texture(screenTexture, TexCoord);\n"
    "}\n";

typedef struct {
    SDL_Window *window;
    SDL_GLContext gl_context;
    uint32_t last_direct_key_char;
    double last_direct_key_time;
    GLuint shader_program;
    GLuint term_vao, term_vbo;
    GLuint ui_vao, ui_vbo;
    GLuint texture_id;
    GLuint pbo_ids[2];
    size_t pbo_index;
    bool running;

    plato_terminal_t terminal;
    plato_optical_t *optical;
    plato_transport_t *transport;
    plato_profile_t profile;
    plato_keyboard_state_t kbd_state;
    linux_menu_context_t menu_ctx;
    plato_script_runner_t *script_runner;
    volatile bool paste_cancelled;
    uint32_t *render_buffer;

    /* Buffer di ingestione asincrono con pacing storico pterm (8 ms) */
    uint8_t feed_buf[8192];
    size_t feed_len;
    size_t feed_pos;
    double pace_until_sec;

    char pending_mte_path[1024];
    volatile bool has_pending_mte;

    int vp_x, vp_y, vp_side;
} linux_client_t;

static GLuint compile_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    return shader;
}

static GLuint create_shader_program(void) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

static void init_gl_quads(linux_client_t *app) {
    /* 1. Terminal Quad NDC [-1, 1] */
    static const float term_verts[] = {
        -1.0f,  1.0f,     0.0f, 0.0f,
        -1.0f, -1.0f,     0.0f, 1.0f,
         1.0f, -1.0f,     1.0f, 1.0f,

        -1.0f,  1.0f,     0.0f, 0.0f,
         1.0f, -1.0f,     1.0f, 1.0f,
         1.0f,  1.0f,     1.0f, 0.0f
    };

    glGenVertexArrays(1, &app->term_vao);
    glGenBuffers(1, &app->term_vbo);
    glBindVertexArray(app->term_vao);
    glBindBuffer(GL_ARRAY_BUFFER, app->term_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(term_verts), term_verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    /* 2. UI Menu Quad Dinamico */
    glGenVertexArrays(1, &app->ui_vao);
    glGenBuffers(1, &app->ui_vbo);
    glBindVertexArray(app->ui_vao);
    glBindBuffer(GL_ARRAY_BUFFER, app->ui_vbo);
    glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

static void init_gl_streaming(linux_client_t *app) {
    glGenTextures(1, &app->texture_id);
    glBindTexture(GL_TEXTURE_2D, app->texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    size_t buffer_size = TEXTURE_WIDTH * TEXTURE_HEIGHT * 4;
    glGenBuffers(2, app->pbo_ids);
    for (int i = 0; i < 2; ++i) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, app->pbo_ids[i]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, (GLsizeiptr)buffer_size, NULL, GL_STREAM_DRAW);
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    app->pbo_index = 0;
}

static double compute_process_cpu_usage(double elapsed) {
    static struct rusage prev_ru;
    static bool first = true;
    struct rusage cur_ru;
    if (getrusage(RUSAGE_SELF, &cur_ru) != 0) return 0.0;
    if (first) {
        prev_ru = cur_ru;
        first = false;
        return 0.0;
    }
    double user = (double)(cur_ru.ru_utime.tv_sec - prev_ru.ru_utime.tv_sec) +
                  (double)(cur_ru.ru_utime.tv_usec - prev_ru.ru_utime.tv_usec) / 1e6;
    double sys  = (double)(cur_ru.ru_stime.tv_sec - prev_ru.ru_stime.tv_sec) +
                  (double)(cur_ru.ru_stime.tv_usec - prev_ru.ru_stime.tv_usec) / 1e6;
    prev_ru = cur_ru;
    if (elapsed <= 0.0) return 0.0;

    /* Normalizzazione su tutti i core logici disponibili (come Windows Task Manager) */
    int num_cores = SDL_GetCPUCount();
    if (num_cores < 1) num_cores = 1;

    double pct = (((user + sys) / elapsed) * 100.0) / (double)num_cores;
    if (pct < 0.0) pct = 0.0;
    if (pct > 100.0) pct = 100.0;
    return pct;
}

static FILE *s_renderer_log_fp = NULL;

static void set_renderer_log_state(bool enabled) {
    if (enabled && !s_renderer_log_fp) {
        char log_path[1024];
        const char *home = getenv("HOME");
        if (home) {
            snprintf(log_path, sizeof(log_path), "%s/Scrivania/PlatoLives_renderer.log", home);
            s_renderer_log_fp = fopen(log_path, "w");
            if (!s_renderer_log_fp) {
                snprintf(log_path, sizeof(log_path), "%s/Desktop/PlatoLives_renderer.log", home);
                s_renderer_log_fp = fopen(log_path, "w");
            }
            if (!s_renderer_log_fp) {
                snprintf(log_path, sizeof(log_path), "%s/PlatoLives_renderer.log", home);
                s_renderer_log_fp = fopen(log_path, "w");
            }
        }
        if (!s_renderer_log_fp) {
            snprintf(log_path, sizeof(log_path), "PlatoLives_renderer.log");
            s_renderer_log_fp = fopen(log_path, "w");
        }
        if (s_renderer_log_fp) {
            printf("[*] Renderer Performance Log creato in: %s\n", log_path);
            fprintf(s_renderer_log_fp, "=======================================================\n");
            fprintf(s_renderer_log_fp, " PLATOLIVES RENDERER PERFORMANCE TELEMETRY LOG\n");
            fprintf(s_renderer_log_fp, "=======================================================\n\n");
            fflush(s_renderer_log_fp);
        }
    } else if (!enabled && s_renderer_log_fp) {
        fprintf(s_renderer_log_fp, "\n=== RENDERER LOG CLOSED ===\n");
        fclose(s_renderer_log_fp);
        s_renderer_log_fp = NULL;
    }
}

static bool update_and_render_frame(linux_client_t *app, double now_sec) {
    /* 1. Generazione frame ottico terminale e telemetria reale */
    bool fb_was_dirty = app->terminal.fb.dirty;
    bool is_animating = plato_optical_render(app->optical, &app->terminal, &app->profile, now_sec, app->render_buffer);

    /* Indicatore visivo Script in Esecuzione: salvataggio e ripristino fedele dello sfondo */
    static uint32_t s_saved_bg[33 * 33];
    static bool s_has_saved_bg = false;
    static bool s_prev_script_running = false;

    const int dot_cx = TEXTURE_WIDTH - 48;
    const int dot_cy = 48;
    const int dot_r = 16;
    const int dot_box_dim = 2 * dot_r + 1; /* 33 */

    /* Ripristina SEMPRE lo sfondo originale pulito se era stato salvato */
    if (s_has_saved_bg) {
        for (int dy = -dot_r; dy <= dot_r; dy++) {
            for (int dx = -dot_r; dx <= dot_r; dx++) {
                int px = dot_cx + dx;
                int py = dot_cy + dy;
                if (px >= 0 && px < TEXTURE_WIDTH && py >= 0 && py < TEXTURE_HEIGHT) {
                    app->render_buffer[py * TEXTURE_WIDTH + px] = s_saved_bg[(dy + dot_r) * dot_box_dim + (dx + dot_r)];
                }
            }
        }
        s_has_saved_bg = false;
    }

    bool is_z80_running = (app->terminal.mt.running || app->terminal.mt.in_r_exec);
    if (is_z80_running) is_animating = true;

    bool is_script_running = (app->script_runner && plato_script_is_running(app->script_runner));
    if (is_script_running || s_prev_script_running) {
        is_animating = true;
        if (!is_script_running && s_prev_script_running) {
            plato_optical_invalidate(app->optical);
            app->terminal.fb.dirty = true;
        }
    }
    s_prev_script_running = is_script_running;

    if (is_script_running) {
        /* Frequenza lampeggio: ciclo di 400ms (200ms ON / 200ms OFF) */
        bool blink_on = ((uint64_t)(now_sec * 1000.0) % 400) < 200;
        if (blink_on) {
            /* Salva i pixel di sfondo correnti prima di applicare il colore */
            for (int dy = -dot_r; dy <= dot_r; dy++) {
                for (int dx = -dot_r; dx <= dot_r; dx++) {
                    int px = dot_cx + dx;
                    int py = dot_cy + dy;
                    if (px >= 0 && px < TEXTURE_WIDTH && py >= 0 && py < TEXTURE_HEIGHT) {
                        s_saved_bg[(dy + dot_r) * dot_box_dim + (dx + dot_r)] = app->render_buffer[py * TEXTURE_WIDTH + px];
                    }
                }
            }
            s_has_saved_bg = true;

            uint32_t col = 0xFFFF6E00; /* BGRA Orange Plasma */
            for (int dy = -dot_r; dy <= dot_r; dy++) {
                for (int dx = -dot_r; dx <= dot_r; dx++) {
                    if (dx * dx + dy * dy <= dot_r * dot_r) {
                        int px = dot_cx + dx;
                        int py = dot_cy + dy;
                        if (px >= 0 && px < TEXTURE_WIDTH && py >= 0 && py < TEXTURE_HEIGHT) {
                            app->render_buffer[py * TEXTURE_WIDTH + px] = col;
                        }
                    }
                }
            }
        }
    }

    static uint32_t s_total_frames = 0;
    static uint32_t s_partial_frames = 0;
    static uint32_t s_full_frames = 0;
    static double s_last_fps_sample = 0.0;

    if (is_animating) {
        s_total_frames++;
        if (app->profile.display_mode != 1 && app->profile.display_mode != 3) {
            if (plato_optical_is_full_frame(app->optical)) {
                s_full_frames++;
            } else {
                s_partial_frames++;
            }
        }
    }

    if (s_last_fps_sample == 0.0) s_last_fps_sample = now_sec;
    if (now_sec - s_last_fps_sample >= 0.5) {
        double elapsed = now_sec - s_last_fps_sample;
        if (elapsed > 0.0) {
            app->menu_ctx.hud_fps_total = (double)s_total_frames / elapsed;
            app->menu_ctx.hud_fps_partial = (double)s_partial_frames / elapsed;
            app->menu_ctx.hud_fps_full = (double)s_full_frames / elapsed;
        }
        s_total_frames = s_partial_frames = s_full_frames = 0;
        s_last_fps_sample = now_sec;
        app->menu_ctx.hud_cpu_percent = compute_process_cpu_usage(elapsed);
        if (linux_hud_is_open()) {
            linux_hud_telemetry_t telem = {0};
            telem.fps_total = app->menu_ctx.hud_fps_total;
            telem.fps_partial = app->menu_ctx.hud_fps_partial;
            telem.fps_full = app->menu_ctx.hud_fps_full;
            telem.cpu_percent = app->menu_ctx.hud_cpu_percent;
            telem.display_mode = app->profile.display_mode;
            telem.persistence_ms = (app->profile.display_mode == 4 ? app->profile.crt_persistence_ms : app->profile.persistence_ms);
            telem.distortion = (app->profile.display_mode == 4 ? app->profile.crt_distortion : app->profile.plasma_distortion);
            telem.connected = (app->transport && app->transport->connected);
            if (app->terminal.user_name[0] && app->terminal.user_group[0]) {
                snprintf(telem.user_info, sizeof(telem.user_info), "%s/%s (%s)",
                         app->terminal.user_name, app->terminal.user_group, app->terminal.user_station);
            } else if (app->terminal.user_station[0]) {
                snprintf(telem.user_info, sizeof(telem.user_info), "SLOT %s", app->terminal.user_station);
            } else {
                snprintf(telem.user_info, sizeof(telem.user_info), "STANDALONE");
            }
            linux_hud_update_data(&telem);
        }
        if (app->menu_ctx.renderer_log) {
            printf("[Renderer Profiler] FPS: %.1f (Full: %.1f, Part: %.1f) | CPU: %.1f%% | Mode: %d | Decay: %d ms | Distort: %d\n",
                   app->menu_ctx.hud_fps_total, app->menu_ctx.hud_fps_full, app->menu_ctx.hud_fps_partial,
                   app->menu_ctx.hud_cpu_percent, app->profile.display_mode,
                   (app->profile.display_mode == 4 ? app->profile.crt_persistence_ms : app->profile.persistence_ms),
                   (app->profile.display_mode == 4 ? app->profile.crt_distortion : app->profile.plasma_distortion));
            fflush(stdout);
            if (s_renderer_log_fp) {
                fprintf(s_renderer_log_fp, "[Telemetry] FPS: %.1f (Full: %.1f, Part: %.1f) | CPU: %.1f%% | Mode: %d | Decay: %d ms | Distort: %d\n",
                        app->menu_ctx.hud_fps_total, app->menu_ctx.hud_fps_full, app->menu_ctx.hud_fps_partial,
                        app->menu_ctx.hud_cpu_percent, app->profile.display_mode,
                        (app->profile.display_mode == 4 ? app->profile.crt_persistence_ms : app->profile.persistence_ms),
                        (app->profile.display_mode == 4 ? app->profile.crt_distortion : app->profile.plasma_distortion));
                fflush(s_renderer_log_fp);
            }
        }
    }

    /* 2. Streaming PBO condizionato con pipeline flush a doppio stadio */
    static bool s_first_frame = true;
    static int s_pbo_cooldown = 0;
    if (fb_was_dirty) {
        s_pbo_cooldown = 2; /* Garantisce il completamento di entrambi gli stadi: CPU->PBO e PBO->Texture */
    }

    if (is_animating || fb_was_dirty || s_pbo_cooldown > 0 || s_first_frame) {
        if (s_pbo_cooldown > 0) s_pbo_cooldown--;
        s_first_frame = false;
        size_t next_index = (app->pbo_index + 1) % 2;
        size_t buffer_size = TEXTURE_WIDTH * TEXTURE_HEIGHT * 4;

        glBindTexture(GL_TEXTURE_2D, app->texture_id);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, app->pbo_ids[app->pbo_index]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, TEXTURE_WIDTH, TEXTURE_HEIGHT, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, 0);

        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, app->pbo_ids[next_index]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, (GLsizeiptr)buffer_size, NULL, GL_STREAM_DRAW);
        void *ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
        if (ptr) {
            memcpy(ptr, app->render_buffer, buffer_size);
            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        }
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        app->pbo_index = next_index;
    }

    if (s_pbo_cooldown > 0) {
        is_animating = true; /* Mantiene attivo il loop per completare lo stage GPU al frame successivo */
    }

    /* 3. Disegno Viewport Terminale con Aspect Ratio 1:1 */
    int draw_w, draw_h;
    SDL_GL_GetDrawableSize(app->window, &draw_w, &draw_h);
    int win_w, win_h;
    SDL_GetWindowSize(app->window, &win_w, &win_h);

    glViewport(0, 0, draw_w, draw_h);
    glClearColor(0.06f, 0.04f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    float scale_y = (win_h > 0) ? ((float)draw_h / (float)win_h) : 1.0f;
    /* In modalità Hide Menu Bar, il terminale è SEMPRE a schermo intero senza sobbalzi */
    int menu_bar_pixels = (!app->menu_ctx.hide_menu_bar) ? (int)(MENU_BAR_HEIGHT * scale_y) : 0;
    int avail_h = draw_h - menu_bar_pixels;
    app->vp_side = (draw_w < avail_h) ? draw_w : avail_h;
    app->vp_x = (draw_w - app->vp_side) / 2;
    app->vp_y = (avail_h - app->vp_side) / 2;

    glViewport(app->vp_x, app->vp_y, app->vp_side, app->vp_side);
    glUseProgram(app->shader_program);
    glBindVertexArray(app->term_vao);
    glBindTexture(GL_TEXTURE_2D, app->texture_id);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    /* 4. Disegno Barra dei Menu e Sottomenu ad Alta Risoluzione */
    app->menu_ctx.connected = (app->transport && app->transport->connected);
    linux_menu_update_surface(&app->menu_ctx, &app->profile, &app->terminal, win_w, win_h);

    /* Aggiornamento animazione fluida slide (250ms) */
    float target_anim = 1.0f;
    if (app->menu_ctx.hide_menu_bar) {
        /* Controllo scadenza timeout di 2.0 secondi */
        if (!app->menu_ctx.is_open && now_sec >= app->menu_ctx.menu_hide_deadline) {
            app->menu_ctx.menu_bar_revealed = false;
        }
        target_anim = (app->menu_ctx.menu_bar_revealed || app->menu_ctx.is_open) ? 1.0f : 0.0f;
    }

    float anim_speed = 4.0f; /* 250ms per transizione completa */
    static double s_last_anim_time = 0.0;
    if (s_last_anim_time == 0.0) s_last_anim_time = now_sec;
    float dt_anim = (float)(now_sec - s_last_anim_time);
    s_last_anim_time = now_sec;
    if (dt_anim < 0.0f) dt_anim = 0.0f;
    if (dt_anim > 0.05f) dt_anim = 0.05f;

    if (app->menu_ctx.menu_anim_y < target_anim) {
        app->menu_ctx.menu_anim_y += dt_anim * anim_speed;
        if (app->menu_ctx.menu_anim_y > target_anim) app->menu_ctx.menu_anim_y = target_anim;
        is_animating = true;
    } else if (app->menu_ctx.menu_anim_y > target_anim) {
        app->menu_ctx.menu_anim_y -= dt_anim * anim_speed;
        if (app->menu_ctx.menu_anim_y < target_anim) app->menu_ctx.menu_anim_y = target_anim;
        is_animating = true;
    }

    if (app->menu_ctx.hide_menu_bar && now_sec < app->menu_ctx.menu_hide_deadline) {
        is_animating = true; /* Mantiene loop fluido finché il grace timer di 2s è attivo */
    }

    bool bar_visible = (!app->menu_ctx.hide_menu_bar || app->menu_ctx.menu_anim_y > 0.001f);

    if (bar_visible && app->menu_ctx.ui_height > 0) {
        glViewport(0, 0, draw_w, draw_h);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        float ui_h = (float)app->menu_ctx.ui_height;
        float height_ndc = (ui_h / (float)win_h) * 2.0f;

        /* Calcolo traslazione verticale NDC per slide overlay fluido */
        float slide_offset_ndc = 0.0f;
        if (app->menu_ctx.hide_menu_bar) {
            slide_offset_ndc = (1.0f - app->menu_ctx.menu_anim_y) * height_ndc;
        }

        float y_top_ndc = 1.0f + slide_offset_ndc;
        float y_bottom_ndc = y_top_ndc - height_ndc;

        float ui_verts[] = {
            -1.0f,  y_top_ndc,     0.0f, 0.0f,
            -1.0f,  y_bottom_ndc,  0.0f, 1.0f,
             1.0f,  y_bottom_ndc,  1.0f, 1.0f,

            -1.0f,  y_top_ndc,     0.0f, 0.0f,
             1.0f,  y_bottom_ndc,  1.0f, 1.0f,
             1.0f,  y_top_ndc,     1.0f, 0.0f
        };

        glBindVertexArray(app->ui_vao);
        glBindBuffer(GL_ARRAY_BUFFER, app->ui_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(ui_verts), ui_verts);

        glBindTexture(GL_TEXTURE_2D, app->menu_ctx.ui_texture_id);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glDisable(GL_BLEND);
    }

    SDL_GL_SwapWindow(app->window);
    return is_animating;
}

static char g_binary_path[1024] = {0};

typedef struct {
    linux_client_t *app;
    char *text;
} linux_paste_task_t;

static void* linux_paste_worker(void *arg) {
    linux_paste_task_t *task = (linux_paste_task_t *)arg;
    linux_client_t *app = task->app;
    char *text = task->text;

    size_t len = strlen(text);
    for (size_t i = 0; i < len && app->running && !app->paste_cancelled && app->transport && app->transport->connected; i++) {
        char ch = text[i];
        if (ch == '\r') continue;
        if (ch == '\n') {
            plato_protocol_send_key(&app->terminal, plato_keyboard_keycode(PLATO_KEY_NEXT, false));
            /* Attesa canonica di 500ms su NEXT a step da 10ms (1:1 con Mac e Windows) */
            for (int s = 0; s < 50 && !app->paste_cancelled && app->running; s++) {
                usleep(10000);
            }
        } else if ((unsigned char)ch >= 32 && (unsigned char)ch <= 126) {
            uint8_t b = (uint8_t)ch;
            plato_transport_send(app->transport, &b, 1);
            /* Attesa canonica di 250ms su carattere a step da 10ms (1:1 con Mac e Windows) */
            for (int s = 0; s < 25 && !app->paste_cancelled && app->running; s++) {
                usleep(10000);
            }
        }
    }

    free(text);
    free(task);
    return NULL;
}

static void linux_paste_clipboard_text(linux_client_t *app) {
    if (!app->transport || !app->transport->connected) return;
    if (!SDL_HasClipboardText()) return;

    char *clip = SDL_GetClipboardText();
    if (!clip || !clip[0]) {
        if (clip) SDL_free(clip);
        return;
    }

    app->paste_cancelled = false;
    linux_paste_task_t *task = (linux_paste_task_t *)malloc(sizeof(linux_paste_task_t));
    if (!task) { SDL_free(clip); return; }

    task->app = app;
    task->text = strdup(clip);
    SDL_free(clip);

    pthread_t paste_th;
    pthread_create(&paste_th, NULL, linux_paste_worker, task);
    pthread_detach(paste_th);
}

static bool linux_open_mte_file(char *out_path, size_t max_len) {
    if (!out_path || max_len == 0) return false;
    out_path[0] = '\0';

    /* Tentativo 1: Zenity (GNOME / Ubuntu Desktop) */
    FILE *fp = popen("zenity --file-selection --title=\"Select MicroTutor Executable (.mte) Floppy Image\" --file-filter=\"MicroTutor Executable (*.mte) | *.mte\" --file-filter=\"All Files | *\" 2>/dev/null", "r");
    if (fp) {
        if (fgets(out_path, (int)max_len, fp) != NULL) {
            size_t len = strlen(out_path);
            while (len > 0 && (out_path[len - 1] == '\n' || out_path[len - 1] == '\r')) {
                out_path[--len] = '\0';
            }
            pclose(fp);
            if (len > 0) return true;
        } else {
            pclose(fp);
        }
    }

    /* Tentativo 2: Kdialog (KDE Plasma) */
    fp = popen("kdialog --getopenfilename . \"*.mte\" 2>/dev/null", "r");
    if (fp) {
        if (fgets(out_path, (int)max_len, fp) != NULL) {
            size_t len = strlen(out_path);
            while (len > 0 && (out_path[len - 1] == '\n' || out_path[len - 1] == '\r')) {
                out_path[--len] = '\0';
            }
            pclose(fp);
            if (len > 0) return true;
        } else {
            pclose(fp);
        }
    }

    return false;
}

typedef struct {
    linux_client_t *app;
} linux_mte_task_t;

static void* linux_mte_dialog_worker(void *arg) {
    linux_mte_task_t *task = (linux_mte_task_t *)arg;
    linux_client_t *app = task->app;
    char path[1024] = {0};

    if (linux_open_mte_file(path, sizeof(path))) {
        snprintf(app->pending_mte_path, sizeof(app->pending_mte_path), "%s", path);
        app->has_pending_mte = true;
    }
    free(task);
    return NULL;
}


static void linux_copy_screen_image(linux_client_t *app) {
    if (!app || !app->render_buffer) return;
    const char *png_path = "/tmp/platolives_screen.png";
    const char *bmp_path = "/tmp/platolives_screen.bmp";

    plato_script_save_png(png_path, TEXTURE_WIDTH, TEXTURE_HEIGHT, app->render_buffer);
    plato_script_save_bmp(bmp_path, TEXTURE_WIDTH, TEXTURE_HEIGHT, app->render_buffer);

    /* Tentativo 1: Wayland clipboard nativo (wl-copy) */
    int r = system("wl-copy -t image/png < /tmp/platolives_screen.png 2>/dev/null");

    /* Tentativo 2: X11 clipboard nativo (xclip) */
    if (r != 0) {
        r = system("xclip -selection clipboard -t image/png -i /tmp/platolives_screen.png 2>/dev/null");
    }

    /* Tentativo 3: GNOME Desktop Clipboard via Python3 GdkPixbuf (preinstallato su Ubuntu) */
    if (r != 0) {
        r = system("python3 -c \""
                   "import gi; "
                   "gi.require_version('Gtk', '3.0'); "
                   "from gi.repository import Gtk, GdkPixbuf; "
                   "cb = Gtk.Clipboard.get(Gtk.gdk.SELECTION_CLIPBOARD if hasattr(Gtk, 'gdk') else Gtk.Clipboard.get); "
                   "pb = GdkPixbuf.Pixbuf.new_from_file('/tmp/platolives_screen.png'); "
                   "cb = Gtk.Clipboard.get_default(Gtk.gdk.Display.get_default() if hasattr(Gtk, 'gdk') else None); "
                   "cb.set_image(pb); cb.store()\" 2>/dev/null");
    }

    if (r == 0) {
        printf("[Clipboard] Immagine copiata negli appunti con successo (salvata anche in %s).\n", png_path);
    } else {
        printf("[Clipboard] Immagine salvata in %s (Installa 'wl-clipboard' o 'xclip' per la sincronizzazione diretta negli appunti: sudo apt install xclip wl-clipboard).\n", png_path);
    }
}


static void linux_launch_instance(const char *profile_name) {
    char exe_path[1024];
    memset(exe_path, 0, sizeof(exe_path));

#if defined(__APPLE__)
    uint32_t size = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &size) != 0) {
        if (g_binary_path[0]) snprintf(exe_path, sizeof(exe_path), "%s", g_binary_path);
        else snprintf(exe_path, sizeof(exe_path), "./build/PlatoLives-linux");
    }
#else
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len > 0) {
        exe_path[len] = '\0';
    } else {
        if (g_binary_path[0]) snprintf(exe_path, sizeof(exe_path), "%s", g_binary_path);
        else snprintf(exe_path, sizeof(exe_path), "./build/PlatoLives-linux-gui-arm64");
    }
#endif

    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        if (profile_name && strlen(profile_name) > 0) {
            char *args[] = { exe_path, (char *)"--profile", (char *)profile_name, NULL };
            execv(exe_path, args);
        } else {
            char *args[] = { exe_path, NULL };
            execv(exe_path, args);
        }
        _exit(1);
    }
}

static int handle_network_io(linux_client_t *app, double now_sec) {
    if (!app->transport || !app->transport->connected) return 0;

    /* Se e attiva la pausa di pacing storico (8 ms per NUL), non elaborare nuovi byte */
    if (now_sec < app->pace_until_sec) {
        return (app->feed_pos < app->feed_len) ? 1 : 0;
    }

    int total_processed = 0;

    while (1) {
        /* Se il buffer locale e esaurito, leggi nuovi dati dal socket */
        if (app->feed_pos >= app->feed_len) {
            app->feed_pos = 0;
            app->feed_len = 0;
            if (plato_transport_poll_read(app->transport, 0) > 0) {
                int n = plato_transport_recv(app->transport, app->feed_buf, sizeof(app->feed_buf));
                if (n > 0) {
                    app->feed_len = (size_t)n;
                }
            }
            if (app->feed_len == 0) break;
        }

        app->terminal.delay_requested = false;
        size_t remaining = app->feed_len - app->feed_pos;
        size_t consumed = plato_terminal_feed(&app->terminal, &app->feed_buf[app->feed_pos], remaining);
        app->feed_pos += consumed;
        total_processed += (int)consumed;

        if (app->terminal.delay_requested) {
            app->terminal.delay_requested = false;
            /* Pausa di pacing di 8 ms (conforme alla formula storica di pterm) */
            app->pace_until_sec = now_sec + 0.008;
            break;
        }
    }

    return total_processed;
}

static void linux_reset_terminal(linux_client_t *app) {
    if (!app) return;
    if (app->transport && app->transport->connected) {
        plato_transport_disconnect(app->transport);
    }
    app->menu_ctx.connected = false;
    plato_terminal_reset(&app->terminal);
    plato_keyboard_state_init(&app->kbd_state);
    app->feed_len = 0;
    app->feed_pos = 0;
    app->pace_until_sec = 0.0;
    if (app->render_buffer) {
        memset(app->render_buffer, 0, TEXTURE_WIDTH * TEXTURE_HEIGHT * sizeof(uint32_t));
    }
    if (app->optical) {
        plato_optical_invalidate(app->optical);
    }
}

static void linux_connect_profile(linux_client_t *app, const plato_profile_t *p) {
    if (!app || !p) return;

    linux_reset_terminal(app);

    for (size_t i = 0; i < app->menu_ctx.profiles.count; i++) {
        if (strcmp(app->menu_ctx.profiles.profiles[i].name, p->name) == 0) {
            app->menu_ctx.current_profile_idx = (int)i;
            break;
        }
    }

    app->profile = *p;
    if (app->profile.crt_persistence_ms == 0) app->profile.crt_persistence_ms = 20;

    bool is_color = (app->profile.display_mode == 3 || app->profile.display_mode == 4);
    plato_terminal_set_color_mode(&app->terminal, is_color);
    plato_optical_invalidate(app->optical);

    if (app->profile.full_screen != app->menu_ctx.fullscreen) {
        app->menu_ctx.fullscreen = app->profile.full_screen;
        SDL_SetWindowFullscreen(app->window, app->menu_ctx.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    }

    if (app->transport) {
        plato_transport_set_logging(app->transport, app->menu_ctx.diag_log);
        printf("[Network] Connessione a %s:%d (Profilo: '%s')...\n", app->profile.host, app->profile.port, app->profile.name);
        if (plato_transport_connect(app->transport, app->profile.host, app->profile.port)) {
            printf("[Network] Connesso con successo al profilo '%s'!\n", app->profile.name);
            app->menu_ctx.connected = true;
            char title[256];
            snprintf(title, sizeof(title), "PlatoLives - %s (%s:%d)", app->profile.name, app->profile.host, app->profile.port);
            SDL_SetWindowTitle(app->window, title);
        } else {
            printf("[Network] Profilo '%s': %s:%d offline\n", app->profile.name, app->profile.host, app->profile.port);
            app->menu_ctx.connected = false;
            char title[256];
            snprintf(title, sizeof(title), "PlatoLives - %s (Offline)", app->profile.name);
            SDL_SetWindowTitle(app->window, title);
        }
    }
}

static void on_script_dialog_run(const plato_script_t *s, void *user_data) {
    linux_client_t *app = (linux_client_t *)user_data;
    if (app && app->script_runner && s) {
        printf("[Script] Avvio manuale script '%s'...\n", s->name);
        plato_script_start_ex(app->script_runner, s);
    }
}

static void on_profile_dialog_connect(const plato_profile_t *p, void *user_data) {
    linux_client_t *app = (linux_client_t *)user_data;
    if (app && p) {
        linux_connect_profile(app, p);
    }
}

static void apply_menu_action(linux_client_t *app, menu_action_code_t action) {
    /* 1. Same window with profile */
    if (action >= ACT_CONN_PROFILE_BASE && action < ACT_CONN_PROFILE_BASE + 32) {
        int p_idx = (int)(action - ACT_CONN_PROFILE_BASE);
        if (p_idx >= 0 && (size_t)p_idx < app->menu_ctx.profiles.count) {
            linux_connect_profile(app, &app->menu_ctx.profiles.profiles[p_idx]);
        }
        return;
    }

    /* 2. New window with profile */
    if (action >= ACT_CONN_NEW_WIN_PROFILE_BASE && action < ACT_CONN_NEW_WIN_PROFILE_BASE + 32) {
        int p_idx = (int)(action - ACT_CONN_NEW_WIN_PROFILE_BASE);
        if (p_idx >= 0 && (size_t)p_idx < app->menu_ctx.profiles.count) {
            linux_launch_instance(app->menu_ctx.profiles.profiles[p_idx].name);
        }
        return;
    }

    /* Esecuzione Script Dinamici da Menu */
    if (action >= ACT_SCRIPTS_EXEC_BASE && action < ACT_SCRIPTS_EXEC_BASE + 100) {
        size_t s_idx = (size_t)(action - ACT_SCRIPTS_EXEC_BASE);
        if (app->menu_ctx.scripts && s_idx < app->menu_ctx.scripts->count) {
            if (app->script_runner) {
                const plato_script_t *s = &app->menu_ctx.scripts->scripts[s_idx];
                printf("[Script] Avvio script '%s' da menu...\n", s->name);
                plato_script_start_ex(app->script_runner, s);
            }
        }
        return;
    }

    switch (action) {
                case ACT_SCRIPTS_MANAGE:
            linux_scripts_show(app->menu_ctx.scripts, on_script_dialog_run, app);
            break;
        case ACT_SCRIPTS_REFERENCE:
            linux_scriptref_toggle();
            break;
        case ACT_SCRIPTS_CANCEL:
            if (app->script_runner && plato_script_is_running(app->script_runner)) {
                printf("[Script] Cancel Script Execution richiesto.\n");
                plato_script_stop(app->script_runner);
            }
            break;
        case ACT_CONN_MANAGE_PROFILES:
            linux_profiles_show(&app->menu_ctx.profiles, on_profile_dialog_connect, app);
            break;
        case ACT_CONN_DEFAULT: {
            plato_profile_t *def = plato_profiles_get_default(&app->menu_ctx.profiles);
            if (def) linux_connect_profile(app, def);
            else linux_connect_profile(app, &app->profile);
            break;
        }
        case ACT_CONN_NEW_WIN_DEFAULT:
            linux_launch_instance(NULL);
            break;
        case ACT_CONN_DISCONNECT:
            if (app->transport && app->transport->connected) {
                linux_reset_terminal(app);
                printf("[Network] Disconnesso con successo.\n");
                SDL_SetWindowTitle(app->window, "PlatoLives - Disconnected");
            }
            break;
        case ACT_CONN_CLOSE_WINDOW:
        case ACT_CONN_EXIT:
            app->running = false;
            break;

        /* View Modes con Riconnessione Automatica Plasma <-> Colore */
        case ACT_VIEW_CRISP_MONO:
        case ACT_VIEW_SPLIT_MONO:
        case ACT_VIEW_REAL_PLASMA:
        case ACT_VIEW_CRISP_COLOR:
        case ACT_VIEW_REAL_CRT: {
            int new_mode = (action == ACT_VIEW_CRISP_MONO) ? 1 :
                           (action == ACT_VIEW_SPLIT_MONO) ? 2 :
                           (action == ACT_VIEW_REAL_PLASMA) ? 0 :
                           (action == ACT_VIEW_CRISP_COLOR) ? 3 : 4;

            int old_mode = app->profile.display_mode;
            bool cur_is_color = (old_mode == 3 || old_mode == 4);
            bool new_is_color = (new_mode == 3 || new_mode == 4);
            bool is_connected = (app->transport && app->transport->connected);

            app->profile.display_mode = new_mode;

            if (is_connected && (cur_is_color != new_is_color)) {
                printf("[Display] Commutazione Mono <-> Color: Riconnessione automatica al mainframe Cyber1...\n");
                plato_transport_disconnect(app->transport);
                plato_terminal_reset(&app->terminal);
                plato_keyboard_state_init(&app->kbd_state);
                plato_terminal_set_color_mode(&app->terminal, new_is_color);
                plato_optical_invalidate(app->optical);
                app->terminal.fb.dirty = true;
                plato_transport_connect(app->transport, app->profile.host, app->profile.port);
            } else {
                plato_terminal_set_color_mode(&app->terminal, new_is_color);
                plato_optical_invalidate(app->optical);
                app->terminal.fb.dirty = true;
            }
            goto sync_optical_view;
        }

        /* Plasma Decay */
        case ACT_VIEW_PLASMA_DECAY_100: app->profile.persistence_ms = 100; goto sync_optical_view;
        case ACT_VIEW_PLASMA_DECAY_200: app->profile.persistence_ms = 200; goto sync_optical_view;
        case ACT_VIEW_PLASMA_DECAY_500: app->profile.persistence_ms = 500; goto sync_optical_view;
        case ACT_VIEW_PLASMA_DECAY_1000: app->profile.persistence_ms = 1000; goto sync_optical_view;
        case ACT_VIEW_PLASMA_DECAY_2000: app->profile.persistence_ms = 2000; goto sync_optical_view;
        case ACT_VIEW_PLASMA_DECAY_5000: app->profile.persistence_ms = 5000; goto sync_optical_view;

        /* Plasma Distortion */
        case ACT_VIEW_PLASMA_DIST_NONE: app->profile.plasma_distortion = 0; goto sync_optical_view;
        case ACT_VIEW_PLASMA_DIST_BARREL: app->profile.plasma_distortion = 1; goto sync_optical_view;
        case ACT_VIEW_PLASMA_DIST_CYL: app->profile.plasma_distortion = 2; goto sync_optical_view;

        /* CRT Beam */
        case ACT_VIEW_CRT_BEAM_STD: app->profile.crt_beam_level = 0; goto sync_optical_view;
        case ACT_VIEW_CRT_BEAM_HIGH: app->profile.crt_beam_level = 1; goto sync_optical_view;
        case ACT_VIEW_CRT_BEAM_ULTRA: app->profile.crt_beam_level = 2; goto sync_optical_view;

        /* CRT Decay */
        case ACT_VIEW_CRT_DECAY_20: app->profile.crt_persistence_ms = 20; goto sync_optical_view;
        case ACT_VIEW_CRT_DECAY_200: app->profile.crt_persistence_ms = 200; goto sync_optical_view;
        case ACT_VIEW_CRT_DECAY_500: app->profile.crt_persistence_ms = 500; goto sync_optical_view;
        case ACT_VIEW_CRT_DECAY_1000: app->profile.crt_persistence_ms = 1000; goto sync_optical_view;
        case ACT_VIEW_CRT_DECAY_2000: app->profile.crt_persistence_ms = 2000; goto sync_optical_view;
        case ACT_VIEW_CRT_DECAY_5000: app->profile.crt_persistence_ms = 5000; goto sync_optical_view;

        /* CRT Distortion */
        case ACT_VIEW_CRT_DIST_NONE: app->profile.crt_distortion = 0; goto sync_optical_view;
        case ACT_VIEW_CRT_DIST_BARREL: app->profile.crt_distortion = 1; goto sync_optical_view;
        case ACT_VIEW_CRT_DIST_CYL: app->profile.crt_distortion = 2; goto sync_optical_view;

sync_optical_view:
            plato_optical_invalidate(app->optical);
            app->terminal.fb.dirty = true;
            if (app->menu_ctx.current_profile_idx >= 0 && (size_t)app->menu_ctx.current_profile_idx < app->menu_ctx.profiles.count) {
                app->menu_ctx.profiles.profiles[app->menu_ctx.current_profile_idx] = app->profile;
                plato_profiles_save(&app->menu_ctx.profiles);
            }
            break;

        case ACT_VIEW_KEYBOARD_REF:
            linux_keyref_toggle();
            break;
        case ACT_VIEW_FULLSCREEN:
            app->menu_ctx.fullscreen = !app->menu_ctx.fullscreen;
            SDL_SetWindowFullscreen(app->window, app->menu_ctx.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
            break;
        case ACT_VIEW_FPS_HUD:
            linux_hud_toggle();
            break;
        case ACT_VIEW_HIDE_MENU_BAR:
            app->menu_ctx.hide_menu_bar = !app->menu_ctx.hide_menu_bar;
            if (!app->menu_ctx.hide_menu_bar) {
                app->menu_ctx.menu_bar_revealed = true;
                app->menu_ctx.menu_anim_y = 1.0f;
            } else {
                app->menu_ctx.menu_bar_revealed = false;
                app->menu_ctx.menu_hide_deadline = (double)SDL_GetTicks() / 1000.0;
            }
            break;

        /* Edit / Appunti */
        case ACT_EDIT_COPY_VERBATIM: {
            char txt[4096];
            plato_terminal_get_text_area(&app->terminal, 0, 0, PLATO_COLS - 1, PLATO_ROWS - 1, false, txt, sizeof(txt));
            SDL_SetClipboardText(txt);
            break;
        }
        case ACT_EDIT_COPY_SELECTED:
        case ACT_TOOLS_LIVE_TEXT:
            linux_livetext_toggle(&app->terminal);
            break;
        case ACT_EDIT_COPY_IMAGE:
            linux_copy_screen_image(app);
            break;
        case ACT_EDIT_COPY_COMPACT: {
            char txt[4096];
            plato_terminal_get_text_area(&app->terminal, 0, 0, PLATO_COLS - 1, PLATO_ROWS - 1, true, txt, sizeof(txt));
            SDL_SetClipboardText(txt);
            break;
        }
        case ACT_EDIT_PASTE:
            linux_paste_clipboard_text(app);
            break;
        case ACT_EDIT_CANCEL_PASTE:
            app->paste_cancelled = true;
            printf("[Paste] Cancel Paste richiesto.\n");
            break;
        case ACT_MT_BOOT_MTE: {
            linux_mte_task_t *task = (linux_mte_task_t *)malloc(sizeof(linux_mte_task_t));
            if (task) {
                task->app = app;
                pthread_t th;
                pthread_create(&th, NULL, linux_mte_dialog_worker, task);
                pthread_detach(th);
            }
            break;
        }
        case ACT_MT_TOGGLE_LOG:
            app->menu_ctx.z80_log = !app->menu_ctx.z80_log;
            plato_microtutor_set_logging(&app->terminal.mt, app->menu_ctx.z80_log);
            printf("[MicroTutor] Z80 Diagnostic Log: %s\n", app->menu_ctx.z80_log ? "ABILITATO" : "DISABILITATO");
            break;
        case ACT_TOOLS_DIAG_LOG:
            app->menu_ctx.diag_log = !app->menu_ctx.diag_log;
            if (app->transport) {
                plato_transport_set_logging(app->transport, app->menu_ctx.diag_log);
            }
            printf("[Network] Diagnostic Traffic Log: %s\n", app->menu_ctx.diag_log ? "ABILITATO" : "DISABILITATO");
            fflush(stdout);
            break;
        case ACT_TOOLS_RENDERER_LOG:
            app->menu_ctx.renderer_log = !app->menu_ctx.renderer_log;
            set_renderer_log_state(app->menu_ctx.renderer_log);
            printf("[Renderer] Performance Profiler Log: %s\n", app->menu_ctx.renderer_log ? "ABILITATO" : "DISABILITATO");
            fflush(stdout);
            break;

        case ACT_HELP_ABOUT:
            linux_about_show();
            break;

        default:
            break;
    }
}

static void handle_sdl_key(linux_client_t *app, SDL_KeyboardEvent *key, double now_sec) {
    if (key->type != SDL_KEYDOWN) return;

    SDL_Keymod mod = SDL_GetModState();
    bool shift = (mod & KMOD_SHIFT) != 0;
    bool ctrl  = (mod & KMOD_CTRL) != 0;
    bool alt   = (mod & KMOD_LALT) != 0; /* AltGr (RALT) è per comporre caratteri */
    bool gui   = (mod & KMOD_GUI) != 0;

    if ((ctrl || gui) && shift && key->keysym.sym == SDLK_t) {
        apply_menu_action(app, ACT_TOOLS_LIVE_TEXT);
        return;
    }
    if ((ctrl || gui) && shift && key->keysym.sym == SDLK_x) {
        apply_menu_action(app, ACT_SCRIPTS_CANCEL);
        return;
    }
    if ((ctrl || gui) && shift && key->keysym.sym == SDLK_m) {
        apply_menu_action(app, ACT_VIEW_HIDE_MENU_BAR);
        return;
    }

    /* Scorciatoie dinamiche registrate per gli script abilitati */
    if (app->menu_ctx.scripts && app->script_runner) {
        uint32_t pmods = 0;
        if (ctrl)  pmods |= PLATO_HOTKEY_MOD_CTRL;
        if (shift) pmods |= PLATO_HOTKEY_MOD_SHIFT;
        if (alt)   pmods |= PLATO_HOTKEY_MOD_ALT;
        if (gui)   pmods |= PLATO_HOTKEY_MOD_CMD;

        for (size_t s = 0; s < app->menu_ctx.scripts->count; ++s) {
            const plato_script_t *scr = &app->menu_ctx.scripts->scripts[s];
            if (scr->enabled && scr->hotkey_key > 0 &&
                (uint32_t)key->keysym.sym == scr->hotkey_key &&
                pmods == scr->hotkey_modifiers) {
                printf("[Script] Avvio script '%s' via scorciatoia (%s)...\n", scr->name, scr->hotkey_display);
                plato_script_start_ex(app->script_runner, scr);
                return;
            }
        }
    }

    if (ctrl && key->keysym.sym == SDLK_q) { apply_menu_action(app, ACT_CONN_EXIT); return; }
    if (ctrl && key->keysym.sym == SDLK_d) { apply_menu_action(app, ACT_CONN_DISCONNECT); return; }
    if (ctrl && key->keysym.sym == SDLK_w) { apply_menu_action(app, ACT_CONN_CLOSE_WINDOW); return; }
    if ((gui && key->keysym.sym == SDLK_n) || (ctrl && shift && key->keysym.sym == SDLK_n)) { apply_menu_action(app, ACT_CONN_NEW_WIN_DEFAULT); return; }
    if (ctrl && key->keysym.sym == SDLK_k) { apply_menu_action(app, ACT_VIEW_KEYBOARD_REF); return; }
    if (key->keysym.sym == SDLK_ESCAPE && app->menu_ctx.show_keyref) { app->menu_ctx.show_keyref = false; return; }
    if (key->keysym.sym == SDLK_F11) { apply_menu_action(app, ACT_VIEW_FULLSCREEN); return; }
    if (key->keysym.sym == SDLK_F12) { linux_hud_toggle(); return; }
    if (ctrl && key->keysym.sym == SDLK_c) { apply_menu_action(app, shift ? ACT_EDIT_COPY_COMPACT : ACT_EDIT_COPY_VERBATIM); return; }
    if (ctrl && key->keysym.sym == SDLK_PERIOD) { apply_menu_action(app, ACT_EDIT_CANCEL_PASTE); return; }
    if (ctrl && key->keysym.sym == SDLK_v) { apply_menu_action(app, ACT_EDIT_PASTE); return; }

    plato_key_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.shift = shift;
    ev.ctrl = ctrl;
    ev.alt = alt;
    ev.gui = gui;

    /* Risoluzione immediata dead-key apostrofo per Linux X11/Wayland/UTM (scancode 52) */
    /* Risoluzione dead-key apostrofo (52) e backtick/tilde (53) */
    if (key->keysym.scancode == SDL_SCANCODE_APOSTROPHE || key->keysym.sym == SDLK_QUOTE) {
        if (!ctrl && !gui) {
            ev.vkey = PLATO_VKEY_NONE;
            ev.codepoint = shift ? (uint32_t)'"' : 39; /* 39 = ''' */
            ev.unmod_codepoint = 39;
            app->last_direct_key_char = ev.codepoint;
            app->last_direct_key_time = now_sec;
            plato_keyboard_dispatch(&app->kbd_state, &app->terminal, &ev, now_sec);
            return;
        }
    }
    if (key->keysym.scancode == SDL_SCANCODE_GRAVE || key->keysym.sym == SDLK_BACKQUOTE) {
        if (!ctrl && !gui) {
            ev.vkey = PLATO_VKEY_NONE;
            ev.codepoint = shift ? (uint32_t)'~' : 96; /* 96 = '`' */
            ev.unmod_codepoint = 96;
            app->last_direct_key_char = ev.codepoint;
            app->last_direct_key_time = now_sec;
            plato_keyboard_dispatch(&app->kbd_state, &app->terminal, &ev, now_sec);
            return;
        }
    }
    if (false) {
        if (!ctrl && !alt && !gui) {
            ev.vkey = PLATO_VKEY_NONE;
            ev.codepoint = shift ? (uint32_t)'"' : 39; /* 39 = ''' */
            ev.unmod_codepoint = 39;
            app->last_direct_key_char = ev.codepoint;
            app->last_direct_key_time = now_sec;
            plato_keyboard_dispatch(&app->kbd_state, &app->terminal, &ev, now_sec);
            return;
        }
    }

    switch (key->keysym.sym) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            ev.vkey = PLATO_VKEY_RETURN;
            break;
        case SDLK_BACKSPACE:
            ev.vkey = PLATO_VKEY_BACKSPACE;
            break;
        case SDLK_TAB:
            ev.vkey = PLATO_VKEY_TAB;
            break;
        case SDLK_ESCAPE:
            ev.vkey = PLATO_VKEY_ESCAPE;
            break;
        case SDLK_UP:
            ev.vkey = PLATO_VKEY_UP;
            break;
        case SDLK_DOWN:
            ev.vkey = PLATO_VKEY_DOWN;
            break;
        case SDLK_LEFT:
            ev.vkey = PLATO_VKEY_LEFT;
            break;
        case SDLK_RIGHT:
            ev.vkey = PLATO_VKEY_RIGHT;
            break;
        case SDLK_PAGEUP:
            ev.vkey = PLATO_VKEY_PAGEUP;
            break;
        case SDLK_PAGEDOWN:
            ev.vkey = PLATO_VKEY_PAGEDOWN;
            break;
        case SDLK_HOME:
            ev.vkey = PLATO_VKEY_HOME;
            break;
        case SDLK_END:
            ev.vkey = PLATO_VKEY_END;
            break;
        case SDLK_F1:  ev.vkey = PLATO_VKEY_F1; break;
        case SDLK_F2:  ev.vkey = PLATO_VKEY_F2; break;
        case SDLK_F3:  ev.vkey = PLATO_VKEY_F3; break;
        case SDLK_F4:  ev.vkey = PLATO_VKEY_F4; break;
        case SDLK_F5:  ev.vkey = PLATO_VKEY_F5; break;
        case SDLK_F6:  ev.vkey = PLATO_VKEY_F6; break;
        case SDLK_F7:  ev.vkey = PLATO_VKEY_F7; break;
        case SDLK_F8:  ev.vkey = PLATO_VKEY_F8; break;
        case SDLK_F9:  ev.vkey = PLATO_VKEY_F9; break;
        case SDLK_F10: ev.vkey = PLATO_VKEY_F10; break;
        case SDLK_QUOTE: {
            ev.vkey = PLATO_VKEY_NONE;
            ev.codepoint = shift ? (uint32_t)'"' : 39; /* 39 = ''' */
            ev.unmod_codepoint = 39;
            app->last_direct_key_char = ev.codepoint;
            app->last_direct_key_time = now_sec;
            plato_keyboard_dispatch(&app->kbd_state, &app->terminal, &ev, now_sec);
            return;
        }
        case SDLK_BACKQUOTE: {
            ev.vkey = PLATO_VKEY_NONE;
            ev.codepoint = shift ? (uint32_t)'~' : 96; /* 96 = '`' */
            ev.unmod_codepoint = 96;
            app->last_direct_key_char = ev.codepoint;
            app->last_direct_key_time = now_sec;
            plato_keyboard_dispatch(&app->kbd_state, &app->terminal, &ev, now_sec);
            return;
        }
        default:
            if (ctrl || alt || gui) {
                if (key->keysym.sym >= 'a' && key->keysym.sym <= 'z') {
                    ev.vkey = PLATO_VKEY_NONE;
                    ev.codepoint = (uint32_t)(key->keysym.sym);
                    ev.unmod_codepoint = (uint32_t)(key->keysym.sym);
                    plato_keyboard_dispatch(&app->kbd_state, &app->terminal, &ev, now_sec);
                }
            }
            return;
    }

    plato_keyboard_dispatch(&app->kbd_state, &app->terminal, &ev, now_sec);
}

static void handle_sdl_text(linux_client_t *app, const char *text, double now_sec) {
    if (!text || !text[0]) return;

    SDL_Keymod mod = SDL_GetModState();
    bool shift = (mod & KMOD_SHIFT) != 0;
    bool ctrl  = (mod & KMOD_CTRL) != 0;
    bool alt   = (mod & KMOD_LALT) != 0; /* AltGr (RALT) è per comporre caratteri */
    bool gui   = (mod & KMOD_GUI) != 0;

    const uint8_t *p = (const uint8_t *)text;
    while (*p) {
        uint32_t cp = 0;
        int len = 0;

        if (*p < 0x80) {
            cp = *p;
            len = 1;
        } else if ((*p & 0xE0) == 0xC0) {
            if ((p[1] & 0xC0) == 0x80) {
                cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
                len = 2;
            } else { len = 1; }
        } else if ((*p & 0xF0) == 0xE0) {
            if ((p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
                cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
                len = 3;
            } else { len = 1; }
        } else {
            len = 1;
        }
        p += len;

        /* Normalizzazione avanzata per layout internazionali, dead-keys e VM Linux */
        if (cp == 0x00B4 || cp == 0x2018 || cp == 0x2019 || cp == 0x02BC) {
            cp = 39; /* Normalizza '´' (acuto) e apostrofi curvi a '\'' standard */
        } else if (cp == 0x0060) {
            cp = 96; /* Backtick '`' */
        } else if (cp == 0x201C || cp == 0x201D || cp == 0x00AB || cp == 0x00BB) {
            cp = '"';
        } else if (cp == 0x2013 || cp == 0x2014) {
            cp = '-';
        } else if (cp == 0x00E0 || cp == 0x00E1) {
            cp = 'a';
        } else if (cp == 0x00E8 || cp == 0x00E9) {
            cp = 'e';
        } else if (cp == 0x00EC || cp == 0x00ED) {
            cp = 'i';
        } else if (cp == 0x00F2 || cp == 0x00F3) {
            cp = 'o';
        } else if (cp == 0x00F9 || cp == 0x00FA) {
            cp = 'u';
        }

        if (cp >= 32 && cp <= 126) {
            /* Debounce dead-keys gestite preventivamente */
            if (cp == app->last_direct_key_char && (now_sec - app->last_direct_key_time) < 0.25) {
                app->last_direct_key_char = 0;
                continue;
            }

            plato_key_event_t ev;
            memset(&ev, 0, sizeof(ev));
            ev.vkey = PLATO_VKEY_NONE;
            ev.codepoint = cp;
            ev.unmod_codepoint = (cp >= 'A' && cp <= 'Z') ? (cp + 32) : cp;
            ev.shift = shift;
            ev.ctrl = ctrl;
            ev.alt = alt;
            ev.gui = gui;

            plato_keyboard_dispatch(&app->kbd_state, &app->terminal, &ev, now_sec);
        }
    }
}

static void handle_sdl_mouse(linux_client_t *app, int mouse_x, int mouse_y, bool is_down) {
    if (!is_down) return;

    menu_action_code_t act = linux_menu_handle_mouse(&app->menu_ctx, &app->profile, mouse_x, mouse_y, is_down);
    if (act != ACT_NONE) {
        apply_menu_action(app, act);
        return;
    }


    if (mouse_y <= MENU_BAR_HEIGHT || app->vp_side <= 0 || app->menu_ctx.is_open) return;

    int win_w, win_h, draw_w, draw_h;
    SDL_GetWindowSize(app->window, &win_w, &win_h);
    SDL_GL_GetDrawableSize(app->window, &draw_w, &draw_h);
    float scale_x = (win_w > 0) ? ((float)draw_w / (float)win_w) : 1.0f;
    float scale_y = (win_h > 0) ? ((float)draw_h / (float)win_h) : 1.0f;

    int draw_mx = (int)(mouse_x * scale_x);
    int draw_my = draw_h - (int)(mouse_y * scale_y);

    int local_x = draw_mx - app->vp_x;
    int local_y = draw_my - app->vp_y;

    if (local_x < 0 || local_x >= app->vp_side || local_y < 0 || local_y >= app->vp_side) {
        return;
    }

    int plato_x = (local_x * PLATO_WIDTH) / app->vp_side;
    int plato_y = (local_y * PLATO_HEIGHT) / app->vp_side;

    plato_optical_unwarp_touch(&plato_x, &plato_y, 
                               app->profile.display_mode, 
                               app->profile.plasma_distortion, 
                               app->profile.crt_distortion);

    plato_protocol_send_touch(&app->terminal, (int16_t)plato_x, (int16_t)plato_y);
}

int main(int argc, char *argv[]) {
    if (argc > 0 && argv[0]) {
        strncpy(g_binary_path, argv[0], sizeof(g_binary_path) - 1);
    }

    const char *cli_profile = NULL;
    const char *cli_host = NULL;
    int cli_port = 0;
    const char *cli_script = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "-V") == 0) {
            printf("PlatoLives GUI v" PLATO_VERSION_STRING " (Linux/OpenGL)\n");
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("PlatoLives v4.5 - CDC PLATO IV & Cyber1 Terminal Emulator\n\n"
                   "Usage:\n  PlatoLives [options] [host] [port]\n\n"
                   "Options:\n"
                   "  --profile, -p <name>   Load specific connection profile\n"
                   "  --script <file>        Execute a PLATO script on startup\n"
                   "  --script -h            Display PLATO Scripting Engine language reference\n"
                   "  --version, -v          Display application version and exit\n"
                   "  --help, -h             Display this help message and exit\n");
            return 0;
        }
        if ((strcmp(argv[i], "--script") == 0 || strcmp(argv[i], "--test-script") == 0) &&
            i + 1 < argc && (strcmp(argv[i+1], "-h") == 0 || strcmp(argv[i+1], "--help") == 0)) {
            printf("%s\n", plato_script_get_manual_text());
            return 0;
        }
        if (strcmp(argv[i], "--script-help") == 0) {
            printf("%s\n", plato_script_get_manual_text());
            return 0;
        }
        
        if ((strcmp(argv[i], "--profile") == 0 || strcmp(argv[i], "-p") == 0) && i + 1 < argc) {
            cli_profile = argv[++i];
        } else if ((strcmp(argv[i], "--script") == 0 || strcmp(argv[i], "--test-script") == 0) && i + 1 < argc) {
            cli_script = argv[++i];
        } else if (argv[i][0] != '-') {
            if (!cli_host) cli_host = argv[i];
            else if (cli_port == 0) cli_port = atoi(argv[i]);
        }
    }

    printf("[PlatoLives GUI] Avvio client desktop con Barra Menu V4.5 unificata...\n");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "Errore SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    linux_client_t app;
    memset(&app, 0, sizeof(app));

    app.window = SDL_CreateWindow(
        "PlatoLives - Cyber1 Terminal (Linux/OpenGL)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH_INIT, WINDOW_HEIGHT_INIT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!app.window) {
        fprintf(stderr, "Errore creazione finestra SDL: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    /* Impostazione icona ufficiale Mac per finestra, Dock e Alt+Tab */
    SDL_Surface *icon_surface = SDL_CreateRGBSurfaceFrom(
        (void *)g_platolives_icon_rgba,
        ICON_WIDTH, ICON_HEIGHT, 32, ICON_WIDTH * 4,
        0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
    );
    if (icon_surface) {
        SDL_SetWindowIcon(app.window, icon_surface);
        SDL_FreeSurface(icon_surface);
    }

    app.gl_context = SDL_GL_CreateContext(app.window);
    if (app.gl_context) { linux_gl_load_procs(); }
    if (!app.gl_context) {
        fprintf(stderr, "Errore creazione contesto OpenGL: %s\n", SDL_GetError());
        SDL_DestroyWindow(app.window);
        SDL_Quit();
        return 1;
    }

    SDL_GL_SetSwapInterval(1);
    SDL_StartTextInput();

    plato_terminal_init(&app.terminal);
    linux_audio_init();
    app.terminal.beep_callback = linux_audio_beep;
    app.terminal.beep_context = &app;
    app.script_runner = plato_script_create(&app.terminal);
    plato_keyboard_state_init(&app.kbd_state);
    linux_menu_init(&app.menu_ctx);
    app.menu_ctx.menu_anim_y = 1.0f; /* Visibile di default all'avvio */

    /* Profilo Predefinito o Specificato da CLI */
    int selected_idx = -1;
    if (cli_profile) {
        for (size_t i = 0; i < app.menu_ctx.profiles.count; i++) {
            if (strcmp(app.menu_ctx.profiles.profiles[i].name, cli_profile) == 0) {
                selected_idx = (int)i;
                break;
            }
        }
    }
    if (selected_idx < 0) {
        selected_idx = app.menu_ctx.profiles.default_index;
    }

    if (selected_idx >= 0 && (size_t)selected_idx < app.menu_ctx.profiles.count) {
        app.profile = app.menu_ctx.profiles.profiles[selected_idx];
        app.menu_ctx.current_profile_idx = selected_idx;
    } else {
        memset(&app.profile, 0, sizeof(app.profile));
        snprintf(app.profile.name, sizeof(app.profile.name), "Cyber1");
        snprintf(app.profile.host, sizeof(app.profile.host), "%s", CYBER1_DEFAULT_HOST);
        app.profile.port = CYBER1_DEFAULT_PORT;
        app.profile.display_mode = 0; /* Real Plasma */
        app.profile.persistence_ms = 500;
        app.profile.plasma_distortion = 2; /* Cylindrical */
        app.profile.crt_persistence_ms = 20;
        app.profile.crt_distortion = 0;
        app.profile.crt_beam_level = 0;
        app.menu_ctx.current_profile_idx = -1;
    }
    if (app.profile.crt_persistence_ms == 0) app.profile.crt_persistence_ms = 20;

    /* Applica override CLI (Host/Port) */
    if (cli_host) snprintf(app.profile.host, sizeof(app.profile.host), "%s", cli_host);
    if (cli_port > 0) app.profile.port = cli_port;

    app.optical = plato_optical_create();
    app.render_buffer = (uint32_t *)calloc(TEXTURE_WIDTH * TEXTURE_HEIGHT, sizeof(uint32_t));

    app.shader_program = create_shader_program();
    init_gl_quads(&app);
    init_gl_streaming(&app);

    app.transport = plato_transport_create();
    if (app.transport) {
        app.terminal.transport = app.transport;
        linux_connect_profile(&app, &app.profile);
    }

    /* Carica ed esegui script da CLI se richiesto */
    if (cli_script && app.script_runner) {
        FILE *f = fopen(cli_script, "r");
        if (f) {
            plato_script_t temp_script;
            memset(&temp_script, 0, sizeof(temp_script));
            snprintf(temp_script.name, sizeof(temp_script.name), "CLI Script");
            fread(temp_script.body, 1, sizeof(temp_script.body) - 1, f);
            fclose(f);
            plato_script_start_ex(app.script_runner, &temp_script);
            printf("[PlatoLives] Avvio script da linea di comando: %s\n", cli_script);
        } else {
            fprintf(stderr, "[PlatoLives] Errore: Impossibile leggere lo script '%s'\n", cli_script);
        }
    }

    app.running = true;
    const uint32_t frame_delay_ms = 1000 / 60;
    uint32_t last_tick_ms = SDL_GetTicks();

    while (app.running) {
        uint32_t frame_start = SDL_GetTicks();
        int dt_ms = (int)(frame_start - last_tick_ms);
        last_tick_ms = frame_start;
        double now_sec = (double)frame_start / 1000.0;

        bool had_events = false;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            had_events = true;
                        linux_keyref_handle_event(&event);
            linux_profiles_handle_event(&event);
            linux_scriptref_handle_event(&event);
            linux_scripts_handle_event(&event);
            linux_livetext_handle_event(&event);
            linux_about_handle_event(&event);
            linux_hud_handle_event(&event);

            /* Filtro eventi per finestra secondaria: non toccare la finestra principale se l'evento è per Keyref */
            uint32_t main_win_id = SDL_GetWindowID(app.window);
            if (event.type == SDL_WINDOWEVENT && event.window.windowID != main_win_id) continue;
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.windowID != main_win_id) continue;
            if (event.type == SDL_MOUSEMOTION && event.motion.windowID != main_win_id) continue;
            if (event.type == SDL_KEYDOWN && event.key.windowID != main_win_id) continue;

            if (event.type == SDL_QUIT) {
                app.running = false;
            } else if (event.type == SDL_KEYDOWN) {
                handle_sdl_key(&app, &event.key, now_sec);
            } else if (event.type == SDL_TEXTINPUT) {
                handle_sdl_text(&app, event.text.text, now_sec);
            } else if (event.type == SDL_MOUSEMOTION) {
                linux_menu_handle_mouse(&app.menu_ctx, &app.profile, event.motion.x, event.motion.y, false);
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    handle_sdl_mouse(&app, event.button.x, event.button.y, true);
                }
            }
        }

        /* Controllo asincrono selezione file MicroTutor MTE */
        if (app.has_pending_mte) {
            app.has_pending_mte = false;
            if (app.script_runner && plato_script_is_running(app.script_runner)) {
                plato_script_stop(app.script_runner);
            }
            printf("[MicroTutor] Caricamento immagine floppy MTE: %s\n", app.pending_mte_path);
            if (plato_microtutor_boot_mte(&app.terminal.mt, app.pending_mte_path)) {
                printf("[MicroTutor] Boot locale Z80 eseguito con successo!\n");
                plato_optical_invalidate(app.optical);
                app.terminal.fb.dirty = true;
                SDL_SetWindowTitle(app.window, "PlatoLives - MicroTutor Z80 (Local Boot)");
            } else {
                fprintf(stderr, "[MicroTutor] Errore: impossibile eseguire il boot da %s\n", app.pending_mte_path);
            }
        }

        int net_bytes = handle_network_io(&app, now_sec);
        plato_keyboard_poll(&app.kbd_state, &app.terminal, now_sec);
        plato_terminal_tick(&app.terminal, dt_ms);

        app.menu_ctx.is_script_running = (app.script_runner && plato_script_is_running(app.script_runner));
        static bool s_is_animating = true;
        static int s_redraw_cooldown = 0;

        bool has_pending_feed = (app.feed_pos < app.feed_len || now_sec < app.pace_until_sec);
        if (had_events || net_bytes > 0 || has_pending_feed || app.terminal.fb.dirty || s_is_animating) {
            s_redraw_cooldown = 5; /* Mantiene loop fluido finche dura l'animazione */
        }
        if (s_redraw_cooldown > 0) {
            s_redraw_cooldown--;
        }

        bool needs_redraw = (s_redraw_cooldown > 0 || s_is_animating ||
                             app.menu_ctx.is_script_running || app.menu_ctx.is_open ||
                             app.terminal.mt.running || app.terminal.mt.in_r_exec);

        if (needs_redraw) {
            s_is_animating = update_and_render_frame(&app, now_sec);
            linux_keyref_render();
            linux_profiles_render();
            linux_scriptref_render();
            linux_scripts_render();
            linux_livetext_render();
            linux_about_render();
            linux_hud_render();

            uint32_t elapsed = SDL_GetTicks() - frame_start;
            if (elapsed < frame_delay_ms) {
                SDL_Delay(frame_delay_ms - elapsed);
            }
        } else {
            /* IDLE PURO: Salta il rendering GL e SDL_GL_SwapWindow per azzerare l'overhead di Mesa VirGL */
            if (linux_hud_is_open()) {
                linux_hud_render();
            }
            SDL_Delay(10);
        }
    }

    /* Rilascio */
    SDL_StopTextInput();
        linux_keyref_destroy();
    linux_profiles_destroy();
    linux_scriptref_destroy();
    linux_scripts_destroy();
    linux_livetext_destroy();
    linux_about_destroy();
    linux_hud_destroy();
    linux_audio_destroy();
    if (app.script_runner) {
        plato_script_stop(app.script_runner);
        plato_script_destroy(app.script_runner);
        app.script_runner = NULL;
    }
    linux_menu_destroy(&app.menu_ctx);
    if (app.transport) {
        plato_transport_disconnect(app.transport);
        plato_transport_destroy(app.transport);
    }
    if (app.optical) {
        plato_optical_destroy(app.optical);
    }
    free(app.render_buffer);

    glDeleteBuffers(2, app.pbo_ids);
    glDeleteTextures(1, &app.texture_id);
    glDeleteBuffers(1, &app.term_vbo);
    glDeleteVertexArrays(1, &app.term_vao);
    glDeleteBuffers(1, &app.ui_vbo);
    glDeleteVertexArrays(1, &app.ui_vao);
    glDeleteProgram(app.shader_program);

    SDL_GL_DeleteContext(app.gl_context);
    SDL_DestroyWindow(app.window);
    SDL_Quit();

    return 0;
}
