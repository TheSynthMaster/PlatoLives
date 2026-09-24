#ifndef _WIN32
#define _DARWIN_C_SOURCE
#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <signal.h>
#include <pthread.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <time.h>
#include <signal.h>

#ifdef _WIN32
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
static inline void console_sleep_us(unsigned int us) {
    Sleep((DWORD)(us >= 1000 ? us / 1000 : 1));
}
#define usleep(us) console_sleep_us(us)

static inline ssize_t console_platform_write(int fd, const void *buf, size_t count) {
    HANDLE h = (fd == STDOUT_FILENO) ? GetStdHandle(STD_OUTPUT_HANDLE) : GetStdHandle(STD_ERROR_HANDLE);
    if (!h || h == INVALID_HANDLE_VALUE) return -1;
    DWORD written = 0;
    if (!WriteFile(h, buf, (DWORD)count, &written, NULL)) return -1;
    return (ssize_t)written;
}
#define write(fd, buf, count) console_platform_write(fd, buf, count)
#endif

#include "plato/plato_terminal.h"
#include "plato/plato_transport.h"
#include "plato/plato_protocol.h"
#include "plato/plato_keyboard.h"
#include "plato/plato_ringbuf.h"
#include "plato/console_runner.h"

#ifndef _WIN32
static struct termios g_orig_termios;
#else
static DWORD g_orig_in_mode = 0;
static DWORD g_orig_out_mode = 0;
static UINT g_orig_cp = 0;
static UINT g_orig_out_cp = 0;
static HANDLE g_hIn = NULL;
static HANDLE g_hOut = NULL;
#endif
static bool g_raw_enabled = false;
static volatile sig_atomic_t g_quit = 0;
static volatile sig_atomic_t g_resized = 1;
static volatile sig_atomic_t g_status_dirty = 1;
static double g_feedback_until = 0.0;
static char g_feedback_msg[64] = "";
static plato_console_clipboard_cb g_clipboard_cb = NULL;

void plato_console_set_clipboard_callback(plato_console_clipboard_cb cb) {
    g_clipboard_cb = cb;
}

static double get_time_sec(void) {
#ifndef _WIN32
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#else
    static LARGE_INTEGER freq;
    static bool init = false;
    if (!init) {
        QueryPerformanceFrequency(&freq);
        init = true;
    }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart / (double)freq.QuadPart;
#endif
}

static void show_status_feedback(const char *msg, double duration_sec) {
    snprintf(g_feedback_msg, sizeof(g_feedback_msg), "%s", msg);
    g_feedback_until = get_time_sec() + duration_sec;
    g_status_dirty = 1;
}

static void console_restore_terminal(void) {
    if (g_raw_enabled) {
        write(STDOUT_FILENO, "\033[0m\033[?25h\033[?1049l", 19);
#ifndef _WIN32
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
#else
        if (g_hIn) SetConsoleMode(g_hIn, g_orig_in_mode);
        if (g_hOut) SetConsoleMode(g_hOut, g_orig_out_mode);
        if (g_orig_cp) SetConsoleCP(g_orig_cp);
        if (g_orig_out_cp) SetConsoleOutputCP(g_orig_out_cp);
#endif
        g_raw_enabled = false;
    }
}

#ifndef _WIN32
static void console_signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM || sig == SIGHUP) {
        g_quit = 1;
    } else if (sig == SIGWINCH) {
        g_resized = 1;
    }
}
#else
static BOOL WINAPI console_ctrl_handler(DWORD ctrl_type) {
    if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT || ctrl_type == CTRL_CLOSE_EVENT) {
        g_quit = 1;
        return TRUE;
    }
    return FALSE;
}
#endif

static bool console_get_window_size(int *width, int *height) {
#ifndef _WIN32
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        *width = ws.ws_col;
        *height = ws.ws_row;
        return true;
    }
    return false;
#else
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h && h != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(h, &csbi)) {
        *width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        *height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        return true;
    }
    return false;
#endif
}

static bool console_enable_raw_mode(void) {
#ifndef _WIN32
    if (!isatty(STDIN_FILENO)) return false;
    if (tcgetattr(STDIN_FILENO, &g_orig_termios) == -1) return false;

    struct termios raw = g_orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) return false;
    g_raw_enabled = true;
    atexit(console_restore_terminal);

    write(STDOUT_FILENO, "\033[?1049h\033[?25l\033[2J\033[H", 20);
    return true;
