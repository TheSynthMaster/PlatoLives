#ifndef PLATO_OPTICAL_H
#define PLATO_OPTICAL_H

#include "plato_terminal.h"
#include "plato_profile.h"
#include <stdbool.h>

#define PLATO_OPTICAL_SCALE 4
#define PLATO_OPTICAL_WIDTH (PLATO_WIDTH * PLATO_OPTICAL_SCALE)
#define PLATO_OPTICAL_HEIGHT (PLATO_HEIGHT * PLATO_OPTICAL_SCALE)

typedef struct plato_optical plato_optical_t;

plato_optical_t* plato_optical_create(void);
void plato_optical_destroy(plato_optical_t *opt);
void plato_optical_invalidate(plato_optical_t *opt);

bool plato_optical_is_full_frame(const plato_optical_t *opt);

bool plato_optical_render(plato_optical_t *opt,
                          plato_terminal_t *term,
                          const plato_profile_t *profile,
                          double now_sec,
                          uint32_t *out_bgra);

void plato_optical_unwarp_touch(int *x, int *y, int display_mode, int plasma_distortion, int crt_distortion);

#endif /* PLATO_OPTICAL_H */
