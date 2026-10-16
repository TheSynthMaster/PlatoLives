#include "plato/plato_keymap.h"
#include "plato/plato_keyboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
  #include <windows.h>
  #include <shlobj.h>
#else
  #include <unistd.h>
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <dirent.h>
#endif

void plato_keymap_get_storage_dir(char *out_path, size_t max_len) {
#ifdef _WIN32
    char appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata))) {
        char base[MAX_PATH];
        snprintf(base, sizeof(base), "%s\\PlatoLives", appdata);
        CreateDirectoryA(base, NULL);
        snprintf(out_path, max_len, "%s\\keymaps", base);
        CreateDirectoryA(out_path, NULL);
        return;
    }
    snprintf(out_path, max_len, "keymaps");
#else
    const char *home = getenv("HOME");
    if (home) {
        char base[512];
        snprintf(base, sizeof(base), "%s/.platolives", home);
        mkdir(base, 0755);
        snprintf(out_path, max_len, "%s/keymaps", base);
        mkdir(out_path, 0755);
        return;
    }
    snprintf(out_path, max_len, "keymaps");
#endif
}

static void init_builtin_profiles(plato_keymap_manager_t *mgr) {
    mgr->profile_count = 0;
    mgr->active_profile_index = 0;

    /* 1. Profilo Built-in: Standard PLATO */
    {
        plato_keymap_profile_t *p = &mgr->profiles[mgr->profile_count++];
        memset(p, 0, sizeof(*p));
        snprintf(p->name, sizeof(p->name), "Standard PLATO");
        snprintf(p->description, sizeof(p->description), "Canonical pterm / TUTOR layout (standard arrow keys)");
        p->is_builtin = true;
        p->binding_count = 0; /* Nessun remapping; fallback canonico */
    }

    /* 2. Profilo Built-in: WASD Gaming Mode */
    {
        plato_keymap_profile_t *p = &mgr->profiles[mgr->profile_count++];
        memset(p, 0, sizeof(*p));
        snprintf(p->name, sizeof(p->name), "WASD Gaming");
        snprintf(p->description, sizeof(p->description), "Directional arrow keys remapped to WASD for PLATO gaming");
        p->is_builtin = true;

        plato_key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.type = PLATO_BIND_ACTION_RAW_BYTE;
        b.modifiers = 0;

        b.vkey = PLATO_VKEY_UP;    b.target.raw_byte = 'w'; snprintf(b.label, sizeof(b.label), "Move Up (W)");
        p->bindings[p->binding_count++] = b;

        b.vkey = PLATO_VKEY_DOWN;  b.target.raw_byte = 's'; snprintf(b.label, sizeof(b.label), "Move Down (S)");
        p->bindings[p->binding_count++] = b;

        b.vkey = PLATO_VKEY_LEFT;  b.target.raw_byte = 'a'; snprintf(b.label, sizeof(b.label), "Move Left (A)");
        p->bindings[p->binding_count++] = b;

        b.vkey = PLATO_VKEY_RIGHT; b.target.raw_byte = 'd'; snprintf(b.label, sizeof(b.label), "Move Right (D)");
        p->bindings[p->binding_count++] = b;
    }

    /* 3. Profilo Built-in: WASX / Avatar Mode */
    {
        plato_keymap_profile_t *p = &mgr->profiles[mgr->profile_count++];
        memset(p, 0, sizeof(*p));
        snprintf(p->name, sizeof(p->name), "WASX / Avatar");
        snprintf(p->description, sizeof(p->description), "Directional arrow keys remapped to WASX (Avatar/Oubliette)");
        p->is_builtin = true;

        plato_key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.type = PLATO_BIND_ACTION_RAW_BYTE;
        b.modifiers = 0;

        b.vkey = PLATO_VKEY_UP;    b.target.raw_byte = 'w'; snprintf(b.label, sizeof(b.label), "Avatar: Move Up (W)");
        p->bindings[p->binding_count++] = b;

        b.vkey = PLATO_VKEY_DOWN;  b.target.raw_byte = 'x'; snprintf(b.label, sizeof(b.label), "Avatar: Move Down (X)");
        p->bindings[p->binding_count++] = b;

        b.vkey = PLATO_VKEY_LEFT;  b.target.raw_byte = 'a'; snprintf(b.label, sizeof(b.label), "Avatar: Turn Left (A)");
        p->bindings[p->binding_count++] = b;

        b.vkey = PLATO_VKEY_RIGHT; b.target.raw_byte = 'd'; snprintf(b.label, sizeof(b.label), "Avatar: Turn Right (D)");
        p->bindings[p->binding_count++] = b;
    }

    /* 4. Profilo Built-in: Numpad Roguelike (Autentica matrice 8 direzioni per Avatar / Moria / Oubliette) */
    {
        plato_keymap_profile_t *p = &mgr->profiles[mgr->profile_count++];
        memset(p, 0, sizeof(*p));
        snprintf(p->name, sizeof(p->name), "Numpad Roguelike");
        snprintf(p->description, sizeof(p->description), "8-directional cardinal and diagonal movement matrix for Avatar/Moria");
        p->is_builtin = true;

        plato_key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.type = PLATO_BIND_ACTION_RAW_BYTE;
        b.modifiers = 0;

        /* Frecce Standard */
        b.vkey = PLATO_VKEY_UP;    b.target.raw_byte = 'w'; snprintf(b.label, sizeof(b.label), "North (W)");
        p->bindings[p->binding_count++] = b;
        b.vkey = PLATO_VKEY_DOWN;  b.target.raw_byte = 'x'; snprintf(b.label, sizeof(b.label), "South (X)");
        p->bindings[p->binding_count++] = b;
        b.vkey = PLATO_VKEY_LEFT;  b.target.raw_byte = 'a'; snprintf(b.label, sizeof(b.label), "West (A)");
        p->bindings[p->binding_count++] = b;
        b.vkey = PLATO_VKEY_RIGHT; b.target.raw_byte = 'd'; snprintf(b.label, sizeof(b.label), "East (D)");
        p->bindings[p->binding_count++] = b;

        /* Tastierino Numerico (8 Direzioni + Search/Wait) */
        b.vkey = PLATO_VKEY_NUMPAD8; b.target.raw_byte = 'w'; snprintf(b.label, sizeof(b.label), "Numpad 8: North (W)");
        p->bindings[p->binding_count++] = b;
        b.vkey = PLATO_VKEY_NUMPAD2; b.target.raw_byte = 'x'; snprintf(b.label, sizeof(b.label), "Numpad 2: South (X)");
        p->bindings[p->binding_count++] = b;
        b.vkey = PLATO_VKEY_NUMPAD4; b.target.raw_byte = 'a'; snprintf(b.label, sizeof(b.label), "Numpad 4: West (A)");
        p->bindings[p->binding_count++] = b;
        b.vkey = PLATO_VKEY_NUMPAD6; b.target.raw_byte = 'd'; snprintf(b.label, sizeof(b.label), "Numpad 6: East (D)");
        p->bindings[p->binding_count++] = b;
        b.vkey = PLATO_VKEY_NUMPAD7; b.target.raw_byte = 'q'; snprintf(b.label, sizeof(b.label), "Numpad 7: North-West (Q)");
        p->bindings[p->binding_count++] = b;
        b.vkey = PLATO_VKEY_NUMPAD9; b.target.raw_byte = 'e'; snprintf(b.label, sizeof(b.label), "Numpad 9: North-East (E)");
        p->bindings[p->binding_count++] = b;
        b.vkey = PLATO_VKEY_NUMPAD1; b.target.raw_byte = 'z'; snprintf(b.label, sizeof(b.label), "Numpad 1: South-West (Z)");
        p->bindings[p->binding_count++] = b;
        b.vkey = PLATO_VKEY_NUMPAD3; b.target.raw_byte = 'c'; snprintf(b.label, sizeof(b.label), "Numpad 3: South-East (C)");
        p->bindings[p->binding_count++] = b;
        b.vkey = PLATO_VKEY_NUMPAD5; b.target.raw_byte = 's'; snprintf(b.label, sizeof(b.label), "Numpad 5: Search/Wait (S)");
        p->bindings[p->binding_count++] = b;
    }

    snprintf(mgr->active_profile_name, sizeof(mgr->active_profile_name), "%s", mgr->profiles[0].name);
}

