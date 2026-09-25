#include "plato/plato_profile.h"
#include "plato/plato_script.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #include <windows.h>
  #include <shlobj.h>
#else
  #include <unistd.h>
  #include <sys/stat.h>
  #include <sys/types.h>
#endif

void plato_profile_get_storage_path(char *out_path, size_t max_len) {
#ifdef _WIN32
    char appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata))) {
        char dir[MAX_PATH];
        snprintf(dir, sizeof(dir), "%s\\PlatoLives", appdata);
        CreateDirectoryA(dir, NULL);
        snprintf(out_path, max_len, "%s\\profiles.ini", dir);
        return;
    }
    snprintf(out_path, max_len, "profiles.ini");
#else
    const char *home = getenv("HOME");
    if (home) {
        char dir[512];
        snprintf(dir, sizeof(dir), "%s/.platolives", home);
        mkdir(dir, 0755);
        snprintf(out_path, max_len, "%s/profiles.ini", dir);
        return;
    }
    snprintf(out_path, max_len, "profiles.ini");
#endif
}

static void init_default_cyber1(plato_profile_list_t *list) {
    memset(list, 0, sizeof(*list));
    list->count = 1;
    list->default_index = 0;

    plato_profile_t *p = &list->profiles[0];
    snprintf(p->id, sizeof(p->id), "cyber1-default");
    snprintf(p->name, sizeof(p->name), "Cyber1");
    snprintf(p->host, sizeof(p->host), "cyberserv.org");
    p->port = 8005;
    p->display_mode = 0;         /* Real Plasma default */
    p->persistence_ms = 100;     /* 100 ms Authentic Plasma */
    p->plasma_distortion = 2;    /* Cylindrical default */
    p->crt_beam_level = 1;       /* High (Soft Glow) */
    p->crt_persistence_ms = 20;  /* 20 ms Default CRT */
    p->crt_distortion = 2;       /* Cylindrical */
    p->full_screen = false;
    p->is_default = true;
    p->startup_script_enabled = false;
    snprintf(p->startup_script, sizeof(p->startup_script), "%s", PLATO_DEFAULT_AUTOLOGIN_SCRIPT);
}

bool plato_profiles_load(plato_profile_list_t *list) {
    if (!list) return false;
    char path[512];
    plato_profile_get_storage_path(path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) {
        init_default_cyber1(list);
        plato_profiles_save(list);
        return true;
    }

    memset(list, 0, sizeof(*list));
    char line[4096];
    plato_profile_t *cur = NULL;

    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        size_t len = strlen(p);
        while (len > 0 && (p[len - 1] == '\r' || p[len - 1] == '\n' || p[len - 1] == ' ')) {
            p[--len] = '\0';
        }
        if (len == 0 || *p == '#' || *p == ';') continue;

        if (*p == '[' && p[len - 1] == ']') {
            p[len - 1] = '\0';
            p++;
            if (strncmp(p, "Profile:", 8) == 0 && list->count < PLATO_MAX_PROFILES) {
                cur = &list->profiles[list->count++];
                memset(cur, 0, sizeof(*cur));
                snprintf(cur->name, sizeof(cur->name), "%s", p + 8);
                snprintf(cur->id, sizeof(cur->id), "%s", p + 8);
                cur->port = 8005;
                cur->display_mode = 0;
                cur->persistence_ms = 100;
                cur->plasma_distortion = 2;
                cur->crt_beam_level = 1;
                cur->crt_persistence_ms = 20;
                cur->crt_distortion = 2;
            } else {
                cur = NULL;
            }
            continue;
        }

        if (!cur) continue;

        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *val = eq + 1;
        while (*val == ' ') val++;

        if (strcmp(p, "id") == 0) snprintf(cur->id, sizeof(cur->id), "%s", val);
        else if (strcmp(p, "host") == 0) snprintf(cur->host, sizeof(cur->host), "%s", val);
        else if (strcmp(p, "port") == 0) cur->port = atoi(val);
        else if (strcmp(p, "display_mode") == 0) cur->display_mode = atoi(val);
        else if (strcmp(p, "persistence_ms") == 0) cur->persistence_ms = atoi(val);
        else if (strcmp(p, "plasma_distortion") == 0) cur->plasma_distortion = atoi(val);
        else if (strcmp(p, "crt_beam_level") == 0) cur->crt_beam_level = atoi(val);
        else if (strcmp(p, "crt_persistence_ms") == 0) cur->crt_persistence_ms = atoi(val);
        else if (strcmp(p, "crt_distortion") == 0) cur->crt_distortion = atoi(val);
        else if (strcmp(p, "full_screen") == 0) cur->full_screen = (atoi(val) != 0);
        else if (strcmp(p, "is_default") == 0) {
            cur->is_default = (atoi(val) != 0);
            if (cur->is_default) list->default_index = (int)(list->count - 1);
        }
        else if (strcmp(p, "startup_script_enabled") == 0) cur->startup_script_enabled = (atoi(val) != 0);
        else if (strcmp(p, "startup_script") == 0) {
            char *d = cur->startup_script;
            size_t d_max = sizeof(cur->startup_script) - 1;
            size_t d_idx = 0;
            for (size_t s = 0; val[s] && d_idx < d_max; s++) {
                if (val[s] == '\\' && val[s + 1] == 'n') {
                    d[d_idx++] = '\n';
                    s++;
                } else {
                    d[d_idx++] = val[s];
                }
            }
            d[d_idx] = '\0';
        }
    }
    fclose(f);

    if (list->count == 0) {
        init_default_cyber1(list);
        plato_profiles_save(list);
    }
    return true;
}

