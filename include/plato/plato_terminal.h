#ifndef PLATO_TERMINAL_H
#define PLATO_TERMINAL_H

#include "plato_types.h"
#include "plato_framebuffer.h"
#include "plato_font.h"
#include "plato_protocol.h"
#include "plato_keyboard.h"

#define PLATO_TERMINAL_RAM_SIZE 65536u
#define PLATO_C2ORIGIN_ADDRESS 0x2306u
#define PLATO_C3ORIGIN_ADDRESS 0x2308u
#define PLATO_DEFAULT_M2_ORIGIN 0x2340u
#define PLATO_DEFAULT_M3_ORIGIN 0x2740u

struct plato_transport;

typedef void (*plato_beep_callback_t)(void *context);
typedef void (*plato_metadata_callback_t)(void *context, const char *name, const char *group, const char *system, const char *station);

typedef struct {
    uint8_t ch;
    uint8_t charset; /* plato_charset_t: M0, M1, M2, M3 */
} plato_text_cell_t;

struct plato_terminal {
    int x;
    int y;
    int margin_x;
    plato_framebuffer_t fb;
    plato_font_t font;
    plato_protocol_decoder_t decoder;
    plato_palette_t palette;
    uint8_t ram[PLATO_TERMINAL_RAM_SIZE];
    struct plato_transport *transport;
    plato_beep_callback_t beep_callback;
    void *beep_context;
    char user_name[32];
    char user_group[32];
    char user_system[32];
    char user_station[32];
    plato_metadata_callback_t metadata_callback;
    void *metadata_context;
    bool color_mode;
    bool delay_requested;
    plato_text_cell_t text_grid[PLATO_ROWS][PLATO_COLS];
};

void plato_terminal_set_color_mode(plato_terminal_t *term, bool enabled);

void plato_terminal_init(plato_terminal_t *term);
size_t plato_terminal_feed(plato_terminal_t *term, const uint8_t *data, size_t len);
void plato_terminal_render_rgba(const plato_terminal_t *term, uint32_t *out_rgba);
void plato_terminal_set_palette(plato_terminal_t *term, plato_palette_t pal);
void plato_terminal_beep(plato_terminal_t *term);

void plato_terminal_clear_text(plato_terminal_t *term);
void plato_terminal_put_char(plato_terminal_t *term, int x, int y, uint8_t ch,
                             plato_charset_t charset, plato_screen_mode_t mode, int size);
size_t plato_terminal_get_text_area(const plato_terminal_t *term,
                                    int col0, int row0, int col1, int row1,
                                    bool compact,
                                    char *out_buf, size_t max_len);

#endif /* PLATO_TERMINAL_H */