void plato_keymap_manager_init(plato_keymap_manager_t *mgr) {
    if (!mgr) return;
    memset(mgr, 0, sizeof(*mgr));
    init_builtin_profiles(mgr);
    plato_keymap_load_all(mgr);
}

void plato_keymap_manager_free(plato_keymap_manager_t *mgr) {
    if (!mgr) return;
    /* Buffer statici interni, azzeriamo per igiene */
    memset(mgr, 0, sizeof(*mgr));
}

int plato_keymap_get_count(const plato_keymap_manager_t *mgr) {
    return mgr ? mgr->profile_count : 0;
}

const plato_keymap_profile_t *plato_keymap_get_profile(const plato_keymap_manager_t *mgr, int index) {
    if (!mgr || index < 0 || index >= mgr->profile_count) return NULL;
    return &mgr->profiles[index];
}

plato_keymap_profile_t *plato_keymap_get_profile_mut(plato_keymap_manager_t *mgr, int index) {
    if (!mgr || index < 0 || index >= mgr->profile_count) return NULL;
    return &mgr->profiles[index];
}

const plato_keymap_profile_t *plato_keymap_get_active_profile(const plato_keymap_manager_t *mgr) {
    if (!mgr || mgr->profile_count == 0) return NULL;
    if (mgr->active_profile_index >= 0 && mgr->active_profile_index < mgr->profile_count) {
        return &mgr->profiles[mgr->active_profile_index];
    }
    return &mgr->profiles[0];
}

