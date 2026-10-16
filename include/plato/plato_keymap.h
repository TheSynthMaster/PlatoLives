#ifndef PLATO_KEYMAP_H
#define PLATO_KEYMAP_H

#include "plato_types.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define PLATO_KEYMAP_MAX_PROFILES 32
#define PLATO_KEYMAP_MAX_BINDINGS 64
#define PLATO_KEYMAP_NAME_LEN     64
#define PLATO_KEYMAP_DESC_LEN     128

/* Maschere modificatori per i binding */
#define PLATO_KEY_MOD_NONE   0
#define PLATO_KEY_MOD_SHIFT  (1 << 0)
#define PLATO_KEY_MOD_CTRL   (1 << 1)
#define PLATO_KEY_MOD_ALT    (1 << 2)
#define PLATO_KEY_MOD_GUI    (1 << 3)

/* Tipologie di azione associate a un tasto */
typedef enum {
    PLATO_BIND_ACTION_NONE = 0,
    PLATO_BIND_ACTION_PLATO_KEY,   /* Tasto speciale PLATO (NEXT, BACK, HELP, LAB, etc.) */
    PLATO_BIND_ACTION_RAW_BYTE,    /* Singolo byte ASCII (es. 'w', 'W', 0x1B) */
    PLATO_BIND_ACTION_PLATO_SEQ    /* Sequenza canonica PLATO a 32 bit stile pterm */
} plato_bind_action_type_t;

/* Forward declaration non circolare dell'evento tastiera */
struct plato_key_event;
typedef struct plato_key_event plato_key_event_t;

/* Singola associazione Tasto Fisico -> Azione */
typedef struct {
    uint16_t vkey;                  /* plato_virtual_key_t o 0 se codepoint */
    uint32_t codepoint;             /* Codepoint ASCII / Unicode o 0 se vkey */
    uint8_t  modifiers;             /* Maschera modificatori: PLATO_KEY_MOD_* */
    plato_bind_action_type_t type;  /* Tipo di azione */
    union {
        uint16_t plato_key;         /* Codice tasto PLATO (es. PLATO_KEY_NEXT o plato_special_key_t) */
        uint8_t  raw_byte;          /* Singolo byte raw */
        uint32_t sequence;          /* Sequenza a 32-bit K(a,b,c,d) */
    } target;
    char label[32];                 /* Etichetta descrittiva (es. "Avatar: Move Down", "PLATO Next") */
} plato_key_binding_t;

/* Profilo completo */
typedef struct {
    char name[PLATO_KEYMAP_NAME_LEN];
    char description[PLATO_KEYMAP_DESC_LEN];
    bool is_builtin;                /* true = Read-only (Standard, WASD, WASX); false = Utente */
    int  binding_count;
    plato_key_binding_t bindings[PLATO_KEYMAP_MAX_BINDINGS];
} plato_keymap_profile_t;

/* Gestore globale dei profili di tastiera */
typedef struct {
    plato_keymap_profile_t profiles[PLATO_KEYMAP_MAX_PROFILES];
    int profile_count;
    int active_profile_index;       /* Indice del profilo correntemente attivo */
    char active_profile_name[PLATO_KEYMAP_NAME_LEN];
} plato_keymap_manager_t;

/* Inizializzazione e ciclo di vita */
void plato_keymap_manager_init(plato_keymap_manager_t *mgr);
void plato_keymap_manager_free(plato_keymap_manager_t *mgr);

/* Accesso e navigazione profili */
int plato_keymap_get_count(const plato_keymap_manager_t *mgr);
const plato_keymap_profile_t *plato_keymap_get_profile(const plato_keymap_manager_t *mgr, int index);
plato_keymap_profile_t *plato_keymap_get_profile_mut(plato_keymap_manager_t *mgr, int index);
const plato_keymap_profile_t *plato_keymap_get_active_profile(const plato_keymap_manager_t *mgr);
int plato_keymap_get_active_index(const plato_keymap_manager_t *mgr);
bool plato_keymap_set_active_by_index(plato_keymap_manager_t *mgr, int index);
bool plato_keymap_set_active_by_name(plato_keymap_manager_t *mgr, const char *name);
int plato_keymap_cycle_profile(plato_keymap_manager_t *mgr);

/* Modifica profili utente */
int plato_keymap_add_profile(plato_keymap_manager_t *mgr, const plato_keymap_profile_t *profile);
bool plato_keymap_duplicate_profile(plato_keymap_manager_t *mgr, int source_index, const char *new_name);
bool plato_keymap_delete_profile(plato_keymap_manager_t *mgr, int index);
bool plato_keymap_rename_profile(plato_keymap_manager_t *mgr, int index, const char *new_name);
bool plato_keymap_update_profile_info(plato_keymap_manager_t *mgr, int index, const char *new_name, const char *new_desc);

/* Gestione binding */
bool plato_keymap_profile_add_binding(plato_keymap_profile_t *prof, const plato_key_binding_t *binding);
bool plato_keymap_profile_set_binding(plato_keymap_profile_t *prof, int index, const plato_key_binding_t *binding);
bool plato_keymap_profile_remove_binding(plato_keymap_profile_t *prof, int binding_index);
void plato_keymap_profile_clear_bindings(plato_keymap_profile_t *prof);

/* Lookup O(1) in memoria cache */
const plato_key_binding_t *plato_keymap_lookup(const plato_keymap_profile_t *prof, const plato_key_event_t *ev);

/* Persistenza I/O (JSON) */
void plato_keymap_get_storage_dir(char *out_path, size_t max_len);
bool plato_keymap_save_profile(const plato_keymap_profile_t *prof);
bool plato_keymap_load_profile(const char *filepath, plato_keymap_profile_t *out_prof);
bool plato_keymap_delete_profile_file(const char *name);
bool plato_keymap_load_all(plato_keymap_manager_t *mgr);
bool plato_keymap_save_all(const plato_keymap_manager_t *mgr);

#endif /* PLATO_KEYMAP_H */
