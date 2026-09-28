#ifndef PLATO_SCRIPT_H
#define PLATO_SCRIPT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "plato/plato_terminal.h"

#define PLATO_DEFAULT_AUTOLOGIN_SCRIPT \
"# ==============================================================================\n" \
"# CYBER1 ADAPTIVE AUTO-LOGIN & SELF-HEALING RECOVERY\n" \
"# ==============================================================================\n\n" \
"# Credentials Configuration\n" \
"let username = \"user\"\n" \
"let group = \"group\"\n" \
"let password = \"password\"\n\n" \
"@start:\n\n" \
"# 1. State check: identify current terminal screen\n" \
"let pos_user = -1\n" \
"find_text regex \"Type +your +CYBIS +name\" into pos_user, dummy_y\n" \
"if pos_user >= 0 goto do_user\n\n" \
"let pos_group = -1\n" \
"find_text regex \"Type +the +name +of +your +CYBIS +group.\" into pos_group, dummy_y\n" \
"if pos_group >= 0 goto do_group\n\n" \
"let pos_pwd = -1\n" \
"find_text regex \"Enter +your +password\" into pos_pwd, dummy_y\n" \
"if pos_pwd >= 0 goto do_password\n\n" \
"let pos_banner = -1\n" \
"find_text regex \"Press +NEXT +to begin\" into pos_banner, dummy_y\n" \
"if pos_banner >= 0 goto do_next\n\n" \
"# If no known prompt is active, trigger reset recovery\n" \
"goto reset\n\n" \
"# --- STEP 0: Press initial NEXT ---\n" \
"@do_next:\n" \
"key NEXT\n\n" \
"# --- STEP 1: Enter Username ---\n" \
"wait_text regex \"Type +your +CYBIS +name\" timeout 5s\n" \
"@do_user:\n" \
"send username\n" \
"key NEXT\n\n" \
"# --- STEP 2: Enter Group ---\n" \
"wait_text regex \"Type +the +name +of +your +CYBIS +group.\" timeout 5s\n" \
"@do_group:\n" \
"send group\n" \
"key SHIFT-STOP\n\n" \
"# --- STEP 3: Enter Password ---\n" \
"wait_text regex \"Enter +your +password\" timeout 5s\n" \
"@do_password:\n" \
"send password\n" \
"key NEXT\n\n" \
"# Allow the system 2 seconds to authenticate credentials\n" \
"wait 2s\n\n" \
"# --- STEP 4: Login Verification ---\n" \
"let login_ok = -1\n" \
"find_text regex \"Choose +a +lesson\" into login_ok, dummy_y\n\n" \
"# If \"Choose a lesson\" was not reached, trigger reset recovery\n" \
"if login_ok < 0 goto reset\n\n" \
"# Login succeeded\n" \
"exit\n\n" \
"# ==============================================================================\n" \
"# SELF-HEALING RESET RECOVERY\n" \
"# ==============================================================================\n" \
"@reset:\n" \
"let r_banner = -1\n" \
"let r_user = -1\n\n" \
"for reset_step = 1 to 10\n" \
"    key SHIFT-STOP\n" \
"    wait 400ms\n\n" \
"    # Check if we reached username prompt\n" \
"    find_text regex \"Type +your +CYBIS +name\" into r_user, dummy_y\n" \
"    if r_user >= 0 goto do_user\n\n" \
"    # Check if we reached initial banner\n" \
"    find_text regex \"Press +NEXT +to begin\" into r_banner, dummy_y\n" \
"    if r_banner >= 0 goto do_next\n" \
"next reset_step\n\n" \
"exit"

#define PLATO_SCRIPT_MAX_NAME     64
#define PLATO_SCRIPT_MAX_HOTKEY   32
#define PLATO_SCRIPT_MAX_BODY     16384
#define PLATO_SCRIPT_MAX_VARS     128
#define PLATO_SCRIPT_VAR_NAME_LEN 32

#define PLATO_HOTKEY_MOD_NONE     0
#define PLATO_HOTKEY_MOD_CTRL     (1 << 0)
#define PLATO_HOTKEY_MOD_SHIFT    (1 << 1)
#define PLATO_HOTKEY_MOD_ALT      (1 << 2)
#define PLATO_HOTKEY_MOD_CMD      (1 << 3)

typedef struct {
    char id[64];
    char name[PLATO_SCRIPT_MAX_NAME];
    bool enabled;
    uint32_t hotkey_modifiers;
    uint32_t hotkey_key;
    char hotkey_display[PLATO_SCRIPT_MAX_HOTKEY];
    int char_delay_ms;    /* default: 250 */
    int next_delay_ms;    /* default: 500 */
    int command_delay_ms; /* default: 0 */
    char body[PLATO_SCRIPT_MAX_BODY];
} plato_script_t;

typedef struct {
    plato_script_t *scripts;
    size_t count;
    size_t capacity;
} plato_script_list_t;

plato_script_list_t *plato_scripts_create(void);
void plato_scripts_free(plato_script_list_t *list);
bool plato_scripts_load(plato_script_list_t *list, const char *filepath);
bool plato_scripts_save(const plato_script_list_t *list, const char *filepath);
int  plato_scripts_add(plato_script_list_t *list, const plato_script_t *script);
int  plato_scripts_clone(plato_script_list_t *list, size_t index);
bool plato_scripts_delete(plato_script_list_t *list, size_t index);
const char *plato_scripts_get_default_path(void);
int plato_script_run_file(const char *filepath, const char *host, int port);

typedef struct plato_script_runner plato_script_runner_t;

plato_script_runner_t* plato_script_create(plato_terminal_t *term);
void plato_script_destroy(plato_script_runner_t *runner);
bool plato_script_start(plato_script_runner_t *runner, const char *script_text);
bool plato_script_start_ex(plato_script_runner_t *runner, const plato_script_t *script);
void plato_script_stop(plato_script_runner_t *runner);
bool plato_script_is_running(const plato_script_runner_t *runner);

int64_t plato_script_get_var(plato_script_runner_t *runner, const char *name, bool *found);
bool    plato_script_set_var(plato_script_runner_t *runner, const char *name, int64_t val);


const char* plato_script_get_manual_text(void);
#endif /* PLATO_SCRIPT_H */