int plato_keymap_get_active_index(const plato_keymap_manager_t *mgr) {
    return mgr ? mgr->active_profile_index : 0;
}

bool plato_keymap_set_active_by_index(plato_keymap_manager_t *mgr, int index) {
    if (!mgr || index < 0 || index >= mgr->profile_count) return false;
    mgr->active_profile_index = index;
    snprintf(mgr->active_profile_name, sizeof(mgr->active_profile_name), "%s", mgr->profiles[index].name);
    return true;
}

bool plato_keymap_set_active_by_name(plato_keymap_manager_t *mgr, const char *name) {
    if (!mgr || !name) return false;
    for (int i = 0; i < mgr->profile_count; i++) {
        if (strcmp(mgr->profiles[i].name, name) == 0) {
            mgr->active_profile_index = i;
            snprintf(mgr->active_profile_name, sizeof(mgr->active_profile_name), "%s", name);
            return true;
        }
    }
    return false;
}

int plato_keymap_cycle_profile(plato_keymap_manager_t *mgr) {
    if (!mgr || mgr->profile_count == 0) return 0;
    mgr->active_profile_index = (mgr->active_profile_index + 1) % mgr->profile_count;
    snprintf(mgr->active_profile_name, sizeof(mgr->active_profile_name), "%s", mgr->profiles[mgr->active_profile_index].name);
    return mgr->active_profile_index;
}

int plato_keymap_add_profile(plato_keymap_manager_t *mgr, const plato_keymap_profile_t *profile) {
    if (!mgr || !profile || mgr->profile_count >= PLATO_KEYMAP_MAX_PROFILES) return -1;
    /* Verifica duplicati per nome */
    for (int i = 0; i < mgr->profile_count; i++) {
        if (strcmp(mgr->profiles[i].name, profile->name) == 0) {
            return -1;
        }
    }
    int idx = mgr->profile_count++;
    mgr->profiles[idx] = *profile;
    mgr->profiles[idx].is_builtin = false; /* I profili aggiunti dall'utente non sono mai built-in */
    return idx;
}

bool plato_keymap_duplicate_profile(plato_keymap_manager_t *mgr, int source_index, const char *new_name) {
    if (!mgr || source_index < 0 || source_index >= mgr->profile_count) return false;
    if (!new_name || new_name[0] == '\0' || mgr->profile_count >= PLATO_KEYMAP_MAX_PROFILES) return false;

    plato_keymap_profile_t copy = mgr->profiles[source_index];
    snprintf(copy.name, sizeof(copy.name), "%s", new_name);
    copy.is_builtin = false;
    return plato_keymap_add_profile(mgr, &copy) >= 0;
}

bool plato_keymap_delete_profile(plato_keymap_manager_t *mgr, int index) {
    if (!mgr || index < 0 || index >= mgr->profile_count) return false;
    if (mgr->profiles[index].is_builtin) return false; /* Vietato eliminare profili built-in */

    /* Cancella il file da disco */
    plato_keymap_delete_profile_file(mgr->profiles[index].name);

    for (int i = index; i < mgr->profile_count - 1; i++) {
        mgr->profiles[i] = mgr->profiles[i + 1];
    }
    mgr->profile_count--;

    if (mgr->active_profile_index >= mgr->profile_count) {
        mgr->active_profile_index = mgr->profile_count > 0 ? mgr->profile_count - 1 : 0;
    }
    if (mgr->profile_count > 0) {
        snprintf(mgr->active_profile_name, sizeof(mgr->active_profile_name), "%s", mgr->profiles[mgr->active_profile_index].name);
    }
    return true;
}

