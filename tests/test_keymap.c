#include "plato/plato_keymap.h"
#include "plato/plato_keyboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void test_builtins_and_cycle(void) {
    printf("[TEST] 1. Built-in profiles and cycling...\n");
    plato_keymap_manager_t mgr;
    plato_keymap_manager_init(&mgr);
    plato_keymap_set_active_by_index(&mgr, 0);

    assert(plato_keymap_get_count(&mgr) >= 4);

    const plato_keymap_profile_t *p0 = plato_keymap_get_profile(&mgr, 0);
    assert(p0 != NULL);
    assert(strcmp(p0->name, "Standard PLATO") == 0);
    assert(p0->is_builtin == true);
    assert(p0->binding_count == 0);

    const plato_keymap_profile_t *p1 = plato_keymap_get_profile(&mgr, 1);
    assert(p1 != NULL);
    assert(strcmp(p1->name, "WASD Gaming") == 0);
    assert(p1->is_builtin == true);
    assert(p1->binding_count == 4);

    const plato_keymap_profile_t *p2 = plato_keymap_get_profile(&mgr, 2);
    assert(p2 != NULL);
    assert(strcmp(p2->name, "WASX / Avatar") == 0);
    assert(p2->is_builtin == true);
    assert(p2->binding_count == 4);
    const plato_keymap_profile_t *p3 = plato_keymap_get_profile(&mgr, 3);
    assert(p3 != NULL);
    assert(strcmp(p3->name, "Numpad Roguelike") == 0);
    assert(p3->binding_count == 13);

    const plato_keymap_profile_t *p4 = plato_keymap_get_profile(&mgr, 4);
    assert(p4 != NULL);
    assert(strcmp(p4->name, "WADX") == 0);
    assert(p4->is_builtin == true);
    assert(p4->binding_count == 4);

    /* Test cycling */
    int idx0 = plato_keymap_get_active_index(&mgr);
    assert(idx0 == 0);
    int idx1 = plato_keymap_cycle_profile(&mgr);
    assert(idx1 == 1);
    assert(plato_keymap_get_active_index(&mgr) == 1);
    assert(strcmp(mgr.active_profile_name, "WASD Gaming") == 0);

    plato_keymap_manager_free(&mgr);
    printf("       -> OK!\n");
}

static void test_lookup(void) {
    printf("[TEST] 2. Fast O(1) lookup...\n");
    plato_keymap_manager_t mgr;
    plato_keymap_manager_init(&mgr);
    plato_keymap_set_active_by_index(&mgr, 0);

    const plato_keymap_profile_t *p_std = plato_keymap_get_profile(&mgr, 0);
    plato_key_event_t ev_up = { .vkey = PLATO_VKEY_UP, .codepoint = 0, .shift = false, .ctrl = false, .alt = false, .gui = false };
    assert(plato_keymap_lookup(p_std, &ev_up) == NULL);

    const plato_keymap_profile_t *p_wasd = plato_keymap_get_profile(&mgr, 1);
    const plato_key_binding_t *b_up = plato_keymap_lookup(p_wasd, &ev_up);
    assert(b_up != NULL);
    assert(b_up->type == PLATO_BIND_ACTION_RAW_BYTE);
    assert(b_up->target.raw_byte == 'w');

    plato_key_event_t ev_down = { .vkey = PLATO_VKEY_DOWN, .codepoint = 0, .shift = false, .ctrl = false, .alt = false, .gui = false };
    const plato_key_binding_t *b_down_wasd = plato_keymap_lookup(p_wasd, &ev_down);
    assert(b_down_wasd != NULL);
    assert(b_down_wasd->target.raw_byte == 's');

    const plato_keymap_profile_t *p_wasx = plato_keymap_get_profile(&mgr, 2);
    const plato_key_binding_t *b_down_wasx = plato_keymap_lookup(p_wasx, &ev_down);
    assert(b_down_wasx != NULL);
    assert(b_down_wasx->target.raw_byte == 'x');

    plato_keymap_manager_free(&mgr);
    printf("       -> OK!\n");
}

static void test_crud_and_json_roundtrip(void) {
    printf("[TEST] 3. Profile CRUD and JSON serialization...\n");
    plato_keymap_manager_t mgr;
    plato_keymap_manager_init(&mgr);
    plato_keymap_set_active_by_index(&mgr, 0);

    bool dup_ok = plato_keymap_duplicate_profile(&mgr, 2, "Avatar Custom Test");
    assert(dup_ok);

    int new_idx = mgr.profile_count - 1;
    plato_keymap_profile_t *p_custom = plato_keymap_get_profile_mut(&mgr, new_idx);
    assert(p_custom != NULL);
    assert(p_custom->is_builtin == false);
    assert(strcmp(p_custom->name, "Avatar Custom Test") == 0);

    plato_key_binding_t b_custom;
    memset(&b_custom, 0, sizeof(b_custom));
    b_custom.vkey = PLATO_VKEY_TAB;
    b_custom.type = PLATO_BIND_ACTION_PLATO_KEY;
    b_custom.target.plato_key = PLATO_KEY_NEXT;
    snprintf(b_custom.label, sizeof(b_custom.label), "Tab to PLATO NEXT");
    bool add_ok = plato_keymap_profile_add_binding(p_custom, &b_custom);
    assert(add_ok);
    assert(p_custom->binding_count == 5);

    bool save_ok = plato_keymap_save_profile(p_custom);
    assert(save_ok);

    char dir[512];
    plato_keymap_get_storage_dir(dir, sizeof(dir));
    char path[640];
    snprintf(path, sizeof(path), "%s/Avatar_Custom_Test.json", dir);

    plato_keymap_profile_t loaded;
    bool load_ok = plato_keymap_load_profile(path, &loaded);
    assert(load_ok);
    assert(strcmp(loaded.name, "Avatar Custom Test") == 0);
    assert(loaded.binding_count == 5);
    assert(loaded.bindings[4].vkey == PLATO_VKEY_TAB);
    assert(loaded.bindings[4].type == PLATO_BIND_ACTION_PLATO_KEY);
    assert(loaded.bindings[4].target.plato_key == PLATO_KEY_NEXT);

    bool del_ok = plato_keymap_delete_profile(&mgr, new_idx);
    assert(del_ok);

    plato_keymap_manager_free(&mgr);
    printf("       -> OK!\n");
}

