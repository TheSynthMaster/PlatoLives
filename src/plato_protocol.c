#include <string.h>
#include <stdio.h>
#include "plato/plato_protocol.h"
#include "plato/plato_terminal.h"
#include "plato/plato_transport.h"
#include "plato/plato_graphics.h"

static const uint8_t PTAT0[128] = {
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x26, 0x60, 0x0A, 0x5E, 0x2B, 0x2D,
    0x13, 0x04, 0x07, 0x08, 0x7B, 0x0B, 0x0D, 0x1A,
    0x02, 0x12, 0x01, 0x03, 0x7D, 0x0C, 0x83, 0x85,
    0x3C, 0x3E, 0x5B, 0x5D, 0x24, 0x25, 0x5F, 0x7C,
    0x2A, 0x28, 0x40, 0x27, 0x1C, 0x5C, 0x23, 0x7E,
    0x17, 0x05, 0x14, 0x19, 0x7F, 0x09, 0x1E, 0x18,
    0x0E, 0x1D, 0x11, 0x16, 0x00, 0x0F, 0x87, 0x88,
    0x20, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x78, 0x79, 0x7A, 0x3D, 0x3B, 0x2F, 0x2E, 0x2C,
    0x1F, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
    0x58, 0x59, 0x5A, 0x29, 0x3A, 0x3F, 0x21, 0x22
};

static const uint8_t PTAT1[128] = {
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x26, 0x60, 0x09, 0x5E, 0x2B, 0x2D,
    0x17, 0x04, 0x07, 0x08, 0x7B, 0x0B, 0x0D, 0x1A,
    0x02, 0x12, 0x01, 0x03, 0x7D, 0x0C, 0x83, 0x85,
    0x3C, 0x3E, 0x5B, 0x5D, 0x24, 0x25, 0x5F, 0x27,
    0x2A, 0x28, 0x40, 0x7C, 0x1C, 0x5C, 0x23, 0x7E,
    0x97, 0x84, 0x14, 0x19, 0x7F, 0x0A, 0x1E, 0x18,
    0x0E, 0x1D, 0x05, 0x16, 0x9D, 0x0F, 0x87, 0x88,
    0x20, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x78, 0x79, 0x7A, 0x3D, 0x3B, 0x2F, 0x2E, 0x2C,
    0x1F, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
    0x58, 0x59, 0x5A, 0x29, 0x3A, 0x3F, 0x21, 0x22
};

static void send_echo_key(plato_terminal_t *term, uint16_t key) { plato_protocol_send_key(term, 0x080 | (key & 0x7F)); }

static void parse_pmd_item(const char *str, const char *key, char *out, size_t out_max) {
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "%s=", key);
    const char *p = strstr(str, pattern);
    if (!p) return;
    p += strlen(pattern);
    size_t idx = 0;
    while (*p && *p != ';' && idx < out_max - 1) {
        out[idx++] = *p++;
    }
    out[idx] = '\0';
}

static void plato_process_pmd(plato_terminal_t *term, const char *pmd, int len) {
    if (!term || !pmd || len <= 0) return;
    parse_pmd_item(pmd, "name", term->user_name, sizeof(term->user_name));
    parse_pmd_item(pmd, "group", term->user_group, sizeof(term->user_group));
    parse_pmd_item(pmd, "system", term->user_system, sizeof(term->user_system));
    parse_pmd_item(pmd, "station", term->user_station, sizeof(term->user_station));

    fprintf(stderr, "[PLATO METADATA] User: %s, Group: %s, System: %s, Slot: %s\n",
            term->user_name, term->user_group, term->user_system, term->user_station);

    if (term->metadata_callback) {
        term->metadata_callback(term->metadata_context,
                               term->user_name,
                               term->user_group,
                               term->user_system,
                               term->user_station);
    }
}

