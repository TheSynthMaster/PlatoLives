#include "plato/plato_terminal.h"
#include <string.h>

static void terminal_ram_write_word(plato_terminal_t *term, uint16_t address, uint16_t value) {
    term->ram[address] = (uint8_t)(value & 0xFFu);
    term->ram[(uint16_t)(address + 1u)] = (uint8_t)(value >> 8);
}

void plato_terminal_reset(plato_terminal_t *term) {
    if (!term) return;

    term->x = 0;
    term->y = 496;
    term->margin_x = 0;
    term->delay_requested = false;

    memset(term->ram, 0, sizeof(term->ram));
    terminal_ram_write_word(term, PLATO_C2ORIGIN_ADDRESS, PLATO_DEFAULT_M2_ORIGIN);
    terminal_ram_write_word(term, PLATO_C3ORIGIN_ADDRESS, PLATO_DEFAULT_M3_ORIGIN);

    plato_font_init(&term->font);
    plato_protocol_init(&term->decoder);
    plato_microtutor_init(&term->mt, term);
    plato_fb_clear(&term->fb);
    plato_terminal_clear_text(term);
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
    plato_microtutor_init(&term->mt, term);
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
        term->fb.fg_color = 0xFFFF6E00u; /* Amber plasma */
        term->fb.bg_color = 0xFF0A0300u;
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

static const char* classify_glyph_bitmap(const uint8_t bitmap[16]) {
    bool any_bit = false;
    int total_bits = 0;
    int max_row = 0;
    int col_counts[8] = {0};

    for (int r = 0; r < 16; r++) {
        uint8_t b = bitmap[r];
        if (b != 0) any_bit = true;
        int row_w = 0;
        for (int c = 0; c < 8; c++) {
            if (b & (0x80 >> c)) {
                row_w++;
                col_counts[c]++;
                total_bits++;
            }
        }
        if (row_w > max_row) max_row = row_w;
    }

    if (!any_bit) return " ";
    if (total_bits >= 45) return "█";

    int max_col = 0;
    for (int c = 0; c < 8; c++) {
        if (col_counts[c] > max_col) max_col = col_counts[c];
    }

    bool has_top = false, has_bottom = false, has_left = false, has_right = false;
    for (int r = 0; r <= 6; r++) {
        if (bitmap[r] & 0x3C) has_top = true;
    }
    for (int r = 9; r <= 15; r++) {
        if (bitmap[r] & 0x3C) has_bottom = true;
    }
    for (int r = 5; r <= 11; r++) {
        if (bitmap[r] & 0xE0) has_left = true;
        if (bitmap[r] & 0x07) has_right = true;
    }

    if (has_top && has_bottom && has_left && has_right) return "┼";
    if (has_top && has_bottom && has_right && !has_left) return "├";
    if (has_top && has_bottom && has_left && !has_right) return "┤";
    if (has_bottom && has_left && has_right && !has_top) return "┬";
    if (has_top && has_left && has_right && !has_bottom) return "┴";
    if (has_bottom && has_right && !has_top && !has_left) return "┌";
    if (has_bottom && has_left && !has_top && !has_right) return "┐";
    if (has_top && has_right && !has_bottom && !has_left) return "└";
    if (has_top && has_left && !has_bottom && !has_right) return "┘";

    if (max_row >= 5) return "─";
    if (max_col >= 6) return "│";
    if (total_bits <= 4) return "·";

    return "░";
}

const char* plato_cell_to_utf8(const plato_terminal_t *term, plato_text_cell_t cell, char tmp_buf[8]) {
    uint8_t ch = cell.ch & 0x7F;

    if (cell.charset == PLATO_CHARSET_M0) {
        if (ch >= 32 && ch <= 126) {
            tmp_buf[0] = (char)ch;
            tmp_buf[1] = '\0';
            return tmp_buf;
        }
        return " ";
    }

    if (cell.charset == PLATO_CHARSET_M1) {
        switch (ch) {
            case 0x25: return "≠";
            case 0x26: return "↑";
            case 0x27: return "→";
            case 0x28: return "↓";
            case 0x29: return "←";
            case 0x2A: return "Σ";
            case 0x2B: return "Δ";
            case 0x30: return "β";
            case 0x38: return "≤";
            case 0x39: return "≥";
            case 0x3A: return "Θ";
            case 0x3F: return "═";
            case 0x43: return "◆";
            case 0x61: return "α";
            case 0x62: return "β";
            case 0x64: return "δ";
            case 0x65: return "ε";
            case 0x67: return "γ";
            case 0x69: return "≤";
            case 0x6A: return "≥";
            case 0x6B: return "θ";
            case 0x6D: return "μ";
            case 0x6E: return "ν";
            case 0x6F: return "═";
            case 0x70: return "π";
            case 0x71: return "◆";
            case 0x72: return "×";
            case 0x73: return "÷";
            case 0x74: return "±";
            case 0x75: return "○";
            case 0x77: return "│";
            case 0x78: return "≈";
            default:
                if (ch >= 32 && ch <= 126) {
                    tmp_buf[0] = (char)ch;
                    tmp_buf[1] = '\0';
                    return tmp_buf;
                }
                return " ";
        }
    }

    if ((cell.charset == PLATO_CHARSET_M2 || cell.charset == PLATO_CHARSET_M3) && term) {
        const uint8_t *bm = (cell.charset == PLATO_CHARSET_M2) ? term->font.m2[ch] : term->font.m3[ch];
        return classify_glyph_bitmap(bm);
    }

    return " ";
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
    char line[PLATO_COLS * 4 + 1];
    char formatted[PLATO_COLS * 4 + 1];
    char tmp[8];
    bool first_line_written = false;

    for (int r = min_r; r <= max_r; r++) {
        size_t line_len = 0;
        for (int c = min_c; c <= max_c; c++) {
            plato_text_cell_t cell = term->text_grid[r][c];
            const char *utf = plato_cell_to_utf8(term, cell, tmp);
            size_t ulen = strlen(utf);
            if (line_len + ulen < sizeof(line) - 1) {
                memcpy(&line[line_len], utf, ulen);
                line_len += ulen;
            }
        }
        line[line_len] = '\0';

        if (!compact) {
            while (line_len > 0 && line[line_len - 1] == ' ') {
                line_len--;
            }
            if (r > min_r) {
                if (out_idx + 1 < max_len) out_buf[out_idx++] = '\n';
            }
            for (size_t i = 0; i < line_len; i++) {
                if (out_idx + 1 < max_len) out_buf[out_idx++] = line[i];
            }
        } else {
            size_t start = 0;
            while (start < line_len && (line[start] == ' ' || line[start] == '\t')) {
                start++;
            }
            size_t end = line_len;
            while (end > start && (line[end - 1] == ' ' || line[end - 1] == '\t')) {
                end--;
            }

            if (start >= end) {
                continue;
            }

            size_t fmt_len = 0;
            bool in_space = false;
            for (size_t i = start; i < end; i++) {
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
            for (size_t i = 0; i < fmt_len; i++) {
                if (out_idx + 1 < max_len) out_buf[out_idx++] = formatted[i];
            }
            first_line_written = true;
        }
    }

    out_buf[out_idx] = '\0';
    return out_idx;
}

void plato_terminal_tick(plato_terminal_t *term, int ms) {
    if (!term || ms <= 0) return;

    /* 1. M_CLOCK timer indipendente a 17 ms (RAM 0x22EC), come m_Mclock in pterm */
    term->mclock_acc_ms += ms;
    while (term->mclock_acc_ms >= 17) {
        term->mclock_acc_ms -= 17;
        uint16_t m_clock = (uint16_t)term->ram[0x22EC] | ((uint16_t)term->ram[0x22ED] << 8);
        m_clock++;
        term->ram[0x22EC] = (uint8_t)(m_clock & 0xFF);
        term->ram[0x22ED] = (uint8_t)((m_clock >> 8) & 0xFF);
    }

    /* 2. Gestione ripresa da yield (R_EXEC / R_WAIT16) e background execution come in pterm */
    if (term->mt.in_r_exec) {
        term->mt.r_exec_wait_ms -= ms;
        if (term->mt.r_exec_wait_ms <= ms) {
            term->mt.in_r_exec = false;
            term->mt.r_exec_wait_ms = 0;
            plato_microtutor_emulate(&term->mt);
        }
    } else if (term->mt.running) {
        plato_microtutor_emulate(&term->mt);
    }
}