static void test_keyboard_dispatcher_integration(void) {
    printf("[TEST] 4. Keyboard State & Dispatcher Integration...\n");
    plato_keyboard_state_t state;
    plato_keyboard_state_init(&state);
    plato_keyboard_set_active_keymap_by_index(&state, 0);

    /* 1. Default: Standard PLATO */
    assert(strcmp(plato_keyboard_get_active_keymap_name(&state), "Standard PLATO") == 0);
    assert(plato_keyboard_get_arrow_mode(&state) == PLATO_ARROW_MODE_STANDARD);

    /* Dispatch with term=NULL returns true for handled keys */
    plato_key_event_t ev_up = { .vkey = PLATO_VKEY_UP, .codepoint = 0, .shift = false, .ctrl = false, .alt = false, .gui = false };
    bool handled_std = plato_keyboard_dispatch(&state, NULL, &ev_up, 1.0);
    assert(handled_std == true);

    /* 2. Switch to WASD Gaming */
    plato_keyboard_set_arrow_mode(&state, PLATO_ARROW_MODE_WASD);
    assert(strcmp(plato_keyboard_get_active_keymap_name(&state), "WASD Gaming") == 0);
    assert(plato_keyboard_get_arrow_mode(&state) == PLATO_ARROW_MODE_WASD);

    bool handled_wasd = plato_keyboard_dispatch(&state, NULL, &ev_up, 2.0);
    assert(handled_wasd == true);

    /* 3. Cycle profile -> WASX / Avatar */
    plato_arrow_mode_t cycled = plato_keyboard_cycle_arrow_mode(&state);
    assert(cycled == PLATO_ARROW_MODE_WASX);
    assert(strcmp(plato_keyboard_get_active_keymap_name(&state), "WASX / Avatar") == 0);

    /* 4. Switch by index back to 0 */
    bool set_ok = plato_keyboard_set_active_keymap_by_index(&state, 0);
    assert(set_ok == true);
    assert(strcmp(plato_keyboard_get_active_keymap_name(&state), "Standard PLATO") == 0);
    assert(plato_keyboard_get_arrow_mode(&state) == PLATO_ARROW_MODE_STANDARD);

    printf("       -> OK!\n");
}


static void test_arbitrary_key_remapping(void) {
    printf("[TEST] 5. Arbitrary key remapping ('a' -> 'f', ';' -> '+')...\n");
    plato_keymap_manager_t mgr;
    plato_keymap_manager_init(&mgr);
    plato_keymap_set_active_by_index(&mgr, 0);

    plato_keymap_profile_t custom;
    memset(&custom, 0, sizeof(custom));
    snprintf(custom.name, sizeof(custom.name), "Custom Remap Test");

    /* Rimappa 'a' -> 'f' */
    plato_key_binding_t b1 = { .codepoint = 'a', .type = PLATO_BIND_ACTION_RAW_BYTE, .target.raw_byte = 'f' };
    plato_keymap_profile_add_binding(&custom, &b1);

    /* Rimappa ';' -> '+' */
    plato_key_binding_t b2 = { .codepoint = ';', .type = PLATO_BIND_ACTION_RAW_BYTE, .target.raw_byte = '+' };
    plato_keymap_profile_add_binding(&custom, &b2);

    int idx = plato_keymap_add_profile(&mgr, &custom);
    assert(idx >= 0);

    const plato_keymap_profile_t *p = plato_keymap_get_profile(&mgr, idx);
    assert(p != NULL);
    assert(p->binding_count == 2);

    plato_key_event_t ev_a = { .codepoint = 'a', .unmod_codepoint = 'a' };
    const plato_key_binding_t *res_a = plato_keymap_lookup(p, &ev_a);
    assert(res_a != NULL);
    assert(res_a->target.raw_byte == 'f');

    plato_key_event_t ev_semi = { .codepoint = ';', .unmod_codepoint = ';' };
    const plato_key_binding_t *res_semi = plato_keymap_lookup(p, &ev_semi);
    assert(res_semi != NULL);
    assert(res_semi->target.raw_byte == '+');

    plato_keymap_manager_free(&mgr);
    printf("       -> OK!\n");
}

int main(void) {
    printf("=== RUNNING KEYMAP & DISPATCHER UNIT TESTS ===\n");
    test_builtins_and_cycle();
    test_lookup();
    test_crud_and_json_roundtrip();
    test_keyboard_dispatcher_integration();
    test_arbitrary_key_remapping();
    printf("=== ALL KEYMAP & DISPATCHER TESTS PASSED SUCCESSFULLY! ===\n");
    return 0;
}