bool plato_keymap_update_profile_info(plato_keymap_manager_t *mgr, int index, const char *new_name, const char *new_desc) {
    if (!mgr || index < 0 || index >= mgr->profile_count) return false;
    if (mgr->profiles[index].is_builtin) return false;

    if (new_name && new_name[0] != '\0' && strcmp(mgr->profiles[index].name, new_name) != 0) {
        plato_keymap_delete_profile_file(mgr->profiles[index].name);
        snprintf(mgr->profiles[index].name, sizeof(mgr->profiles[index].name), "%s", new_name);
        if (mgr->active_profile_index == index) {
            snprintf(mgr->active_profile_name, sizeof(mgr->active_profile_name), "%s", new_name);
        }
    }
    if (new_desc) {
        snprintf(mgr->profiles[index].description, sizeof(mgr->profiles[index].description), "%s", new_desc);
    }
    plato_keymap_save_profile(&mgr->profiles[index]);
    return true;
}

bool plato_keymap_rename_profile(plato_keymap_manager_t *mgr, int index, const char *new_name) {
    return plato_keymap_update_profile_info(mgr, index, new_name, NULL);
}

bool plato_keymap_profile_add_binding(plato_keymap_profile_t *prof, const plato_key_binding_t *binding) {
    if (!prof || !binding || prof->is_builtin || prof->binding_count >= PLATO_KEYMAP_MAX_BINDINGS) return false;
    prof->bindings[prof->binding_count++] = *binding;
    return true;
}

bool plato_keymap_profile_set_binding(plato_keymap_profile_t *prof, int index, const plato_key_binding_t *binding) {
    if (!prof || !binding || prof->is_builtin || index < 0 || index >= prof->binding_count) return false;
    prof->bindings[index] = *binding;
    return true;
}

bool plato_keymap_profile_remove_binding(plato_keymap_profile_t *prof, int binding_index) {
    if (!prof || prof->is_builtin || binding_index < 0 || binding_index >= prof->binding_count) return false;
    for (int i = binding_index; i < prof->binding_count - 1; i++) {
        prof->bindings[i] = prof->bindings[i + 1];
    }
    prof->binding_count--;
    return true;
}

void plato_keymap_profile_clear_bindings(plato_keymap_profile_t *prof) {
    if (!prof || prof->is_builtin) return;
    prof->binding_count = 0;
}

/* Lookup O(1) in memoria cache */
const plato_key_binding_t *plato_keymap_lookup(const plato_keymap_profile_t *prof, const plato_key_event_t *ev) {
    if (!prof || !ev || prof->binding_count == 0) return NULL;

    uint8_t ev_mods = 0;
    if (ev->shift) ev_mods |= PLATO_KEY_MOD_SHIFT;
    if (ev->ctrl)  ev_mods |= PLATO_KEY_MOD_CTRL;
    if (ev->alt)   ev_mods |= PLATO_KEY_MOD_ALT;
    if (ev->gui)   ev_mods |= PLATO_KEY_MOD_GUI;

    for (int i = 0; i < prof->binding_count; i++) {
        const plato_key_binding_t *b = &prof->bindings[i];

        bool key_match = false;
        if (b->vkey != 0 && b->vkey == (uint16_t)ev->vkey) {
            key_match = true;
        } else if (b->codepoint != 0 && (b->codepoint == ev->codepoint || b->codepoint == ev->unmod_codepoint)) {
            key_match = true;
        }
        if (!key_match) continue;

        /* Se il binding non richiede modificatori specifici */
        if (b->modifiers == 0) {
            /* Accetta sia senza modificatori, sia con solo Shift nel caso di caratteri raw (gestione automatica Shift) */
            if (ev_mods == 0 || (ev_mods == PLATO_KEY_MOD_SHIFT && b->type == PLATO_BIND_ACTION_RAW_BYTE)) {
                return b;
            }
        } else {
            if (b->modifiers == ev_mods) {
                return b;
            }
        }
    }
    return NULL;
}

/* Serializzazione e Deserializzazione JSON nativa C11 */
static void sanitize_filename(const char *name, char *out, size_t max_len) {
    size_t j = 0;
    for (size_t i = 0; name[i] != '\0' && j + 5 < max_len; i++) {
        char c = name[i];
        if (isalnum((unsigned char)c) || c == '_' || c == '-') {
            out[j++] = c;
        } else if (c == ' ') {
            out[j++] = '_';
        }
    }
    out[j] = '\0';
    if (j == 0) {
        snprintf(out, max_len, "profile");
    }
}