#else
    g_hIn = GetStdHandle(STD_INPUT_HANDLE);
    g_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!g_hIn || g_hIn == INVALID_HANDLE_VALUE || !g_hOut || g_hOut == INVALID_HANDLE_VALUE) {
        return false;
    }

    if (!GetConsoleMode(g_hIn, &g_orig_in_mode) || !GetConsoleMode(g_hOut, &g_orig_out_mode)) {
        return false;
    }

    g_orig_cp = GetConsoleCP();
    g_orig_out_cp = GetConsoleOutputCP();
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    DWORD out_mode = g_orig_out_mode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(g_hOut, out_mode);

    DWORD in_mode = ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS;
    SetConsoleMode(g_hIn, in_mode);

    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

    g_raw_enabled = true;
    atexit(console_restore_terminal);

    write(STDOUT_FILENO, "\033[?1049h\033[?25l\033[2J\033[H", 20);

    /* Garanzia altezza minima di 35 righe per accogliere PLATO (32 righe) + bezel + status */
    int cur_w = 80, cur_h = 24;
    if (console_get_window_size(&cur_w, &cur_h)) {
        if (cur_h < 35) {
            int new_h = 35;
            int new_w = (cur_w < 80) ? 80 : cur_w;
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            if (GetConsoleScreenBufferInfo(g_hOut, &csbi)) {
                COORD bsize = csbi.dwSize;
                if (bsize.Y < new_h) bsize.Y = (SHORT)new_h;
                if (bsize.X < new_w) bsize.X = (SHORT)new_w;
                SetConsoleScreenBufferSize(g_hOut, bsize);
                SMALL_RECT r;
                r.Left = 0; r.Top = 0;
                r.Right = (SHORT)(new_w - 1);
                r.Bottom = (SHORT)(new_h - 1);
                SetConsoleWindowInfo(g_hOut, TRUE, &r);
            }
            char rsz[32];
            int rsz_len = snprintf(rsz, sizeof(rsz), "\033[8;%d;%dt", new_h, new_w);
            if (rsz_len > 0) write(STDOUT_FILENO, rsz, rsz_len);
        }
    }
    return true;
#endif
}

static void console_beep_callback(void *context) {
    (void)context;
    write(STDOUT_FILENO, "\a", 1);
}

static void console_metadata_callback(void *context, const char *name, const char *group, const char *system, const char *station) {
    (void)context; (void)name; (void)group; (void)system; (void)station;
    g_status_dirty = 1;
}

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static void send_osc52_copy(const char *text, size_t len) {
    write(STDOUT_FILENO, "\033]52;c;", 7);
    char chunk[4];
    for (size_t i = 0; i < len; i += 3) {
        uint32_t val = (uint8_t)text[i] << 16;
        if (i + 1 < len) val |= (uint8_t)text[i + 1] << 8;
        if (i + 2 < len) val |= (uint8_t)text[i + 2];

        chunk[0] = b64_table[(val >> 18) & 0x3F];
        chunk[1] = b64_table[(val >> 12) & 0x3F];
        chunk[2] = (i + 1 < len) ? b64_table[(val >> 6) & 0x3F] : '=';
        chunk[3] = (i + 2 < len) ? b64_table[val & 0x3F] : '=';
        write(STDOUT_FILENO, chunk, 4);
    }
    write(STDOUT_FILENO, "\a", 1);
}

static void copy_screen_to_clipboard(const plato_terminal_t *term) {
    char buf[16384];
    size_t out_len = 0;
    char tmp[8];

    for (int r = 0; r < PLATO_ROWS; r++) {
        for (int c = 0; c < PLATO_COLS; c++) {
            const char *utf = plato_cell_to_utf8(term, term->text_grid[r][c], tmp);
            size_t ulen = strlen(utf);
            if (out_len + ulen < sizeof(buf) - 2) {
                memcpy(&buf[out_len], utf, ulen);
                out_len += ulen;
            }
        }
        if (r < PLATO_ROWS - 1) {
            buf[out_len++] = '\n';
        }
    }
    buf[out_len] = '\0';

    FILE *f = fopen("screen.txt", "w");
    if (f) {
        fwrite(buf, 1, out_len, f);
        fclose(f);
    }

    if (g_clipboard_cb) {
        g_clipboard_cb(buf, out_len);
    }

#ifdef _WIN32
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, out_len + 1);
        if (hGlob) {
            char *dst = (char *)GlobalLock(hGlob);
            if (dst) {
                memcpy(dst, buf, out_len);
                dst[out_len] = '\0';
                GlobalUnlock(hGlob);
                SetClipboardData(CF_TEXT, hGlob);
            }
        }
        CloseClipboard();
    }
