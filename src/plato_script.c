#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "plato/plato_script.h"
#include "plato/plato_keyboard.h"
#include "plato/plato_protocol.h"
#include "plato/plato_transport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#ifdef _WIN32
  #include <windows.h>
  static inline void plato_sleep_ms(int ms) {
      Sleep(ms);
  }
#else
  #include <unistd.h>
  static inline void plato_sleep_ms(int ms) {
      struct timespec ts;
      ts.tv_sec = ms / 1000;
      ts.tv_nsec = (long)(ms % 1000) * 1000000L;
      nanosleep(&ts, NULL);
  }
#endif

struct plato_script_runner {
    plato_terminal_t *term;
    char *script_text;
    pthread_t thread;
    volatile bool running;
    volatile bool active;
};

static uint16_t parse_key_name(const char *name) {
    char clean[64];
    size_t len = 0;
    for (size_t i = 0; name[i] && len < sizeof(clean) - 1; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        if (c == '_') c = '-';
        clean[len++] = c;
    }
    clean[len] = '\0';

    bool shifted = (strncmp(clean, "SHIFT-", 6) == 0);
    const char *k = shifted ? (clean + 6) : clean;

    plato_special_key_t key = PLATO_KEY_NONE;
    if (strcmp(k, "NEXT") == 0) key = PLATO_KEY_NEXT;
    else if (strcmp(k, "BACK") == 0) key = PLATO_KEY_BACK;
    else if (strcmp(k, "STOP") == 0) key = PLATO_KEY_STOP;
    else if (strcmp(k, "HELP") == 0) key = PLATO_KEY_HELP;
    else if (strcmp(k, "LAB") == 0) key = PLATO_KEY_LAB;
    else if (strcmp(k, "DATA") == 0) key = PLATO_KEY_DATA;
    else if (strcmp(k, "EDIT") == 0) key = PLATO_KEY_EDIT;
    else if (strcmp(k, "ERASE") == 0) key = PLATO_KEY_ERASE;
    else if (strcmp(k, "ANS") == 0) key = PLATO_KEY_ANS;
    else if (strcmp(k, "SUPER") == 0) key = PLATO_KEY_SUPER;
    else if (strcmp(k, "SUB") == 0) key = PLATO_KEY_SUB;
    else if (strcmp(k, "TERM") == 0) key = PLATO_KEY_TERM;
    else if (strcmp(k, "MICRO") == 0) key = PLATO_KEY_MICRO;
    else if (strcmp(k, "FONT") == 0) key = PLATO_KEY_FONT;
    else if (strcmp(k, "SQUARE") == 0) key = PLATO_KEY_SQUARE;

    return plato_keyboard_keycode(key, shifted);
}

static void* script_worker(void *arg) {
    plato_script_runner_t *r = (plato_script_runner_t *)arg;
    if (!r || !r->script_text) return NULL;

    char *copy = strdup(r->script_text);
    if (!copy) return NULL;

    char *line = copy;
    while (r->running && line && *line) {
        char *next = strchr(line, '\n');
        if (next) {
            *next = '\0';
            next++;
        }

        /* Trim */
        while (*line == ' ' || *line == '\t') line++;
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' ')) line[--len] = '\0';

        if (len > 0 && *line != '#') {
            char op[32] = {0};
            char arg_val[256] = {0};
            if (sscanf(line, "%31s %255[^\n]", op, arg_val) >= 1) {
                if (strcmp(op, "wait") == 0 || strcmp(op, "attendi") == 0) {
                    double sec = 1.0;
                    if (strstr(arg_val, "ms")) {
                        sec = atof(arg_val) * 0.001;
                    } else {
                        sec = atof(arg_val);
                    }
                    if (sec < 0.05) sec = 0.05;
                    int ms = (int)(sec * 1000.0);
                    while (r->running && ms > 0) {
                        int step = (ms > 50) ? 50 : ms;
                        plato_sleep_ms(step);
                        ms -= step;
                    }
                } else if (strcmp(op, "send") == 0 || strcmp(op, "invia") == 0) {
                    if (r->term && r->term->transport && r->term->transport->connected) {
                        for (size_t i = 0; arg_val[i] && r->running; i++) {
                            uint8_t ch = (uint8_t)arg_val[i];
                            if (ch == '\n') {
                                plato_protocol_send_key(r->term, plato_keyboard_keycode(PLATO_KEY_NEXT, false));
                                plato_sleep_ms(500); /* Pacing Mac: 500 ms dopo NEXT */
                            } else if (ch >= 32 && ch <= 126) {
                                plato_transport_send(r->term->transport, &ch, 1);
                                plato_sleep_ms(250); /* Pacing Mac: 250 ms per carattere */
                            }
                        }
                    }
                } else if (strcmp(op, "key") == 0 || strcmp(op, "tasto") == 0) {
                    if (r->term) {
                        uint16_t code = parse_key_name(arg_val);
                        if (code != UINT16_MAX) {
                            plato_protocol_send_key(r->term, code);
                            plato_sleep_ms(500); /* Pacing Mac: 500 ms dopo tasto PLATO */
                        }
                    }
                }
            }
        }
        line = next;
    }

    free(copy);
    r->active = false;
    return NULL;
}

plato_script_runner_t* plato_script_create(plato_terminal_t *term) {
    plato_script_runner_t *r = (plato_script_runner_t *)calloc(1, sizeof(plato_script_runner_t));
    if (r) r->term = term;
    return r;
}

void plato_script_destroy(plato_script_runner_t *runner) {
    if (!runner) return;
    plato_script_stop(runner);
    if (runner->script_text) free(runner->script_text);
    free(runner);
}

bool plato_script_start(plato_script_runner_t *runner, const char *script_text) {
    if (!runner || !script_text || strlen(script_text) == 0) return false;
    plato_script_stop(runner);

    if (runner->script_text) free(runner->script_text);
    runner->script_text = strdup(script_text);
    runner->running = true;
    runner->active = true;

    if (pthread_create(&runner->thread, NULL, script_worker, runner) != 0) {
        runner->running = false;
        runner->active = false;
        return false;
    }
    return true;
}

void plato_script_stop(plato_script_runner_t *runner) {
    if (!runner) return;
    if (runner->running) {
        runner->running = false;
        pthread_join(runner->thread, NULL);
    }
    runner->active = false;
}

bool plato_script_is_running(const plato_script_runner_t *runner) {
    return runner && runner->active;
}