bool plato_profiles_save(const plato_profile_list_t *list) {
    if (!list) return false;
    char path[512];
    plato_profile_get_storage_path(path, sizeof(path));

    FILE *f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "# PlatoLives Connection Profiles Configuration\n\n");
    for (size_t i = 0; i < list->count; i++) {
        const plato_profile_t *p = &list->profiles[i];
        fprintf(f, "[Profile:%s]\n", p->name);
        fprintf(f, "id=%s\n", p->id);
        fprintf(f, "host=%s\n", p->host);
        fprintf(f, "port=%d\n", p->port);
        fprintf(f, "display_mode=%d\n", p->display_mode);
        fprintf(f, "persistence_ms=%d\n", p->persistence_ms);
        fprintf(f, "plasma_distortion=%d\n", p->plasma_distortion);
        fprintf(f, "crt_beam_level=%d\n", p->crt_beam_level);
        fprintf(f, "crt_persistence_ms=%d\n", p->crt_persistence_ms);
        fprintf(f, "crt_distortion=%d\n", p->crt_distortion);
        fprintf(f, "full_screen=%d\n", p->full_screen ? 1 : 0);
        fprintf(f, "is_default=%d\n", p->is_default ? 1 : 0);
        fprintf(f, "startup_script_enabled=%d\n", p->startup_script_enabled ? 1 : 0);

        fprintf(f, "startup_script=");
        for (size_t s = 0; p->startup_script[s]; s++) {
            if (p->startup_script[s] == '\n') fputs("\\n", f);
            else if (p->startup_script[s] == '\r') continue;
            else fputc(p->startup_script[s], f);
        }
        fprintf(f, "\n\n");
    }
    fclose(f);
    return true;
}

plato_profile_t* plato_profiles_get_default(plato_profile_list_t *list) {
    if (!list || list->count == 0) return NULL;
    if (list->default_index >= 0 && (size_t)list->default_index < list->count) {
        return &list->profiles[list->default_index];
    }
    return &list->profiles[0];
}

bool plato_profiles_set_default(plato_profile_list_t *list, size_t index) {
    if (!list || index >= list->count) return false;
    for (size_t i = 0; i < list->count; i++) {
        list->profiles[i].is_default = (i == index);
    }
    list->default_index = (int)index;
    return plato_profiles_save(list);
}

bool plato_profiles_add(plato_profile_list_t *list, const plato_profile_t *prof) {
    if (!list || !prof || list->count >= PLATO_MAX_PROFILES) return false;
    list->profiles[list->count++] = *prof;
    return plato_profiles_save(list);
}

bool plato_profiles_delete(plato_profile_list_t *list, size_t index) {
    if (!list || index >= list->count || list->count <= 1) return false;
    bool was_default = list->profiles[index].is_default;
    for (size_t i = index; i < list->count - 1; i++) {
        list->profiles[i] = list->profiles[i + 1];
    }
    list->count--;

    /* Se abbiamo eliminato il profilo di default o l'indice è fuori scala, elegge il primo profilo */
    if (was_default || list->default_index == (int)index || list->default_index >= (int)list->count) {
        list->default_index = 0;
        for (size_t i = 0; i < list->count; i++) {
            list->profiles[i].is_default = (i == 0);
        }
    } else if (list->default_index > (int)index) {
        list->default_index--;
    }
    return plato_profiles_save(list);
}