void plato_protocol_init(plato_protocol_decoder_t *dec) {
    if (!dec) return;
    dec->state = STATE_NORMAL; dec->plato_mode = true; dec->flow_control = false;
    dec->data_mode = PLATO_MODE_ALPHA; dec->screen_mode = PLATO_SCREEN_REWRITE;
    dec->charset = PLATO_CHARSET_M0; dec->char_size = 0;
    dec->cur_hi_y = dec->cur_lo_y = dec->cur_hi_x = dec->cur_lo_x = 0;
    dec->got_lo_y = false; dec->first_line_coord = true; dec->first_block_coord = true;
    dec->block_x0 = dec->block_y0 = 0; dec->word_idx = 0; dec->skip_bytes_remaining = 0;
    dec->last_raw_byte = 0;
    dec->mode7_active = false;
    dec->mode2_active = false;
    dec->word_param_command = 0;
    dec->load_address = 0;
    dec->pmd_len = 0;
    dec->pmd_buf[0] = '\0';
    dec->color_cmd = 0;
    dec->color_idx = 0;
}

uint16_t plato_protocol_keycode_for_ascii(uint8_t ascii) {
    for (uint16_t key = 0; key < 128; key++) if (PTAT0[key] == ascii) return key;
    return UINT16_MAX;
}

void plato_protocol_send_key(plato_terminal_t *term, uint16_t key) {
    if (!term || !term->transport || !term->transport->connected) return;
    if (key >> 7) {
        uint8_t out[3] = { 0x1B, 0x40 | (key & 0x3F), 0x60 | ((key >> 6) & 0x0F) };
        plato_transport_send(term->transport, out, 3);
    } else {
        uint8_t translated = term->decoder.flow_control ? PTAT1[key] : PTAT0[key];
        if (translated & 0x80) {
            uint8_t out[2] = { 0x1B, translated & 0x7F };
            plato_transport_send(term->transport, out, 2);
        } else plato_transport_send(term->transport, &translated, 1);
    }
}

void plato_protocol_send_touch(plato_terminal_t *term, int16_t x, int16_t y) {
    if (!term || !term->transport || !term->transport->connected) return;
    if (x < 0) x = 0; if (x >= 512) x = 511; if (y < 0) y = 0; if (y >= 512) y = 511;
    uint8_t fgt[6] = { 0x1B, 0x1F, 0x40 + (uint8_t)(x & 0x1F), 0x40 + (uint8_t)((x >> 5) & 0x0F),
                       0x40 + (uint8_t)(y & 0x1F), 0x40 + (uint8_t)((y >> 5) & 0x0F) };
    plato_transport_send(term->transport, fgt, 6);
    plato_protocol_send_key(term, 0x100 | ((x >> 1) & 0xF0) | ((y >> 5) & 0x0F));
}

static void coordinate_complete(plato_protocol_decoder_t *dec, plato_terminal_t *term, int x, int y) {
    if (dec->state == STATE_LOAD_COORD) {
        term->x = x; term->y = y; dec->state = STATE_NORMAL; dec->first_line_coord = false;
    } else if (dec->data_mode == PLATO_MODE_POINT) {
        plato_draw_point(&term->fb, x, y, dec->screen_mode); term->x = x; term->y = y;
    } else if (dec->data_mode == PLATO_MODE_LINE) {
        if (dec->first_line_coord) { term->x = x; term->y = y; dec->first_line_coord = false; }
        else {
            plato_draw_line(&term->fb, term->x, term->y, x, y, dec->screen_mode);
            plato_transport_log_msg(term->transport, "[LINE] (%d,%d)->(%d,%d) mode=%d fg=0x%08X\n",
                                    term->x, term->y, x, y, dec->screen_mode, term->fb.fg_color);
            term->x = x; term->y = y;
        }
    } else if (dec->data_mode == PLATO_MODE_BLOCK) {
        if (dec->first_block_coord) { dec->block_x0 = x; dec->block_y0 = y; dec->first_block_coord = false; }
        else {
            plato_draw_block(&term->fb, dec->block_x0, dec->block_y0, x, y, dec->screen_mode);
            plato_transport_log_msg(term->transport, "[BLOCK] (%d,%d)->(%d,%d) mode=%d fg=0x%08X bg=0x%08X\n",
                                    dec->block_x0, dec->block_y0, x, y, dec->screen_mode, term->fb.fg_color, term->fb.bg_color);
            term->x = x; term->y = y; dec->first_block_coord = true;
        }
    }
}