bool plato_keymap_save_profile(const plato_keymap_profile_t *prof) {
    if (!prof || prof->is_builtin) return false;

    char dir[512];
    plato_keymap_get_storage_dir(dir, sizeof(dir));

    char fname[128];
    sanitize_filename(prof->name, fname, sizeof(fname));

    char path[640];
#ifdef _WIN32
    snprintf(path, sizeof(path), "%s\\%s.json", dir, fname);
#else
    snprintf(path, sizeof(path), "%s/%s.json", dir, fname);
#endif

    FILE *f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "{\n");
    fprintf(f, "  \"name\": \"%s\",\n", prof->name);
    fprintf(f, "  \"description\": \"%s\",\n", prof->description);
    fprintf(f, "  \"is_builtin\": false,\n");
    fprintf(f, "  \"bindings\": [\n");

    for (int i = 0; i < prof->binding_count; i++) {
        const plato_key_binding_t *b = &prof->bindings[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"vkey\": %u,\n", (unsigned)b->vkey);
        fprintf(f, "      \"codepoint\": %u,\n", (unsigned)b->codepoint);
        fprintf(f, "      \"modifiers\": %u,\n", (unsigned)b->modifiers);
        fprintf(f, "      \"type\": %d,\n", (int)b->type);
        if (b->type == PLATO_BIND_ACTION_RAW_BYTE) {
            fprintf(f, "      \"target\": %u,\n", (unsigned)b->target.raw_byte);
        } else if (b->type == PLATO_BIND_ACTION_PLATO_KEY) {
            fprintf(f, "      \"target\": %u,\n", (unsigned)b->target.plato_key);
        } else {
            fprintf(f, "      \"target\": %u,\n", (unsigned)b->target.sequence);
        }
        fprintf(f, "      \"label\": \"%s\"\n", b->label);
        fprintf(f, "    }%s\n", (i == prof->binding_count - 1) ? "" : ",");
    }

    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);
    return true;
}

/* Parser JSON minimale e leggero */
static const char *json_find_key(const char *src, const char *key) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *pos = strstr(src, pattern);
    if (!pos) return NULL;
    pos += strlen(pattern);
    while (*pos && (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n' || *pos == ':')) pos++;
    return pos;
}

static void json_parse_string(const char *src, char *out, size_t max_len) {
    if (!src || *src != '\"') {
        if (max_len > 0) out[0] = '\0';
        return;
    }
    src++;
    size_t j = 0;
    while (*src && *src != '\"' && j + 1 < max_len) {
        if (*src == '\\' && *(src + 1)) src++;
        out[j++] = *src++;
    }
    out[j] = '\0';
}

static int json_parse_int(const char *src) {
    if (!src) return 0;
    return atoi(src);
}

bool plato_keymap_load_profile(const char *filepath, plato_keymap_profile_t *out_prof) {
    if (!filepath || !out_prof) return false;

    FILE *f = fopen(filepath, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0 || len > 1024 * 1024) {
        fclose(f);
        return false;
    }

    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return false;
    }

    size_t read_bytes = fread(buf, 1, (size_t)len, f);
    buf[read_bytes] = '\0';
    fclose(f);

    memset(out_prof, 0, sizeof(*out_prof));
    out_prof->is_builtin = false;

    const char *k_name = json_find_key(buf, "name");
    if (k_name) json_parse_string(k_name, out_prof->name, sizeof(out_prof->name));

    const char *k_desc = json_find_key(buf, "description");
    if (k_desc) json_parse_string(k_desc, out_prof->description, sizeof(out_prof->description));

    /* Parse bindings array */
    const char *k_bind = json_find_key(buf, "bindings");
    if (k_bind) {
        const char *arr = strchr(k_bind, '[');
        if (arr) {
            const char *curr = arr;
            while ((curr = strchr(curr, '{')) != NULL) {
                const char *obj_end = strchr(curr, '}');
                if (!obj_end) break;
                if (out_prof->binding_count >= PLATO_KEYMAP_MAX_BINDINGS) break;

                plato_key_binding_t *b = &out_prof->bindings[out_prof->binding_count];
                memset(b, 0, sizeof(*b));

                const char *kv = json_find_key(curr, "vkey");
                if (kv && kv < obj_end) b->vkey = (uint16_t)json_parse_int(kv);

                const char *kc = json_find_key(curr, "codepoint");
                if (kc && kc < obj_end) b->codepoint = (uint32_t)json_parse_int(kc);

                const char *km = json_find_key(curr, "modifiers");
                if (km && km < obj_end) b->modifiers = (uint8_t)json_parse_int(km);

                const char *kt = json_find_key(curr, "type");
                if (kt && kt < obj_end) b->type = (plato_bind_action_type_t)json_parse_int(kt);

                const char *ktgt = json_find_key(curr, "target");
                if (ktgt && ktgt < obj_end) {
                    uint32_t val = (uint32_t)json_parse_int(ktgt);
                    if (b->type == PLATO_BIND_ACTION_RAW_BYTE) b->target.raw_byte = (uint8_t)val;
                    else if (b->type == PLATO_BIND_ACTION_PLATO_KEY) b->target.plato_key = (uint16_t)val;
                    else b->target.sequence = val;
                }

                const char *kl = json_find_key(curr, "label");
                if (kl && kl < obj_end) json_parse_string(kl, b->label, sizeof(b->label));

                out_prof->binding_count++;
                curr = obj_end + 1;
            }
        }
    }

    free(buf);
    return (out_prof->name[0] != '\0');
}

