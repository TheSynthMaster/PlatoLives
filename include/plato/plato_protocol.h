#ifndef PLATO_PROTOCOL_H
#define PLATO_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "plato_types.h"

typedef struct plato_terminal plato_terminal_t;

typedef enum {
    STATE_NORMAL, STATE_ESCAPE, STATE_LOAD_COORD,
    STATE_ECHO_WORD, STATE_WORD_PARAM, STATE_SKIP_PARAM, STATE_PMD,
    STATE_COLOR_PARAM
} plato_parser_state_t;

typedef struct {
    plato_parser_state_t state;
    bool plato_mode;
    bool flow_control;
    plato_data_mode_t data_mode;
    plato_screen_mode_t screen_mode;
    plato_charset_t charset;
    int char_size;
    uint16_t cur_hi_y, cur_lo_y, cur_hi_x, cur_lo_x;
    bool got_lo_y;
    bool first_line_coord;
    bool first_block_coord;
    int block_x0, block_y0;
    uint8_t word_buf[3];
    int word_idx;
    int skip_bytes_remaining;
    uint8_t last_raw_byte; /* TELNET IAC FF FF unescaping */
    bool mode7_active;     /* ESC V: modalita persistente Word, dati ignorati */
    bool mode2_active;      /* ESC P / ESC S: scrittura persistente nella RAM terminale */
    uint8_t word_param_command;
    uint32_t load_address;
    char pmd_buf[1024];
    int pmd_len;
    uint8_t color_cmd;
    uint8_t color_buf[4];
    int color_idx;
} plato_protocol_decoder_t;

void plato_protocol_init(plato_protocol_decoder_t *dec);
void plato_protocol_process_byte(plato_protocol_decoder_t *dec, plato_terminal_t *term, uint8_t byte);
void plato_protocol_process_bytes(plato_protocol_decoder_t *dec, plato_terminal_t *term, const uint8_t *data, size_t len);
uint16_t plato_protocol_keycode_for_ascii(uint8_t ascii);
void plato_protocol_send_key(plato_terminal_t *term, uint16_t key);
void plato_protocol_send_touch(plato_terminal_t *term, int16_t x, int16_t y);

#endif /* PLATO_PROTOCOL_H */