#endif
    send_osc52_copy(buf, out_len);
    show_status_feedback("*** SCHERMO SALVATO IN screen.txt E NEGLI APPUNTI ***", 1.5);
}

static void console_kbd_feedback(const char *msg, double duration_sec, void *user_data) {
    (void)user_data;
    show_status_feedback(msg, duration_sec);
}

typedef struct {
    plato_transport_t *transport;
    plato_ringbuf_t *ringbuf;
    volatile bool running;
} console_net_worker_ctx_t;

#ifndef _WIN32
static void* console_net_worker(void *arg) {
#else
static DWORD WINAPI console_net_worker(LPVOID arg) {
#endif
    console_net_worker_ctx_t *ctx = (console_net_worker_ctx_t *)arg;
    uint8_t buf[4096];
    while (ctx->running) {
        if (ctx->transport && ctx->transport->connected) {
            int n = plato_transport_recv(ctx->transport, buf, sizeof(buf));
            if (n > 0) {
                plato_ringbuf_write(ctx->ringbuf, buf, (size_t)n);
            } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                plato_transport_disconnect(ctx->transport);
                usleep(30000);
            }
        } else {
            usleep(20000);
        }
    }
#ifndef _WIN32
    return NULL;
#else
    return 0;
#endif
}

static void draw_bezel(int offset_x, int offset_y, int term_w, int term_h) {
    if (offset_x < 2 || offset_y < 2 || (offset_x + PLATO_COLS) >= term_w || (offset_y + PLATO_ROWS) >= term_h) {
        return;
    }
    const char *dim_amber = "\033[38;2;120;65;0m";
    write(STDOUT_FILENO, dim_amber, strlen(dim_amber));

    char buf[512];
    int top_r = offset_y - 1;
    int bot_r = offset_y + PLATO_ROWS;
    int left_c = offset_x - 1;
    int right_c = offset_x + PLATO_COLS;

    int len = snprintf(buf, sizeof(buf), "\033[%d;%dH+", top_r, left_c);
    write(STDOUT_FILENO, buf, len);
    for (int c = 0; c < PLATO_COLS; c++) write(STDOUT_FILENO, "-", 1);
    write(STDOUT_FILENO, "+", 1);

    for (int r = offset_y; r < offset_y + PLATO_ROWS; r++) {
        len = snprintf(buf, sizeof(buf), "\033[%d;%dH|", r, left_c);
        write(STDOUT_FILENO, buf, len);
        len = snprintf(buf, sizeof(buf), "\033[%d;%dH|", r, right_c);
        write(STDOUT_FILENO, buf, len);
    }

    len = snprintf(buf, sizeof(buf), "\033[%d;%dH+", bot_r, left_c);
    write(STDOUT_FILENO, buf, len);
    for (int c = 0; c < PLATO_COLS; c++) write(STDOUT_FILENO, "-", 1);
    write(STDOUT_FILENO, "+", 1);

    write(STDOUT_FILENO, "\033[0m", 4);
}

static void render_status_bar(const plato_terminal_t *term, const char *host, int port, int term_w, int status_y) {
    char bar[512];

    if (get_time_sec() < g_feedback_until) {
        snprintf(bar, sizeof(bar), " PLATO | %s", g_feedback_msg);
    } else {
        char user_info[96] = "";
        if (term->user_name[0] && term->user_group[0]) {
            if (term->user_station[0]) {
                snprintf(user_info, sizeof(user_info), "User: %s/%s (%s) | ", term->user_name, term->user_group, term->user_station);
            } else {
                snprintf(user_info, sizeof(user_info), "User: %s/%s | ", term->user_name, term->user_group);
            }
        } else if (term->user_station[0]) {
            snprintf(user_info, sizeof(user_info), "Slot: %s | ", term->user_station);
        }

        if (term_w >= 145) {
            snprintf(bar, sizeof(bar), " PLATO | %s:%d | %sRet:Next(x2:Sh)  F8/ESC:Back(x2:Sh)  F4/^S:Stop(x2:Sh)  F1:Help(x2:Sh)  ^Y:Copy  ^C:Quit",
                     host, port, user_info);
        } else if (term_w >= 95) {
            snprintf(bar, sizeof(bar), " PLATO | %s:%d | %sRet(x2:Sh)  F8:Back(x2:Sh)  F4:Stop(x2:Sh)  ^Y:Copy  ^C:Quit",
                     host, port, user_info);
        } else {
            snprintf(bar, sizeof(bar), " PLATO | %s | %sRet(x2:Sh) F8:Back F4:Stop ^Y:Copy ^C:Quit",
                     host, user_info);
        }
    }

    char out[1024];
    int len = snprintf(out, sizeof(out), "\033[%d;1H\033[7m%s\033[K\033[0m", status_y, bar);
    if (len > 0) write(STDOUT_FILENO, out, len);
}