bool plato_keymap_delete_profile_file(const char *name) {
    if (!name || name[0] == '\0') return false;
    char dir[512];
    plato_keymap_get_storage_dir(dir, sizeof(dir));
    char fname[128];
    sanitize_filename(name, fname, sizeof(fname));
    char path[640];
#ifdef _WIN32
    snprintf(path, sizeof(path), "%s\\%s.json", dir, fname);
    return DeleteFileA(path) != 0;
#else
    snprintf(path, sizeof(path), "%s/%s.json", dir, fname);
    return unlink(path) == 0;
#endif
}

bool plato_keymap_load_all(plato_keymap_manager_t *mgr) {
    if (!mgr) return false;

    char dir[512];
    plato_keymap_get_storage_dir(dir, sizeof(dir));

#ifdef _WIN32
    char search_pattern[MAX_PATH];
    snprintf(search_pattern, sizeof(search_pattern), "%s\\*.json", dir);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_pattern, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                char full_path[MAX_PATH];
                snprintf(full_path, sizeof(full_path), "%s\\%s", dir, fd.cFileName);
                plato_keymap_profile_t p;
                if (plato_keymap_load_profile(full_path, &p)) {
                    plato_keymap_add_profile(mgr, &p);
                }
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
#else
    DIR *dp = opendir(dir);
    if (dp) {
        struct dirent *ep;
        while ((ep = readdir(dp)) != NULL) {
            size_t l = strlen(ep->d_name);
            if (l > 5 && strcmp(ep->d_name + l - 5, ".json") == 0) {
                char full_path[640];
                snprintf(full_path, sizeof(full_path), "%s/%s", dir, ep->d_name);
                plato_keymap_profile_t p;
                if (plato_keymap_load_profile(full_path, &p)) {
                    plato_keymap_add_profile(mgr, &p);
                }
            }
        }
        closedir(dp);
    }
#endif

    /* Carica stato profilo attivo se salvato */
    char state_path[640];
#ifdef _WIN32
    snprintf(state_path, sizeof(state_path), "%s\\active.txt", dir);
#else
    snprintf(state_path, sizeof(state_path), "%s/active.txt", dir);
#endif
    FILE *fs = fopen(state_path, "r");
    if (fs) {
        char act[PLATO_KEYMAP_NAME_LEN];
        if (fgets(act, sizeof(act), fs)) {
            size_t sl = strlen(act);
            while (sl > 0 && (act[sl - 1] == '\r' || act[sl - 1] == '\n')) act[--sl] = '\0';
            plato_keymap_set_active_by_name(mgr, act);
        }
        fclose(fs);
    }

    return true;
}

bool plato_keymap_save_all(const plato_keymap_manager_t *mgr) {
    if (!mgr) return false;
    for (int i = 0; i < mgr->profile_count; i++) {
        if (!mgr->profiles[i].is_builtin) {
            plato_keymap_save_profile(&mgr->profiles[i]);
        }
    }
    char dir[512];
    plato_keymap_get_storage_dir(dir, sizeof(dir));
    char state_path[640];
#ifdef _WIN32
    snprintf(state_path, sizeof(state_path), "%s\\active.txt", dir);
#else
    snprintf(state_path, sizeof(state_path), "%s/active.txt", dir);
#endif
    FILE *fs = fopen(state_path, "w");
    if (fs) {
        fprintf(fs, "%s\n", mgr->active_profile_name);
        fclose(fs);
    }
    return true;
}
