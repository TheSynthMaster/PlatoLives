#ifndef PLATO_PROFILE_H
#define PLATO_PROFILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PLATO_MAX_PROFILES 32
#define PLATO_SCRIPT_MAX_LEN 4096

typedef struct {
    char id[64];
    char name[64];
    char host[128];
    int port;
    int display_mode;        /* 0: Real Plasma, 1: Crisp Mono, 2: Split Mono, 3: Crisp Color, 4: Real CRT */
    int persistence_ms;      /* 100, 200, 500, 1000, 2000, 5000 */
    int plasma_distortion;   /* 0: None, 1: Barrel, 2: Cylindrical */
    int crt_beam_level;      /* 0: Standard, 1: High, 2: Ultra */
    int crt_persistence_ms;  /* 20, 200, 500, 1000, 2000, 5000 */
    int crt_distortion;      /* 0: None, 1: Barrel, 2: Cylindrical */
    bool full_screen;
    bool is_default;
    bool startup_script_enabled;
    char startup_script[PLATO_SCRIPT_MAX_LEN];
} plato_profile_t;

typedef struct {
    plato_profile_t profiles[PLATO_MAX_PROFILES];
    size_t count;
    int default_index;
} plato_profile_list_t;

void plato_profile_get_storage_path(char *out_path, size_t max_len);
bool plato_profiles_load(plato_profile_list_t *list);
bool plato_profiles_save(const plato_profile_list_t *list);
plato_profile_t* plato_profiles_get_default(plato_profile_list_t *list);
bool plato_profiles_set_default(plato_profile_list_t *list, size_t index);
bool plato_profiles_add(plato_profile_list_t *list, const plato_profile_t *prof);
bool plato_profiles_delete(plato_profile_list_t *list, size_t index);

#endif /* PLATO_PROFILE_H */
