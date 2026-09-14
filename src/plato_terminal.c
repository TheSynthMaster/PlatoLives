#include "plato/plato_terminal.h"
#include <string.h>

static void terminal_ram_write_word(plato_terminal_t *term, uint16_t address, uint16_t value) {
    term->ram[address] = (uint8_t)(value & 0xFFu);
    term->ram[(uint16_t)(address + 1u)] = (uint8_t)(value >> 8);
}

void plato_terminal_init(plato_terminal_t *term) {
    if (!term) return;
    term->x = 0;
    term->y = 496;
    term->margin_x = 0;
    term->palette = PLATO_PALETTE_ORANGE_PLASMA;
    term->transport = NULL;
    term->beep_callback = NULL;
    term->beep_context = NULL;
    term->user_name[0] = '\0';
    term->user_group[0] = '\0';
    term->user_system[0] = '\0';
    term->user_station[0] = '\0';
    term->metadata_callback = NULL;
    term->metadata_context = NULL;
    memset(term->ram, 0, sizeof(term->ram));
    terminal_ram_write_word(term, PLATO_C2ORIGIN_ADDRESS, PLATO_DEFAULT_M2_ORIGIN);
    terminal_ram_write_word(term, PLATO_C3ORIGIN_ADDRESS, PLATO_DEFAULT_M3_ORIGIN);

    term->color_mode = false;
    term->delay_requested = false;
    plato_fb_init(&term->fb);
    plato_font_init(&term->font);
    plato_protocol_init(&term->decoder);
}

size_t plato_terminal_feed(plato_terminal_t *term, const uint8_t *data, size_t len) {
    if (!term || !data) return 0;
    size_t i = 0;
    for (; i < len; i++) {
        plato_protocol_process_byte(&term->decoder, term, data[i]);
        if (term->delay_requested) {
            i++; /* Consuma il byte 0x00 prima di cedere il passo */
            break;
        }
    }
    return i;
}

void plato_terminal_render_rgba(const plato_terminal_t *term, uint32_t *out_rgba) {
    if (!term || !out_rgba) return;
    plato_fb_to_rgba32(&term->fb, out_rgba, term->palette);
}

void plato_terminal_set_palette(plato_terminal_t *term, plato_palette_t pal) {
    if (term) term->palette = pal;
}

void plato_terminal_beep(plato_terminal_t *term) {
    if (term && term->beep_callback) term->beep_callback(term->beep_context);
}

void plato_terminal_set_color_mode(plato_terminal_t *term, bool enabled) {
    if (!term) return;
    term->color_mode = enabled;
    term->fb.color_enabled = enabled;
    if (enabled) {
        term->fb.fg_color = 0xFFFFFFFFu; /* Crisp white default for color mode */
        term->fb.bg_color = 0xFF000000u; /* Pure black background */
    } else {
        term->fb.fg_color = 0xFF006EFFu; /* Amber plasma */
        term->fb.bg_color = 0xFF00030Au;
    }
}