static bool coordinate_byte(plato_protocol_decoder_t *dec, plato_terminal_t *term, uint8_t ch) {
    uint8_t tag = (ch >> 5) & 0x03, val = ch & 0x1F;
    if (tag == 1) { if (!dec->got_lo_y) dec->cur_hi_y = val; else dec->cur_hi_x = val; return true; }
    if (tag == 3) { dec->cur_lo_y = val; dec->got_lo_y = true; return true; }
    if (tag == 2) {
        dec->cur_lo_x = val;
        int x = (dec->cur_hi_x << 5) | dec->cur_lo_x, y = (dec->cur_hi_y << 5) | dec->cur_lo_y;
        dec->got_lo_y = false; coordinate_complete(dec, term, x, y); return true;
    }
    return false;
}

static void echo_complete(plato_protocol_decoder_t *dec, plato_terminal_t *term) {
    uint16_t code = (uint16_t)(dec->word_buf[0] & 0x3F) |
                    ((uint16_t)(dec->word_buf[1] & 0x3F) << 6) |
                    ((uint16_t)(dec->word_buf[2] & 0x3F) << 12);
    code &= 0x7F;
    switch (code) {
        case 0x52: dec->flow_control = true; send_echo_key(term, 0x53); break;
        case 0x60: send_echo_key(term, 0x01); break;
        case 0x70: send_echo_key(term, 12); break;
        case 0x71: 
            plato_transport_log_msg(term->transport, "[ECHO 0x71] Subtype Query -> reply %d (color=%d)\n",
                                    term->color_mode ? 2 : 1, term->color_mode);
            send_echo_key(term, term->color_mode ? 2 : 1); 
            break;
        case 0x72: send_echo_key(term, 0); break;
        case 0x73: 
            plato_transport_log_msg(term->transport, "[ECHO 0x73] Config Query -> reply 0x%02X (color=%d)\n",
                                    term->color_mode ? 0x70 : 0x40, term->color_mode);
            send_echo_key(term, term->color_mode ? 0x70 : 0x40); 
            break;
        case 0x7A: plato_protocol_send_key(term, 0x3FF); break;
        case 0x7B: plato_terminal_beep(term); break;
        case 0x7D: send_echo_key(term, 0x7F); break;
        default: send_echo_key(term, code); break;
    }
}

static uint32_t decode_word18(const uint8_t word_buf[3]) {
    return (uint32_t)(word_buf[0] & 0x3F) |
           ((uint32_t)(word_buf[1] & 0x3F) << 6) |
           ((uint32_t)(word_buf[2] & 0x3F) << 12);
}

static uint16_t terminal_ram_read_word(const plato_terminal_t *term, uint16_t address) {
    return (uint16_t)term->ram[address] |
           ((uint16_t)term->ram[(uint16_t)(address + 1u)] << 8);
}

static void terminal_ram_write_word(plato_terminal_t *term, uint16_t address, uint16_t value) {
    term->ram[address] = (uint8_t)(value & 0xFFu);
    term->ram[(uint16_t)(address + 1u)] = (uint8_t)(value >> 8);
}

static void rebuild_programmable_glyph(plato_terminal_t *term, plato_charset_t charset,
                                       uint8_t code, uint16_t glyph_address) {
    uint8_t bitmap[16] = {0};
    for (int column = 0; column < 8; column++) {
        uint16_t column_word = terminal_ram_read_word(term, (uint16_t)(glyph_address + column * 2));
        for (int source_bit = 0; source_bit < 16; source_bit++) {
            if (column_word & ((uint16_t)1u << source_bit)) bitmap[15 - source_bit] |= (uint8_t)(0x80u >> column);
        }
    }
    plato_font_load_glyph(&term->font, charset, code, bitmap);
}

