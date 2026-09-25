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
#include <strings.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
static inline void plato_sleep_ms(int ms) { if (ms > 0) Sleep(ms); }
#else
#include <unistd.h>
#include <pwd.h>
static inline void plato_sleep_ms(int ms) {
    if (ms <= 0) return;
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}
#endif

plato_script_list_t *plato_scripts_create(void) {
    plato_script_list_t *list = (plato_script_list_t *)calloc(1, sizeof(plato_script_list_t));
    if (!list) return NULL;
    list->capacity = 8;
    list->scripts = (plato_script_t *)calloc(list->capacity, sizeof(plato_script_t));
    if (!list->scripts) { free(list); return NULL; }
    return list;
}

void plato_scripts_free(plato_script_list_t *list) {
    if (!list) return;
    if (list->scripts) free(list->scripts);
    free(list);
}

const char *plato_scripts_get_default_path(void) {
    static char s_path[1024] = {0};
    if (s_path[0] != 0) return s_path;
#if defined(_WIN32)
    char appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata))) {
        char dir[MAX_PATH];
        snprintf(dir, sizeof(dir), "%s\\PlatoLives", appdata);
        CreateDirectoryA(dir, NULL);
        snprintf(s_path, sizeof(s_path), "%s\\scripts.ini", dir);
    } else { snprintf(s_path, sizeof(s_path), "platolives_scripts.ini"); }
#else
    const char *home = getenv("HOME");
    if (!home) { struct passwd *pw = getpwuid(getuid()); if (pw) home = pw->pw_dir; }
    if (home) {
        char dir[1024];
        snprintf(dir, sizeof(dir), "%s/.platolives", home);
        mkdir(dir, 0755);
        snprintf(s_path, sizeof(s_path), "%s/.platolives/scripts.ini", home);
    } else { snprintf(s_path, sizeof(s_path), "platolives_scripts.ini"); }
#endif
    return s_path;
}

static void escape_str(const char *in, char *out, size_t sz) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 2 < sz; i++) {
        if (in[i] == 10) { out[o++] = 92; out[o++] = 110; }
        else if (in[i] == 13) { out[o++] = 92; out[o++] = 114; }
        else if (in[i] == 92) { out[o++] = 92; out[o++] = 92; }
        else { out[o++] = in[i]; }
    }
    out[o] = 0;
}

static void unescape_str(const char *in, char *out, size_t sz) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 1 < sz; i++) {
        if (in[i] == 92 && in[i+1] != 0) {
            i++;
            if (in[i] == 110) out[o++] = 10;
            else if (in[i] == 114) out[o++] = 13;
            else if (in[i] == 92) out[o++] = 92;
            else out[o++] = in[i];
        } else { out[o++] = in[i]; }
    }
    out[o] = 0;
}

bool plato_scripts_save(const plato_script_list_t *list, const char *filepath) {
    if (!list) return false;
    const char *path = filepath ? filepath : plato_scripts_get_default_path();
    FILE *f = fopen(path, "w");
    if (!f) return false;
    fprintf(f, "# PlatoLives Scripts Configuration\n[general]\ncount=%zu\n\n", list->count);
    for (size_t i = 0; i < list->count; i++) {
        const plato_script_t *s = &list->scripts[i];
        char esc_b[PLATO_SCRIPT_MAX_BODY * 2];
        escape_str(s->body, esc_b, sizeof(esc_b));
        fprintf(f, "[script_%zu]\nid=%s\nname=%s\nenabled=%d\nhotkey_modifiers=%u\nhotkey_key=%u\nhotkey_display=%s\nchar_delay_ms=%d\nnext_delay_ms=%d\ncommand_delay_ms=%d\nbody=%s\n\n",
                i, s->id[0] ? s->id : "script", s->name, s->enabled ? 1 : 0, s->hotkey_modifiers, s->hotkey_key, s->hotkey_display, s->char_delay_ms, s->next_delay_ms, s->command_delay_ms, esc_b);
    }
    fclose(f);
    return true;
}