#ifdef _WIN32
static bool win_console_has_input(HANDLE hIn) {
    DWORD num_events = 0;
    if (!GetNumberOfConsoleInputEvents(hIn, &num_events) || num_events == 0) {
        return false;
    }
    INPUT_RECORD records[32];
    DWORD read_count = 0;
    if (!PeekConsoleInputW(hIn, records, 32, &read_count) || read_count == 0) {
        return false;
    }
    for (DWORD i = 0; i < read_count; i++) {
        if (records[i].EventType == KEY_EVENT) {
            if (records[i].Event.KeyEvent.bKeyDown) {
                return true;
            }
        } else if (records[i].EventType == WINDOW_BUFFER_SIZE_EVENT) {
            g_resized = 1;
        }
    }
    ReadConsoleInputW(hIn, records, read_count, &read_count);
    return false;
}
#endif

static size_t console_read_sequence(int fd, uint8_t *buf, size_t max_len) {
#ifndef _WIN32
    ssize_t n = read(fd, buf, 1);
    if (n <= 0) return 0;
    size_t total = 1;

    if (buf[0] == 0x1B) {
        struct pollfd pfd = { .fd = fd, .events = POLLIN, .revents = 0 };
        while (total < max_len && poll(&pfd, 1, 60) > 0 && (pfd.revents & POLLIN)) {
            ssize_t m = read(fd, &buf[total], 1);
            if (m <= 0) break;
            total++;
            uint8_t last = buf[total - 1];
            if (total == 2 && last != '[' && last != 'O') {
                break;
            }
            if (total >= 3 && ((last >= '@' && last <= '~') || last == '$')) {
                break;
            }
        }
    }
    return total;
#else
    (void)fd;
    if (!win_console_has_input(g_hIn)) return 0;
    DWORD dwRead = 0;
    if (!ReadFile(g_hIn, buf, 1, &dwRead, NULL) || dwRead == 0) return 0;
    size_t total = 1;

    if (buf[0] == 0x1B) {
        while (total < max_len) {
            DWORD wait_res = WaitForSingleObject(g_hIn, 60);
            if (wait_res != WAIT_OBJECT_0 || !win_console_has_input(g_hIn)) break;
            DWORD m = 0;
            if (!ReadFile(g_hIn, &buf[total], 1, &m, NULL) || m == 0) break;
            total++;
            uint8_t last = buf[total - 1];
            if (total == 2 && last != '[' && last != 'O') {
                break;
            }
            if (total >= 3 && ((last >= '@' && last <= '~') || last == '$')) {
                break;
            }
        }
    }
    return total;
#endif
}

int plato_console_run(const char *host, int port) {
    const char *target_host = (host && host[0]) ? host : CYBER1_DEFAULT_HOST;
    int target_port = (port > 0) ? port : CYBER1_DEFAULT_PORT;

#ifndef _WIN32
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = console_signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGWINCH, &sa, NULL);
#endif

    if (!console_enable_raw_mode()) {
        fprintf(stderr, "[PlatoLives] Errore: impossibile inizializzare la modalita RAW del terminale.\n");
        return 1;
    }

    plato_terminal_t term;
    plato_terminal_init(&term);
    term.beep_callback = console_beep_callback;
    term.metadata_callback = console_metadata_callback;

    plato_ringbuf_t ringbuf;
    plato_ringbuf_init(&ringbuf);

    plato_transport_t *transport = plato_transport_create();
    term.transport = transport;

    console_net_worker_ctx_t net_ctx = {
        .transport = transport,
        .ringbuf = &ringbuf,
        .running = true
    };
    plato_keyboard_state_t kbd_state;
    plato_keyboard_state_init(&kbd_state);
    plato_keyboard_set_feedback_cb(&kbd_state, console_kbd_feedback, NULL);

