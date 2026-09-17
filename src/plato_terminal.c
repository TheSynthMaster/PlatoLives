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
    plato_terminal_clear_text(term);
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

void plato_terminal_clear_text(plato_terminal_t *term) {
    if (!term) return;
    for (int r = 0; r < PLATO_ROWS; r++) {
        for (int c = 0; c < PLATO_COLS; c++) {
            term->text_grid[r][c].ch = ' ';
            term->text_grid[r][c].charset = (uint8_t)PLATO_CHARSET_M0;
        }
    }
}

void plato_terminal_put_char(plato_terminal_t *term, int x, int y, uint8_t ch,
                             plato_charset_t charset, plato_screen_mode_t mode, int size) {
    if (!term) return;
    int y_mod = ((y % PLATO_HEIGHT) + PLATO_HEIGHT) % PLATO_HEIGHT;
    int x_mod = ((x % PLATO_WIDTH) + PLATO_WIDTH) % PLATO_WIDTH;
    int row = 31 - (y_mod / PLATO_CHAR_HEIGHT);
    int col = x_mod / PLATO_CHAR_WIDTH;

    if (row < 0 || row >= PLATO_ROWS || col < 0 || col >= PLATO_COLS) return;

    if (mode == PLATO_SCREEN_ERASE) {
        term->text_grid[row][col].ch = ' ';
        term->text_grid[row][col].charset = (uint8_t)PLATO_CHARSET_M0;
    } else {
        term->text_grid[row][col].ch = ch;
        term->text_grid[row][col].charset = (uint8_t)charset;
    }
    (void)size;
}

size_t plato_terminal_get_text_area(const plato_terminal_t *term,
                                    int col0, int row0, int col1, int row1,
                                    bool compact,
                                    char *out_buf, size_t max_len) {
    if (!term || !out_buf || max_len == 0) return 0;

    int min_c = (col0 < col1) ? col0 : col1;
    int max_c = (col0 > col1) ? col0 : col1;
    int min_r = (row0 < row1) ? row0 : row1;
    int max_r = (row0 > row1) ? row0 : row1;

    if (min_c < 0) min_c = 0;
    if (max_c >= PLATO_COLS) max_c = PLATO_COLS - 1;
    if (min_r < 0) min_r = 0;
    if (max_r >= PLATO_ROWS) max_r = PLATO_ROWS - 1;

    size_t out_idx = 0;
    char line[PLATO_COLS + 1];
    char formatted[PLATO_COLS + 1];
    bool first_line_written = false;

    for (int r = min_r; r <= max_r; r++) {
        int line_len = 0;
        for (int c = min_c; c <= max_c; c++) {
            plato_text_cell_t cell = term->text_grid[r][c];
            /* Esclude esplicitamente i charset grafici M2 ed M3 */
            if (cell.charset == PLATO_CHARSET_M2 || cell.charset == PLATO_CHARSET_M3) {
                line[line_len++] = ' ';
            } else if (cell.ch >= 32 && cell.ch <= 126) {
                line[line_len++] = (char)cell.ch;
            } else {
                line[line_len++] = ' ';
            }
        }
        line[line_len] = '\0';

        if (!compact) {
            /* Modalità Verbatim: preserva le posizioni di colonna, rimuove gli spazi a fine riga */
            while (line_len > 0 && line[line_len - 1] == ' ') {
                line_len--;
            }
            if (r > min_r) {
                if (out_idx + 1 < max_len) out_buf[out_idx++] = '\n';
            }
            for (int i = 0; i < line_len; i++) {
                if (out_idx + 1 < max_len) out_buf[out_idx++] = line[i];
            }
        } else {
            /* Modalità Compact: trimma inizio/fine, collassa spazi consecutivi, scarta righe vuote */
            int start = 0;
            while (start < line_len && (line[start] == ' ' || line[start] == '\t')) {
                start++;
            }
            int end = line_len;
            while (end > start && (line[end - 1] == ' ' || line[end - 1] == '\t')) {
                end--;
            }

            if (start >= end) {
                /* Riga vuota: in modalità compact viene scartata */
                continue;
            }

            int fmt_len = 0;
            bool in_space = false;
            for (int i = start; i < end; i++) {
                char ch = line[i];
                if (ch == ' ' || ch == '\t') {
                    if (!in_space) {
                        formatted[fmt_len++] = ' ';
                        in_space = true;
                    }
                } else {
                    formatted[fmt_len++] = ch;
                    in_space = false;
                }
            }

            if (first_line_written) {
                if (out_idx + 1 < max_len) out_buf[out_idx++] = '\n';
            }
            for (int i = 0; i < fmt_len; i++) {
                if (out_idx + 1 < max_len) out_buf[out_idx++] = formatted[i];
            }
            first_line_written = true;
        }
    }

    out_buf[out_idx] = '\0';
    return out_idx;
}