bool plato_scripts_load(plato_script_list_t *list, const char *filepath) {
    if (!list) return false;
    const char *path = filepath ? filepath : plato_scripts_get_default_path();
    FILE *f = fopen(path, "r");
    if (!f) return false;
    list->count = 0;
    char line[PLATO_SCRIPT_MAX_BODY * 2];
    plato_script_t cur = {0};
    bool in_s = false;
    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, 13); if (nl) *nl = 0;
        nl = strchr(line, 10); if (nl) *nl = 0;
        if (line[0] == 0 || line[0] == 35) continue;
        if (line[0] == 91 && strncmp(line, "[script_", 8) == 0) {
            if (in_s) plato_scripts_add(list, &cur);
            memset(&cur, 0, sizeof(cur));
            cur.char_delay_ms = 250; cur.next_delay_ms = 500; cur.command_delay_ms = 0; cur.enabled = true;
            in_s = true; continue;
        }
        if (!in_s) continue;
        char *eq = strchr(line, 61); if (!eq) continue;
        *eq = 0; char *k = line; char *v = eq + 1;
        if (strcmp(k, "id") == 0) snprintf(cur.id, sizeof(cur.id), "%s", v);
        else if (strcmp(k, "name") == 0) snprintf(cur.name, sizeof(cur.name), "%s", v);
        else if (strcmp(k, "enabled") == 0) cur.enabled = (atoi(v) != 0);
        else if (strcmp(k, "hotkey_modifiers") == 0) cur.hotkey_modifiers = (uint32_t)strtoul(v, NULL, 10);
        else if (strcmp(k, "hotkey_key") == 0) cur.hotkey_key = (uint32_t)strtoul(v, NULL, 10);
        else if (strcmp(k, "hotkey_display") == 0) snprintf(cur.hotkey_display, sizeof(cur.hotkey_display), "%s", v);
        else if (strcmp(k, "char_delay_ms") == 0) cur.char_delay_ms = atoi(v);
        else if (strcmp(k, "next_delay_ms") == 0) cur.next_delay_ms = atoi(v);
        else if (strcmp(k, "command_delay_ms") == 0) cur.command_delay_ms = atoi(v);
        else if (strcmp(k, "body") == 0) unescape_str(v, cur.body, sizeof(cur.body));
    }
    if (in_s) plato_scripts_add(list, &cur);
    fclose(f);
    return true;
}

int plato_scripts_add(plato_script_list_t *list, const plato_script_t *s) {
    if (!list || !s) return -1;
    if (list->count >= list->capacity) {
        size_t nc = list->capacity * 2;
        plato_script_t *na = (plato_script_t *)realloc(list->scripts, nc * sizeof(plato_script_t));
        if (!na) return -1;
        list->scripts = na; list->capacity = nc;
    }
    list->scripts[list->count] = *s;
    if (list->scripts[list->count].char_delay_ms <= 0) list->scripts[list->count].char_delay_ms = 250;
    if (list->scripts[list->count].next_delay_ms <= 0) list->scripts[list->count].next_delay_ms = 500;
    if (list->scripts[list->count].id[0] == 0) {
        snprintf(list->scripts[list->count].id, sizeof(list->scripts[list->count].id), "script_%zu", list->count + 1);
    }
    int idx = (int)list->count;
    list->count++;
    return idx;
}

int plato_scripts_clone(plato_script_list_t *list, size_t index) {
    if (!list || index >= list->count) return -1;
    plato_script_t cp = list->scripts[index];
    char old[PLATO_SCRIPT_MAX_NAME]; snprintf(old, sizeof(old), "%s", cp.name);
    snprintf(cp.name, sizeof(cp.name), "%s (Copy)", old);
    snprintf(cp.id, sizeof(cp.id), "script_%zu_%u", list->count + 1, (unsigned)(rand() % 10000));
    cp.hotkey_modifiers = 0; cp.hotkey_key = 0; cp.hotkey_display[0] = 0;
    return plato_scripts_add(list, &cp);
}

bool plato_scripts_delete(plato_script_list_t *list, size_t index) {
    if (!list || index >= list->count) return false;
    for (size_t i = index; i + 1 < list->count; i++) list->scripts[i] = list->scripts[i+1];
    list->count--;
    return true;
}

typedef struct { char name[PLATO_SCRIPT_VAR_NAME_LEN]; int64_t val; char str_val[128]; bool is_str; } script_var_t;
typedef struct { char name[PLATO_SCRIPT_VAR_NAME_LEN]; int line_index; } script_label_t;
typedef struct { char var_name[PLATO_SCRIPT_VAR_NAME_LEN]; int64_t end_val; int64_t step_val; int start_line; } script_for_frame_t;