#ifndef _WIN32
    pthread_t net_thread;
    pthread_create(&net_thread, NULL, console_net_worker, &net_ctx);
#else
    HANDLE net_thread = CreateThread(NULL, 0, console_net_worker, &net_ctx, 0, NULL);
#endif

    bool ok = plato_transport_connect(transport, target_host, target_port);
    if (!ok) {
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "\033[2J\033[10;10H\033[31mErrore di connessione a %s:%d\033[0m\r\n\033[12;10HPremere un tasto per uscire...", target_host, target_port);
        write(STDOUT_FILENO, err_msg, strlen(err_msg));
        char dummy;
        while (!g_quit && read(STDIN_FILENO, &dummy, 1) <= 0) {
            usleep(50000);
        }
        net_ctx.running = false;
#ifndef _WIN32
        pthread_join(net_thread, NULL);
#else
        if (net_thread) {
            WaitForSingleObject(net_thread, INFINITE);
            CloseHandle(net_thread);
        }
#endif
        plato_transport_destroy(transport);
        plato_ringbuf_destroy(&ringbuf);
        console_restore_terminal();
        return 1;
    }

    plato_text_cell_t prev_screen[PLATO_ROWS][PLATO_COLS];
    memset(prev_screen, 0xFF, sizeof(prev_screen));

    int term_w = 80, term_h = 34;
    int offset_x = 1, offset_y = 1;
    int status_y = 34;

    uint8_t feed_buf[4096];
    size_t feed_len = 0;
    size_t feed_pos = 0;

    char render_buf[32768];
    char tmp_cell_str[8];

    while (!g_quit) {
        double now = get_time_sec();

        plato_keyboard_poll(&kbd_state, &term, now);

#ifdef _WIN32
        {
            static int s_prev_cw = 0, s_prev_ch = 0;
            int cw = 0, ch = 0;
            if (console_get_window_size(&cw, &ch)) {
                if (cw != s_prev_cw || ch != s_prev_ch) {
                    s_prev_cw = cw;
                    s_prev_ch = ch;
                    g_resized = 1;
                }
            }
        }
#endif
        if (g_resized) {
            g_resized = 0;
            int cur_w = 80, cur_h = 34;
            if (console_get_window_size(&cur_w, &cur_h)) {
                term_w = cur_w;
                term_h = cur_h;
            }
            offset_x = (term_w > PLATO_COLS) ? ((term_w - PLATO_COLS) / 2 + 1) : 1;
            int avail_h = (term_h > 1) ? (term_h - 1) : term_h;
            offset_y = (avail_h > PLATO_ROWS) ? ((avail_h - PLATO_ROWS) / 2 + 1) : 1;
            status_y = term_h;

            write(STDOUT_FILENO, "\033[2J", 4);
            draw_bezel(offset_x, offset_y, term_w, term_h);
            memset(prev_screen, 0xFF, sizeof(prev_screen));
            g_status_dirty = 1;
        }

        while (1) {
            if (feed_pos >= feed_len) {
                feed_pos = 0;
                feed_len = plato_ringbuf_read(&ringbuf, feed_buf, sizeof(feed_buf));
                if (feed_len == 0) break;
            }
            term.delay_requested = false;
            size_t rem = feed_len - feed_pos;
            size_t consumed = plato_terminal_feed(&term, &feed_buf[feed_pos], rem);
            feed_pos += consumed;
            if (term.delay_requested) {
                term.delay_requested = false;
                usleep(8000);
            }
        }

        size_t r_pos = 0;
        const char *amber_prefix = "\033[38;2;255;140;0m";
        size_t amb_len = strlen(amber_prefix);
        memcpy(&render_buf[r_pos], amber_prefix, amb_len);
        r_pos += amb_len;

        bool drew_any = false;
        for (int r = 0; r < PLATO_ROWS; r++) {
            for (int c = 0; c < PLATO_COLS; c++) {
                plato_text_cell_t cell = term.text_grid[r][c];

                if (prev_screen[r][c].ch != cell.ch || prev_screen[r][c].charset != cell.charset) {
                    prev_screen[r][c] = cell;
                    drew_any = true;

                    const char *glyph_str = plato_cell_to_utf8(&term, cell, tmp_cell_str);
                    int scr_r = offset_y + r;
                    int scr_c = offset_x + c;
                    int jump_len = snprintf(&render_buf[r_pos], sizeof(render_buf) - r_pos - 32, "\033[%d;%dH%s", scr_r, scr_c, glyph_str);
                    if (jump_len > 0) r_pos += jump_len;

                    if (r_pos > sizeof(render_buf) - 512) {
                        write(STDOUT_FILENO, render_buf, r_pos);
                        r_pos = 0;
                    }
                }
            }
        }

        if (drew_any && r_pos > 0) {
            write(STDOUT_FILENO, render_buf, r_pos);
        }

        now = get_time_sec();
        if (g_status_dirty || (g_feedback_until > 0.0 && now >= g_feedback_until)) {
            if (g_feedback_until > 0.0 && now >= g_feedback_until) {
                g_feedback_until = 0.0;
            }
            g_status_dirty = 0;
            render_status_bar(&term, target_host, target_port, term_w, status_y);
        }

#ifndef _WIN32
        struct pollfd pfd;
        pfd.fd = STDIN_FILENO;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int poll_res = poll(&pfd, 1, 16);
        bool has_input = (poll_res > 0 && (pfd.revents & POLLIN));
#else
        DWORD wait_res = WaitForSingleObject(g_hIn, 16);
        bool has_input = (wait_res == WAIT_OBJECT_0 && win_console_has_input(g_hIn));
#endif
        if (has_input) {
            uint8_t seq[32];
            size_t seq_len = console_read_sequence(STDIN_FILENO, seq, sizeof(seq));
            if (seq_len > 0) {
                plato_key_event_t ev = {0};
                double now_key = get_time_sec();

                if (seq[0] == 0x1B) {
                    if (seq_len == 1) {
                        ev.vkey = PLATO_VKEY_ESCAPE;
                        plato_keyboard_dispatch(&kbd_state, &term, &ev, now_key);
                    } else if (seq_len == 2) {
                        if (seq[1] == 's') {
                            ev.ctrl = true; ev.unmod_codepoint = 's';
                            plato_keyboard_dispatch(&kbd_state, &term, &ev, now_key);
                        } else if (seq[1] == 'S') {
                            ev.ctrl = true; ev.shift = true; ev.unmod_codepoint = 's';
                            plato_keyboard_dispatch(&kbd_state, &term, &ev, now_key);
                        } else if (seq[1] == 'b') {
                            ev.ctrl = true; ev.unmod_codepoint = 'b';
                            plato_keyboard_dispatch(&kbd_state, &term, &ev, now_key);
                        } else if (seq[1] == 'B') {
                            ev.ctrl = true; ev.shift = true; ev.unmod_codepoint = 'b';
                            plato_keyboard_dispatch(&kbd_state, &term, &ev, now_key);
                        } else if (seq[1] == 'n' || seq[1] == 'N' || seq[1] == 10 || seq[1] == 13) {
                            ev.vkey = PLATO_VKEY_RETURN; ev.shift = true;
                            plato_keyboard_dispatch(&kbd_state, &term, &ev, now_key);
                        } else if (seq[1] == 'c' || seq[1] == 'C') {
                            plato_keyboard_flush(&kbd_state, &term);
                            copy_screen_to_clipboard(&term);
                        }
                    } else if (seq[1] == 'O') {
                        switch (seq[2]) {
                            case 'P': ev.vkey = PLATO_VKEY_F1; break;
                            case 'Q': ev.vkey = PLATO_VKEY_F2; break;
                            case 'R': ev.vkey = PLATO_VKEY_F3; break;
                            case 'S': ev.vkey = PLATO_VKEY_F4; break;
                        }
                        if (ev.vkey) plato_keyboard_dispatch(&kbd_state, &term, &ev, now_key);
                    } else if (seq[1] == '[') {
                        if (seq_len >= 3 && seq[2] == 'A') {
                            ev.vkey = PLATO_VKEY_UP;
                            plato_keyboard_dispatch(&kbd_state, &term, &ev, now_key);
                        } else if (seq_len >= 3 && seq[2] == 'B') {
                            ev.vkey = PLATO_VKEY_DOWN;
                            plato_keyboard_dispatch(&kbd_state, &term, &ev, now_key);
                        } else if (seq_len >= 3 && seq[2] == 'C') {
                            ev.vkey = PLATO_VKEY_RIGHT;
                            plato_keyboard_dispatch(&kbd_state, &term, &ev, now_key);
                        } else if (seq_len >= 3 && seq[2] == 'D') {
                            ev.vkey = PLATO_VKEY_LEFT;
                            plato_keyboard_dispatch(&kbd_state, &term, &ev, now_key);
                        } else if (seq_len >= 6 && seq[2] == '1' && seq[3] == ';' && seq[4] == '2') {
                            ev.shift = true;
                            switch (seq[5]) {
                                case 'P': ev.vkey = PLATO_VKEY_F1; break;
                                case 'Q': ev.vkey = PLATO_VKEY_F2; break;
                                case 'R': ev.vkey = PLATO_VKEY_F3; break;
                                case 'S': ev.vkey = PLATO_VKEY_F4; break;
                            }
                            if (ev.vkey) plato_keyboard_dispatch(&kbd_state, &term, &ev, now_key);
                        } else {
                            int fnum = 0, fmod = 1;
                            size_t p = 2;
                            while (p < seq_len && seq[p] >= '0' && seq[p] <= '9') {
                                fnum = fnum * 10 + (seq[p] - '0');
                                p++;
                            }
                            if (p < seq_len && seq[p] == ';') {
                                p++; fmod = 0;
                                while (p < seq_len && seq[p] >= '0' && seq[p] <= '9') {
                                    fmod = fmod * 10 + (seq[p] - '0');
                                    p++;
                                }
                            }
                            if (p < seq_len && seq[p] == '~') {
                                ev.shift = (fmod == 2);
                                switch (fnum) {
                                    case 15: ev.vkey = PLATO_VKEY_F5; break;
                                    case 17: ev.vkey = PLATO_VKEY_F6; break;
                                    case 18: ev.vkey = PLATO_VKEY_F7; break;
                                    case 19: ev.vkey = PLATO_VKEY_F8; break;
                                    case 20: ev.vkey = PLATO_VKEY_F9; break;
                                    case 21: ev.vkey = PLATO_VKEY_F10; break;
                                }
                                if (ev.vkey) plato_keyboard_dispatch(&kbd_state, &term, &ev, now_key);
                            }
                        }
                    }
                } else {
                    uint8_t ch = seq[0];
                    if (ch == 0x03) {
                        g_quit = 1;
                        break;
                    } else if (ch == 0x19) {
                        plato_keyboard_flush(&kbd_state, &term);
                        copy_screen_to_clipboard(&term);
                    } else if (ch == 0x12) {
                        g_resized = 1;
                    } else if (ch == 0x0F) {
                        ev.ctrl = true; ev.shift = true; ev.unmod_codepoint = 's';
                        plato_keyboard_dispatch(&kbd_state, &term, &ev, now_key);
                    } else if (ch == 10 || ch == 13) {
                        ev.vkey = PLATO_VKEY_RETURN;
                        plato_keyboard_dispatch(&kbd_state, &term, &ev, now_key);
                    } else if (ch == 127 || ch == 8) {
                        ev.vkey = PLATO_VKEY_BACKSPACE;
                        plato_keyboard_dispatch(&kbd_state, &term, &ev, now_key);
                    } else if (ch == 9) {
                        ev.vkey = PLATO_VKEY_TAB;
                        plato_keyboard_dispatch(&kbd_state, &term, &ev, now_key);
                    } else if (ch >= 1 && ch <= 26) {
                        ev.ctrl = true;
                        ev.unmod_codepoint = 'a' + (ch - 1);
                        plato_keyboard_dispatch(&kbd_state, &term, &ev, now_key);
                    } else if (ch >= 32 && ch <= 126) {
                        ev.codepoint = ch;
                        plato_keyboard_dispatch(&kbd_state, &term, &ev, now_key);
                    }
                }
            }
        }
    }

    console_restore_terminal();

    net_ctx.running = false;
    plato_transport_disconnect(transport);
#ifndef _WIN32
    pthread_join(net_thread, NULL);
#else
    if (net_thread) {
        WaitForSingleObject(net_thread, INFINITE);
        CloseHandle(net_thread);
    }
#endif
    plato_transport_destroy(transport);
    plato_ringbuf_destroy(&ringbuf);

    return 0;
}
