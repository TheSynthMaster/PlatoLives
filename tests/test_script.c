#include "plato/plato_script.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

static void test_script_store(void) {
    printf("[TEST] Testing Script Store persistence and clone...\n");
    plato_script_list_t *list = plato_scripts_create();
    assert(list != NULL);

    plato_script_t s1 = {0};
    snprintf(s1.name, sizeof(s1.name), "Auto Cyber1");
    s1.enabled = true;
    s1.char_delay_ms = 180;
    s1.next_delay_ms = 450;
    s1.command_delay_ms = 25;
    snprintf(s1.body, sizeof(s1.body), "let count = 0\nfor i = 1 to 3\ncount++\nnext i\n");

    int idx = plato_scripts_add(list, &s1); (void)idx;
    assert(idx == 0);
    assert(list->count == 1);

    int c_idx = plato_scripts_clone(list, 0); (void)c_idx;
    assert(c_idx == 1);
    assert(list->count == 2);
    assert(strcmp(list->scripts[1].name, "Auto Cyber1 (Copy)") == 0);
    assert(list->scripts[1].char_delay_ms == 180);

    const char *tmp_ini = "/tmp/test_platolives_scripts.ini";
    bool saved = plato_scripts_save(list, tmp_ini); (void)saved;
    assert(saved);

    plato_script_list_t *loaded = plato_scripts_create();
    bool ok = plato_scripts_load(loaded, tmp_ini); (void)ok;
    assert(ok);
    assert(loaded->count == 2);
    assert(loaded->scripts[0].char_delay_ms == 180);
    assert(loaded->scripts[0].next_delay_ms == 450);
    assert(loaded->scripts[0].command_delay_ms == 25);

    plato_scripts_free(list);
    plato_scripts_free(loaded);
    remove(tmp_ini);
    printf("       [PASS] Script Store OK\n");
}

static void test_script_runner(void) {
    printf("[TEST] Testing Script Runner (Basic Loop, Variables, i++)...\n");
    plato_script_runner_t *runner = plato_script_create(NULL);
    assert(runner != NULL);

    plato_script_t script = {0};
    snprintf(script.name, sizeof(script.name), "TestBasic");
    script.char_delay_ms = 0;
    script.next_delay_ms = 0;
    script.command_delay_ms = 0;
    snprintf(script.body, sizeof(script.body),
             "let total = 0\n"
             "for i = 1 to 5\n"
             "total++\n"
             "next i\n"
             "exit\n");

    bool started = plato_script_start_ex(runner, &script); (void)started;
    assert(started);

    while (plato_script_is_running(runner)) {
        struct timespec ts = { 0, 10000000L };
        nanosleep(&ts, NULL);
    }

    bool found = false;
    int64_t total = plato_script_get_var(runner, "total", &found);
    assert(found && total == 5);

    plato_script_destroy(runner);
    printf("       [PASS] Script Runner OK (total=%lld)\n", (long long)total);
}

int main(void) {
    printf("========================================\n");
    printf(" RUNNING PLATO SCRIPT ENGINE UNIT TESTS \n");
    printf("========================================\n");
    test_script_store();
    test_script_runner();
    printf("========================================\n");
    printf(" ALL PLATO SCRIPT TESTS PASSED [100%%]   \n");
    printf("========================================\n");
    return 0;
}