static void mode2_word_complete(plato_protocol_decoder_t *dec, plato_terminal_t *term, uint32_t word) {
    uint16_t address = (uint16_t)dec->load_address;
    terminal_ram_write_word(term, address, (uint16_t)word);

    /* I due bit alti della Word Mode 2 specificano l operazione; 0 significa load data. */
    if (((word >> 16) & 0x03u) == 0) {
        uint16_t c2origin = terminal_ram_read_word(term, PLATO_C2ORIGIN_ADDRESS);
        uint16_t c3origin = terminal_ram_read_word(term, PLATO_C3ORIGIN_ADDRESS);
        uint16_t offset;
        plato_charset_t charset;
        uint16_t origin;

        if ((uint16_t)(address - c2origin) < 0x0400u) {
            offset = (uint16_t)(address - c2origin); charset = PLATO_CHARSET_M2; origin = c2origin;
        } else if ((uint16_t)(address - c3origin) < 0x0400u) {
            offset = (uint16_t)(address - c3origin); charset = PLATO_CHARSET_M3; origin = c3origin;
        } else {
            dec->load_address = (uint16_t)(address + 2u);
            return;
        }

        uint8_t glyph = (uint8_t)(offset >> 4);
        uint16_t glyph_address = (uint16_t)(origin + ((uint16_t)glyph << 4));
        rebuild_programmable_glyph(term, charset, (uint8_t)(0x20u + glyph), glyph_address);
    }

    dec->load_address = (uint16_t)(address + 2u);
}

