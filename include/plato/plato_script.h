#ifndef PLATO_SCRIPT_H
#define PLATO_SCRIPT_H

#include "plato/plato_terminal.h"
#include <stdbool.h>

typedef struct plato_script_runner plato_script_runner_t;

plato_script_runner_t* plato_script_create(plato_terminal_t *term);
void plato_script_destroy(plato_script_runner_t *runner);
bool plato_script_start(plato_script_runner_t *runner, const char *script_text);
void plato_script_stop(plato_script_runner_t *runner);
bool plato_script_is_running(const plato_script_runner_t *runner);

#endif /* PLATO_SCRIPT_H */