struct plato_script_runner {
    plato_terminal_t *term;
    char *script_text;
    pthread_t thread;
    volatile bool running;
    volatile bool active;
    int char_delay_ms;
    int next_delay_ms;
    int command_delay_ms;
    script_var_t vars[PLATO_SCRIPT_MAX_VARS];
    int var_count;
};

static uint16_t parse_key_name(const char *name) {
    char clean[64]; size_t len = 0;
    for (size_t i = 0; name[i] && len < sizeof(clean) - 1; i++) {
        char c = name[i];
        if (c >= 97 && c <= 122) c = c - 97 + 65;
        if (c == 95) c = 45;
        clean[len++] = c;
    }
    clean[len] = 0;
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


/* Micro-Regex Engine C11 Puro (Zero-Dependency, Cross-Platform) */
static int match_star(int c, const char *re, const char *text);
static int match_here(const char *re, const char *text) {
    if (re[0] == 0) return 1;
    if (re[1] == 42) return match_star(re[0], re + 2, text); /* c* */
    if (re[1] == 43) { /* c+ */
        if (*text && (re[0] == 46 || tolower((unsigned char)*text) == tolower((unsigned char)re[0])))
            return match_star(re[0], re + 2, text + 1);
        return 0;
    }
    if (re[0] == 36 && re[1] == 0) return *text == 0; /* $ */
    if (*text && (re[0] == 46 || tolower((unsigned char)*text) == tolower((unsigned char)re[0])))
        return match_here(re + 1, text + 1);
    return 0;
}
static int match_star(int c, const char *re, const char *text) {
    do {
        if (match_here(re, text)) return 1;
    } while (*text && (tolower((unsigned char)*text++) == tolower((unsigned char)c) || c == 46));
    return 0;
}
static int plato_regex_match(const char *re, const char *text) {
    if (re[0] == 94) return match_here(re + 1, text); /* ^ */
    do {
        if (match_here(re, text)) return 1;
    } while (*text++);
    return 0;
}

static void trim_s(char *s) {
    while (*s == 32 || *s == 9) memmove(s, s + 1, strlen(s));
    size_t l = strlen(s);
    while (l > 0 && (s[l-1] == 32 || s[l-1] == 9 || s[l-1] == 13 || s[l-1] == 10)) s[--l] = 0;
}

static void strip_q(char *s) {
    size_t l = strlen(s);
    if (l >= 2 && ((s[0] == 34 && s[l-1] == 34) || (s[0] == 39 && s[l-1] == 39))) {
        s[l-1] = 0; memmove(s, s + 1, l - 1);
    }
}


static const char* plato_script_get_str_var(plato_script_runner_t *r, const char *name) {
    if (!r) return NULL;
    for (int i = 0; i < r->var_count; i++) {
        if (strcmp(r->vars[i].name, name) == 0 && r->vars[i].is_str) {
            return r->vars[i].str_val;
        }
    }
    return NULL;
}

static bool plato_script_set_str_var(plato_script_runner_t *r, const char *name, const char *str) {
    if (!r) return false;
    for (int i = 0; i < r->var_count; i++) {
        if (strcmp(r->vars[i].name, name) == 0) {
            snprintf(r->vars[i].str_val, sizeof(r->vars[i].str_val), "%s", str);
            r->vars[i].is_str = true;
            return true;
        }
    }
    if (r->var_count < PLATO_SCRIPT_MAX_VARS) {
        snprintf(r->vars[r->var_count].name, PLATO_SCRIPT_VAR_NAME_LEN, "%s", name);
        snprintf(r->vars[r->var_count].str_val, sizeof(r->vars[r->var_count].str_val), "%s", str);
        r->vars[r->var_count].is_str = true;
        r->var_count++;
        return true;
    }
    return false;
}

int64_t plato_script_get_var(plato_script_runner_t *r, const char *name, bool *found) {
    if (!r) { if (found) *found = false; return 0; }
    for (int i = 0; i < r->var_count; i++) {
        if (strcmp(r->vars[i].name, name) == 0) {
            if (found) *found = true; return r->vars[i].val;
        }
    }
    if (found) *found = false; return 0;
}

bool plato_script_set_var(plato_script_runner_t *r, const char *name, int64_t val) {
    if (!r) return false;
    for (int i = 0; i < r->var_count; i++) {
        if (strcmp(r->vars[i].name, name) == 0) { r->vars[i].val = val; return true; }
    }
    if (r->var_count < PLATO_SCRIPT_MAX_VARS) {
        snprintf(r->vars[r->var_count].name, PLATO_SCRIPT_VAR_NAME_LEN, "%s", name);
        r->vars[r->var_count].val = val; r->var_count++; return true;
    }
    return false;
}

static int64_t eval_tok(plato_script_runner_t *r, const char *tok) {
    if (!tok || tok[0] == 0) return 0;
    char *ep = NULL; long long v = strtoll(tok, &ep, 10);
    if (ep && *ep == 0) return (int64_t)v;
    bool found = false; int64_t val = plato_script_get_var(r, tok, &found);
    return found ? val : 0;
}

static void* script_worker(void *arg) {
    plato_script_runner_t *r = (plato_script_runner_t *)arg;
    if (!r || !r->script_text) return NULL;
    char *tc = strdup(r->script_text); if (!tc) return NULL;
    char *lines[1024]; int lc = 0; char *p = tc;
    while (*p && lc < 1024) {
        lines[lc++] = p;
        char *nl = strchr(p, 10); if (!nl) break;
        *nl = 0; p = nl + 1;
    }
    script_label_t labels[64]; int lcount = 0;
    for (int i = 0; i < lc; i++) {
        char buf[256]; snprintf(buf, sizeof(buf), "%s", lines[i]); trim_s(buf);
        size_t bl = strlen(buf);
        if (bl > 1 && buf[bl - 1] == 58) {
            buf[bl - 1] = 0; trim_s(buf); if (buf[0] == 64) memmove(buf, buf + 1, strlen(buf));
            if (lcount < 64) {
                snprintf(labels[lcount].name, sizeof(labels[0].name), "%s", buf);
                labels[lcount].line_index = i; lcount++;
            }
        }
    }
    script_for_frame_t fstack[16]; int ftop = 0; int cline = 0;
    while (r->running && cline < lc) {
        char lb[512]; snprintf(lb, sizeof(lb), "%s", lines[cline++]); trim_s(lb);
        if (lb[0] == 0 || lb[0] == 35 || (lb[0] == 47 && lb[1] == 47)) continue;
        if (lb[strlen(lb)-1] == 58) continue;
        if (strstr(lb, "++") || strstr(lb, "--")) {
            bool inc = (strstr(lb, "++") != NULL);
            char vn[32]; snprintf(vn, sizeof(vn), "%s", lb);
            char *op = strstr(vn, inc ? "++" : "--"); if (op) *op = 0; trim_s(vn);
            bool fd = false; int64_t v = plato_script_get_var(r, vn, &fd);
            plato_script_set_var(r, vn, inc ? (v + 1) : (v - 1));
            continue;
        }
        char op[32] = {0}; char av[512] = {0}; char *sp = strpbrk(lb, " \t");
        if (sp) {
            size_t ol = (size_t)(sp - lb); if (ol >= sizeof(op)) ol = sizeof(op) - 1;
            strncpy(op, lb, ol); op[ol] = 0; snprintf(av, sizeof(av), "%s", sp + 1); trim_s(av);
        } else { snprintf(op, sizeof(op), "%s", lb); }
        for (size_t k = 0; op[k]; k++) op[k] = (char)tolower((unsigned char)op[k]);

        if (strcmp(op, "let") == 0 || (strchr(lb, 61) && strncmp(op, "if", 2) != 0 && strncmp(op, "for", 3) != 0)) {
            char *al = (strcmp(op, "let") == 0) ? av : lb;
            char *eq = strchr(al, 61);
            if (eq) {
                *eq = 0; char *var = al; char *expr = eq + 1; trim_s(var); trim_s(expr);
                if ((expr[0] == 34 && expr[strlen(expr)-1] == 34) || (expr[0] == 39 && expr[strlen(expr)-1] == 39)) {
                    strip_q(expr);
                    plato_script_set_str_var(r, var, expr);
                } else {
                    plato_script_set_var(r, var, eval_tok(r, expr));
                }
            }
        } else if (strcmp(op, "goto") == 0) {
            if (av[0] == 64) memmove(av, av + 1, strlen(av));
            for (int k = 0; k < lcount; k++) {
                if (strcmp(labels[k].name, av) == 0) { cline = labels[k].line_index + 1; break; }
            }
        } else if (strcmp(op, "if") == 0) {
            char *gp = strstr(av, " goto ");
            if (gp) {
                *gp = 0; char *tgt = gp + 6; trim_s(tgt); if (tgt[0] == 64) tgt++;
                char v1[32] = {0}, cmp[8] = {0}, v2[32] = {0};
                if (sscanf(av, "%31s %7s %31s", v1, cmp, v2) == 3) {
                    int64_t val1 = eval_tok(r, v1), val2 = eval_tok(r, v2); bool mt = false;
                    if (strcmp(cmp, "==") == 0 || strcmp(cmp, "=") == 0) mt = (val1 == val2);
                    else if (strcmp(cmp, "!=") == 0 || strcmp(cmp, "<>") == 0) mt = (val1 != val2);
                    else if (strcmp(cmp, "<") == 0) mt = (val1 < val2);
                    else if (strcmp(cmp, "<=") == 0) mt = (val1 <= val2);
                    else if (strcmp(cmp, ">") == 0) mt = (val1 > val2);
                    else if (strcmp(cmp, ">=") == 0) mt = (val1 >= val2);
                    if (mt) {
                        for (int k = 0; k < lcount; k++) {
                            if (strcmp(labels[k].name, tgt) == 0) { cline = labels[k].line_index + 1; break; }
                        }
                    }
                }
            }
        } else if (strcmp(op, "for") == 0) {
            char vn[32] = {0}; char *eq = strchr(av, 61); char *to = strstr(av, " to ");
            if (eq && to) {
                *eq = 0; snprintf(vn, sizeof(vn), "%s", av); trim_s(vn); *to = 0;
                char *spart = eq + 1; char *epart = to + 4; trim_s(spart); trim_s(epart);
                int64_t sval = eval_tok(r, spart); int64_t eval_v = eval_tok(r, epart); int64_t stp = 1;
                char *step_p = strstr(epart, " step "); if (step_p) { *step_p = 0; stp = eval_tok(r, step_p + 6); }
                plato_script_set_var(r, vn, sval);
                if (ftop < 16) {
                    snprintf(fstack[ftop].var_name, 32, "%s", vn);
                    fstack[ftop].end_val = eval_v; fstack[ftop].step_val = stp;
                    fstack[ftop].start_line = cline; ftop++;
                }
            }
        } else if (strcmp(op, "next") == 0) {
            if (ftop > 0) {
                script_for_frame_t *f = &fstack[ftop - 1];
                bool fd = false; int64_t cur = plato_script_get_var(r, f->var_name, &fd);
                cur += f->step_val; plato_script_set_var(r, f->var_name, cur);
                bool cont = (f->step_val > 0) ? (cur <= f->end_val) : (cur >= f->end_val);
                if (cont) cline = f->start_line; else ftop--;
            }
        } else if (strcmp(op, "wait_text") == 0 || strcmp(op, "find_text") == 0) {
            bool is_re = (strstr(av, "regex ") != NULL);
            if (is_re) { char *rp = strstr(av, "regex ") + 6; memmove(av, rp, strlen(rp) + 1); }
            char vx[32] = {0}, vy[32] = {0};
            char *into_p = strstr(av, " into ");
            if (into_p) { *into_p = 0; sscanf(into_p + 6, "%31[^,],%31s", vx, vy); trim_s(vx); trim_s(vy); }
            int c1 = 0, r1 = 0, c2 = 63, r2 = 31;
            char *in_p = strstr(av, " in ");
            if (in_p) { *in_p = 0; sscanf(in_p + 4, "%d , %d to %d , %d", &c1, &r1, &c2, &r2); }
            /* find_text e' istantaneo a 0ms, wait_text attende di default 5000ms */
            int to_ms = (strcmp(op, "find_text") == 0) ? 0 : 5000;
            char *to_p = strstr(av, " timeout ");
            if (to_p) { *to_p = 0; double f = strstr(to_p + 9, "ms") ? 0.001 : 1.0; to_ms = (int)(atof(to_p + 9) * f * 1000.0); }
            trim_s(av); strip_q(av);
            int el = 0; bool fd = false;
            while (r->running) {
                char scr[4096] = {0};
                if (r->term) plato_terminal_get_text_area(r->term, c1, r1, c2, r2, false, scr, sizeof(scr));
                if (is_re) {
                    if (plato_regex_match(av, scr)) fd = true;
                } else if (strstr(scr, av) != NULL) fd = true;
                if (fd) {
                    if (vx[0]) plato_script_set_var(r, vx, c1);
                    if (vy[0]) plato_script_set_var(r, vy, r1);
                    break;
                }
                if (el >= to_ms) break;
                plato_sleep_ms(50); el += 50;
            }
            if (!fd) {
                if (vx[0]) plato_script_set_var(r, vx, -1);
                if (vy[0]) plato_script_set_var(r, vy, -1);
            }
        } else if (strcmp(op, "wait") == 0 || strcmp(op, "attendi") == 0) {
            double s = strstr(av, "ms") ? (atof(av) * 0.001) : atof(av);
            int ms = (int)(s * 1000.0);
            while (r->running && ms > 0) {
                int stp = (ms > 50) ? 50 : ms; plato_sleep_ms(stp); ms -= stp;
            }
        } else if (strcmp(op, "send") == 0 || strcmp(op, "invia") == 0) {
            const char *to_send = plato_script_get_str_var(r, av);
            if (!to_send) { strip_q(av); to_send = av; }
            if (r->term && r->term->transport && r->term->transport->connected) {
                for (size_t i = 0; to_send[i] && r->running; i++) {
                    uint8_t ch = (uint8_t)to_send[i];
                    if (ch == 10) {
                        plato_protocol_send_key(r->term, plato_keyboard_keycode(PLATO_KEY_NEXT, false));
                        plato_sleep_ms(r->next_delay_ms);
                    } else if (ch >= 32 && ch <= 126) {
                        plato_transport_send(r->term->transport, &ch, 1);
                        plato_sleep_ms(r->char_delay_ms);
                    }
                }
            }
            continue;
        } else if (strcmp(op, "key") == 0 || strcmp(op, "tasto") == 0) {
            strip_q(av);
            if (r->term) {
                uint16_t code = parse_key_name(av);
                if (code != UINT16_MAX) {
                    plato_protocol_send_key(r->term, code);
                    bool is_next = (strcasecmp(av, "NEXT") == 0);
                    plato_sleep_ms(is_next ? r->next_delay_ms : r->command_delay_ms);
                }
            }
        } else if (strcmp(op, "exit") == 0 || strcmp(op, "quit") == 0 || strcmp(op, "stop") == 0) {
            break;
        }
        if (r->command_delay_ms > 0) plato_sleep_ms(r->command_delay_ms);
    }
    free(tc); r->active = false;
    return NULL;
}

plato_script_runner_t* plato_script_create(plato_terminal_t *term) {
    plato_script_runner_t *r = (plato_script_runner_t *)calloc(1, sizeof(plato_script_runner_t));
    if (r) { r->term = term; r->char_delay_ms = 250; r->next_delay_ms = 500; r->command_delay_ms = 0; }
    return r;
}

void plato_script_destroy(plato_script_runner_t *runner) {
    if (!runner) return;
    plato_script_stop(runner);
    if (runner->script_text) free(runner->script_text);
    free(runner);
}

bool plato_script_start_ex(plato_script_runner_t *runner, const plato_script_t *script) {
    if (!runner || !script || strlen(script->body) == 0) return false;
    plato_script_stop(runner);
    runner->char_delay_ms = script->char_delay_ms > 0 ? script->char_delay_ms : 250;
    runner->next_delay_ms = script->next_delay_ms > 0 ? script->next_delay_ms : 500;
    runner->command_delay_ms = script->command_delay_ms >= 0 ? script->command_delay_ms : 0;
    runner->var_count = 0;
    if (runner->script_text) free(runner->script_text);
    runner->script_text = strdup(script->body);
    runner->running = true; runner->active = true;
    if (pthread_create(&runner->thread, NULL, script_worker, runner) != 0) {
        runner->running = false; runner->active = false; return false;
    }
    return true;
}

bool plato_script_start(plato_script_runner_t *runner, const char *script_text) {
    plato_script_t s = {0};
    s.char_delay_ms = 250; s.next_delay_ms = 500; s.command_delay_ms = 0;
    snprintf(s.body, sizeof(s.body), "%s", script_text ? script_text : "");
    return plato_script_start_ex(runner, &s);
}

void plato_script_stop(plato_script_runner_t *runner) {
    if (!runner) return;
    if (runner->running) {
        runner->running = false; pthread_join(runner->thread, NULL);
    }
    runner->active = false;
}

bool plato_script_is_running(const plato_script_runner_t *runner) {
    return runner && runner->active;
}