void plato_protocol_process_byte(plato_protocol_decoder_t *dec, plato_terminal_t *term, uint8_t byte) {
    if (!dec || !term) return;
    /* TELNET codifica un byte dati 0xFF come FF FF: il secondo va scartato. */
    if (dec->last_raw_byte == 0xFF && byte == 0xFF) {
        dec->last_raw_byte = 0;
        return;
    }
    dec->last_raw_byte = byte;
    byte &= 0x7F;
    if (dec->state == STATE_PMD) {
        if (byte < 0x20 || (byte & 0x3F) == 0 || dec->pmd_len >= 1000) {
            dec->state = STATE_NORMAL;
            dec->pmd_buf[dec->pmd_len] = '\0';
            plato_process_pmd(term, dec->pmd_buf, dec->pmd_len);
            dec->pmd_len = 0;
            if (byte < 0x20) {
                plato_protocol_process_byte(dec, term, byte);
            }
            return;
        }
        uint8_t d = byte & 0x3F; {
            char c = 0;
            if (d >= 1 && d <= 26) c = 'a' + (d - 1);
            else if (d >= 27 && d <= 36) c = '0' + (d - 27);
            else if (d == 38) c = '-';
            else if (d == 40) c = '/';
            else if (d == 44) c = '=';
            else if (d == 45) c = ' ';
            else if (d == 63) c = ';';

            if (c != 0 && dec->pmd_len < (int)sizeof(dec->pmd_buf) - 1) {
                dec->pmd_buf[dec->pmd_len++] = c;
            }
        }
        return;
    }

    if (dec->state == STATE_WORD_PARAM) {
        dec->word_buf[dec->word_idx++] = byte;
        if (dec->word_idx >= 3) {
            uint32_t word = decode_word18(dec->word_buf);
            dec->word_idx = 0;
            dec->state = STATE_NORMAL;
            if (dec->word_param_command == 'W') dec->load_address = word;
            dec->word_param_command = 0;
        }
        return;
    }
    if (dec->state == STATE_ECHO_WORD) {
        dec->word_buf[dec->word_idx++] = byte;
        if (dec->word_idx >= 3) { dec->word_idx = 0; dec->state = STATE_NORMAL; echo_complete(dec, term); }
        return;
    }
    if (dec->state == STATE_COLOR_PARAM) {
        dec->color_buf[dec->color_idx++] = byte;
        if (dec->color_idx >= 4) {
            uint32_t b0 = dec->color_buf[0] & 0x3Fu;
            uint32_t b1 = dec->color_buf[1] & 0x3Fu;
            uint32_t b2 = dec->color_buf[2] & 0x3Fu;
            uint32_t b3 = dec->color_buf[3] & 0x3Fu;
            uint32_t val24 = b0 | (b1 << 6) | (b2 << 12) | (b3 << 18);

            /* CDC s0ascers 3.2.3.1.8: bits 1..8: Blue, 9..16: Green, 17..24: Red */
            uint32_t blue  = val24 & 0xFFu;
            uint32_t green = (val24 >> 8) & 0xFFu;
            uint32_t red   = (val24 >> 16) & 0xFFu;
            uint32_t bgra  = (0xFFu << 24) | (red << 16) | (green << 8) | blue;

            plato_transport_log_msg(term->transport, "[COLOR ESC %c] raw:%02X %02X %02X %02X -> R:%u G:%u B:%u (bgra:0x%08X)\n",
                                    dec->color_cmd, dec->color_buf[0], dec->color_buf[1], dec->color_buf[2], dec->color_buf[3],
                                    red, green, blue, bgra);

            if (dec->color_cmd == 'a') {
                term->fb.fg_color = bgra;
            } else if (dec->color_cmd == 'b') {
                term->fb.bg_color = bgra;
            }
            dec->state = STATE_NORMAL;
        }
        return;
    }

    if (dec->state == STATE_SKIP_PARAM) {
        if (--dec->skip_bytes_remaining <= 0) dec->state = STATE_NORMAL;
        return;
    }
    if (dec->state == STATE_ESCAPE) {
        dec->state = STATE_NORMAL;
        if (byte != 'a' && byte != 'b') {
            printf("[PROTOCOL ESC 0x%02X (%c)]\n", byte, (byte >= 32 && byte <= 126) ? byte : '?');
        }
        switch (byte) {
            case 0x02:
                dec->plato_mode = true; dec->data_mode = PLATO_MODE_ALPHA; dec->screen_mode = PLATO_SCREEN_REWRITE;
                dec->mode7_active = false; dec->mode2_active = false;
                dec->word_idx = 0; dec->word_param_command = 0;
                break;
            case 0x03: break;
            case 0x0C:
                plato_transport_log_msg(term->transport, "[CLEAR SCREEN ESC 0x0C] bg=0x%08X color=%d\n", term->fb.bg_color, term->fb.color_enabled);
                plato_fb_clear(&term->fb); term->x = 0; term->y = 496; term->margin_x = 0;
                dec->char_size = 0;
                dec->screen_mode = PLATO_SCREEN_REWRITE; dec->data_mode = PLATO_MODE_ALPHA;
                dec->mode7_active = false; dec->mode2_active = false;
                dec->word_idx = 0; dec->word_param_command = 0;
                break;
            case 0x11: dec->screen_mode = PLATO_SCREEN_INVERSE; break;
            case 0x12: dec->screen_mode = PLATO_SCREEN_WRITE; break;
            case 0x13: dec->screen_mode = PLATO_SCREEN_ERASE; break;
            case 0x14: dec->screen_mode = PLATO_SCREEN_REWRITE; break;
            case '2': dec->state = STATE_LOAD_COORD; break;
            case 'P': case 'S':
                /* entrambi selezionano Mode 2, che scrive Word nella RAM terminale. */
                dec->mode7_active = false; dec->mode2_active = true; dec->word_idx = 0;
                break;
            case 'Q': case 'R': case 'W':
                /* Comandi Word transitori; soltanto W modifica il memory address register. */
                dec->state = STATE_WORD_PARAM; dec->word_idx = 0; dec->word_param_command = byte;
                break;
            case 'X':
                dec->state = STATE_PMD;
                dec->pmd_len = 0;
                dec->pmd_buf[0] = '\0';
                break;
            case 'V':
                /* SetMode(mMode7, tWord): persiste fino a un vero cambio di modalita. */
                dec->mode2_active = false; dec->mode7_active = true; dec->word_idx = 0;
                break;
            case 'Y': dec->state = STATE_ECHO_WORD; dec->word_idx = 0; break;
            case 'a': case 'b': dec->state = STATE_COLOR_PARAM; dec->color_cmd = byte; dec->color_idx = 0; break;
			case 'c':
			    /* Paint: parametro transitorio di 12 bit composto da due byte a 6 bit. */
			    dec->state = STATE_SKIP_PARAM;
			    dec->skip_bytes_remaining = 2;
			    break;
            case '@': term->y = term->y + 5 < PLATO_HEIGHT ? term->y + 5 : term->y; break;
            case 'A': term->y = term->y >= 5 ? term->y - 5 : 0; break;
            case 'B': dec->charset = PLATO_CHARSET_M0; break;
            case 'C': dec->charset = PLATO_CHARSET_M1; break;
            case 'D': dec->charset = PLATO_CHARSET_M2; break;
            case 'E': dec->charset = PLATO_CHARSET_M3; break;
            case 'N': dec->char_size = 0; break;
            case 'O': dec->char_size = 2; break;
            case 'Z': term->margin_x = term->x; break;
            default: break;
        }
        return;
    }
    if (byte == 0x1B) { dec->state = STATE_ESCAPE; return; }
    if (byte < 0x20) {
        int step = dec->char_size == 2 ? 16 : 8;
        switch (byte) {
            case 0x00:
                /* TUTOR pause / -delay- NOP */
                term->delay_requested = true;
                break;
            case 0x08: term->x = term->x >= step ? term->x - step : 0; break;
            case 0x09: term->x = term->x + step < PLATO_WIDTH ? term->x + step : PLATO_WIDTH - step; break;
            case 0x0A:
                /* PLATO LF: spostamento verticale modulo 512, senza scroll del framebuffer. */
                term->y -= 16;
                if (term->y < 0) term->y += PLATO_HEIGHT;
                break;
            case 0x0B:
                /* PLATO VT: spostamento verticale modulo 512. */
                term->y += 16;
                if (term->y >= PLATO_HEIGHT) term->y -= PLATO_HEIGHT;
                break;
            case 0x0C: term->x = 0; term->y = 496; break;
            case 0x0D:
                /* PLATO CR: ritorno al margine seguito da LF modulo 512. */
                term->x = term->margin_x;
                term->y -= 16;
                if (term->y < 0) term->y += PLATO_HEIGHT;
                break;
            case 0x19:
                dec->mode7_active = false; dec->mode2_active = false;
                dec->word_idx = 0; dec->word_param_command = 0;
                dec->data_mode = PLATO_MODE_BLOCK; dec->first_block_coord = true;
                break;
            case 0x1C:
                dec->mode7_active = false; dec->mode2_active = false;
                dec->word_idx = 0; dec->word_param_command = 0;
                dec->data_mode = PLATO_MODE_POINT;
                break;
            case 0x1D:
                dec->mode7_active = false; dec->mode2_active = false;
                dec->word_idx = 0; dec->word_param_command = 0;
                dec->data_mode = PLATO_MODE_LINE; dec->first_line_coord = true;
                break;
            case 0x1F:
                dec->mode7_active = false; dec->mode2_active = false;
                dec->word_idx = 0; dec->word_param_command = 0;
                dec->data_mode = PLATO_MODE_ALPHA;
                break;
            default: break;
        }
        return;
    }
    if (dec->mode7_active || dec->mode2_active) {
        dec->word_buf[dec->word_idx++] = byte;
        if (dec->word_idx >= 3) {
            uint32_t word = decode_word18(dec->word_buf);
            dec->word_idx = 0;
            if (dec->mode2_active) mode2_word_complete(dec, term, word);
        }
        return;
    }

    if (dec->state == STATE_LOAD_COORD || dec->data_mode == PLATO_MODE_POINT ||
        dec->data_mode == PLATO_MODE_LINE || dec->data_mode == PLATO_MODE_BLOCK) {
        if (coordinate_byte(dec, term, byte)) return;
    }
    if (dec->data_mode == PLATO_MODE_ALPHA) {
        int advance = dec->char_size == 2 ? 16 : 8;
        plato_draw_char(&term->fb, &term->font, dec->charset, byte, term->x, term->y, dec->screen_mode, dec->char_size);
        /* In Alpha PLATO l avanzamento orizzontale avvolge modulo 512 senza LF o scroll. */
        term->x += advance;
        if (term->x >= PLATO_WIDTH) term->x -= PLATO_WIDTH;
    }
}

void plato_protocol_process_bytes(plato_protocol_decoder_t *dec, plato_terminal_t *term, const uint8_t *data, size_t len) {
    if (!dec || !term || !data) return;
    for (size_t i = 0; i < len; i++) plato_protocol_process_byte(dec, term, data[i]);
}
