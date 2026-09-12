#ifndef PLATO_TYPES_H
#define PLATO_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PLATO_WIDTH         512
#define PLATO_HEIGHT        512
#define PLATO_CHAR_WIDTH    8
#define PLATO_CHAR_HEIGHT   16
#define PLATO_COLS          (PLATO_WIDTH / PLATO_CHAR_WIDTH)   /* 64 */
#define PLATO_ROWS          (PLATO_HEIGHT / PLATO_CHAR_HEIGHT) /* 32 */

typedef enum {
    PLATO_SCREEN_WRITE   = 0,  /* OR */
    PLATO_SCREEN_ERASE   = 1,  /* AND NOT */
    PLATO_SCREEN_REWRITE = 2,  /* COPY */
    PLATO_SCREEN_INVERSE = 3   /* NOT COPY */
} plato_screen_mode_t;

typedef enum {
    PLATO_MODE_POINT = 0,
    PLATO_MODE_LINE  = 1,
    PLATO_MODE_MEM   = 2,
    PLATO_MODE_ALPHA = 3,
    PLATO_MODE_BLOCK = 4
} plato_data_mode_t;

typedef enum {
    PLATO_CHARSET_M0 = 0,
    PLATO_CHARSET_M1 = 1,
    PLATO_CHARSET_M2 = 2,
    PLATO_CHARSET_M3 = 3
} plato_charset_t;

typedef enum {
    PLATO_PALETTE_ORANGE_PLASMA = 0,
    PLATO_PALETTE_GREEN_P31     = 1,
    PLATO_PALETTE_AMBER_P40     = 2,
    PLATO_PALETTE_WHITE_CRT     = 3
} plato_palette_t;

typedef enum {
    PLATO_KEY_NONE = 0,
    PLATO_KEY_NEXT,
    PLATO_KEY_BACK,
    PLATO_KEY_HELP,
    PLATO_KEY_LAB,
    PLATO_KEY_DATA,
    PLATO_KEY_STOP,
    PLATO_KEY_EDIT,
    PLATO_KEY_COPY,
    PLATO_KEY_MICRO,
    PLATO_KEY_FONT,
    PLATO_KEY_SUPER,
    PLATO_KEY_SUB,
    PLATO_KEY_ACCESS,
    PLATO_KEY_ERASE,
    PLATO_KEY_ANS,
    PLATO_KEY_TERM,
    PLATO_KEY_SQUARE
} plato_special_key_t;

#endif /* PLATO_TYPES_H */
