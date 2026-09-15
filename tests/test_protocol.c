#include "plato/plato_terminal.h"
#include <assert.h>
#include <stdio.h>

void test_coord_decompression(void) {
    plato_terminal_t term;
    plato_terminal_init(&term);

    /* 1. Carica coordinata iniziale (100, 200) con ESC 2 */
    uint8_t stream[] = { 0x1B, '2', 0x26, 0x68, 0x23, 0x44 };
    plato_terminal_feed(&term, stream, sizeof(stream));

    assert(term.x == 100);
    assert(term.y == 200);

    /* 2. Switch a Line Mode (0x1D) e primo punto (100, 200) */
    uint8_t start_line[] = { 0x1D, 0x26, 0x68, 0x23, 0x44 };
    plato_terminal_feed(&term, start_line, sizeof(start_line));

    assert(term.x == 100);
    assert(term.y == 200);

    /* 3. Secondo punto (110, 200) -> Traccia la linea */
    uint8_t delta_stream[] = { 0x4E };
    plato_terminal_feed(&term, delta_stream, sizeof(delta_stream));

    assert(term.x == 110);
    assert(term.y == 200);

    printf("[PASS] test_coord_decompression\n");
}

static void assert_color_sequence_consumed(const uint8_t *stream, size_t len) {
    plato_terminal_t term;
    plato_terminal_init(&term);
    plato_terminal_feed(&term, stream, len);

    /* Devono essere disegnati soltanto X e Y: nessun O o @ residuo. */
    assert(term.x == 16);
    assert(term.y == 496);
    assert(term.decoder.state == STATE_NORMAL);
    assert(term.decoder.skip_bytes_remaining == 0);
}

void test_telnet_iac_color_sequences(void) {
    const uint8_t after_date[] = {
        'X', 0x1B, 'a', 0xFF, 0xFF, 0xFF, 0xCF, 0xC0, 'Y'
    };
    const uint8_t after_users[] = {
        'X', 0x1B, 'a', 0xFF, 0xFF, 0x7F, 0xCF, 0xC0, 'Y'
    };
    const uint8_t after_next[] = {
        'X', 0x1B, 'a', 0xFF, 0xFF, 0xFF, 0xFF, 0xCF, 0xC0, 'Y'
    };

    assert_color_sequence_consumed(after_date, sizeof(after_date));
    assert_color_sequence_consumed(after_users, sizeof(after_users));
    assert_color_sequence_consumed(after_next, sizeof(after_next));
    printf("[PASS] test_telnet_iac_color_sequences\\n");
}

void test_mode7_persistent_words(void) {
    plato_terminal_t term;
    plato_terminal_init(&term);

    /* ESC V attiva Mode 7 persistente. ESC 12 ed ESC X non devono terminarla. */
    const uint8_t stream[] = {
        0x1B, 'V',
        0x1B, 0x12,
        0x1B, 'X',
        'N', 'A', 'M', 'E', 'G', 'R', 'O', 'U', 'P',
        0x1F, 'Z'
    };

    plato_terminal_feed(&term, stream, sizeof(stream));

    /* I nove byte Mode 7 non devono essere disegnati; dopo ALPHA viene disegnata solo Z. */
    assert(term.x == 8);
    assert(term.y == 496);
    assert(term.decoder.mode7_active == false);
    assert(term.decoder.word_idx == 0);
    assert(term.decoder.data_mode == PLATO_MODE_ALPHA);
    printf("[PASS] test_mode7_persistent_words\n");
}

void test_load_char_mem_persistent_words(void) {
    plato_terminal_t term;
    plato_terminal_init(&term);

    const uint8_t stream[] = {
        0x1B, 'S',                  /* LoadMem persistente */
        'A', 'B', 'C', 'D', 'E', 'F',
        0x1B, 'W', '1', '2', '3',  /* LoadAddr transitorio, poi riprende LoadMem */
        'G', 'H', 'I',
        0x1B, 'P',                  /* passa a LoadChar persistente */
        'J', 'K', 'L', 'M', 'N', 'O',
        0x1F, 'Z'                   /* Alpha termina la modalita persistente */
    };

    plato_terminal_feed(&term, stream, sizeof(stream));

    /* Tutti i payload P/S/W sono consumati; viene disegnata soltanto Z. */
    assert(term.x == 8);
    assert(term.y == 496);
    assert(term.decoder.mode7_active == false);
    assert(term.decoder.mode2_active == false);
    assert(term.decoder.word_idx == 0);
    assert(term.decoder.state == STATE_NORMAL);
    assert(term.decoder.data_mode == PLATO_MODE_ALPHA);
    printf("[PASS] test_load_char_mem_persistent_words\n");
}

void test_plato_wrap_without_scroll(void) {
    plato_terminal_t term;
    plato_terminal_init(&term);

    /* Un pixel sentinella dimostra che nessuno dei casi deve scrollare il framebuffer. */
    plato_fb_set_pixel(&term.fb, 7, 500, true);

    term.x = 488;
    term.y = 8;
    const uint8_t alpha[] = { '0', '+', '1' };
    plato_terminal_feed(&term, alpha, sizeof(alpha));
    assert(term.x == 0);
    assert(term.y == 8);
    assert(plato_fb_get_pixel(&term.fb, 7, 500) == true);

    const uint8_t lf[] = { 0x0A };
    plato_terminal_feed(&term, lf, sizeof(lf));
    assert(term.x == 0);
    assert(term.y == 504);
    assert(plato_fb_get_pixel(&term.fb, 7, 500) == true);

    term.x = 123;
    term.y = 8;
    term.margin_x = 40;
    const uint8_t cr[] = { 0x0D };
    plato_terminal_feed(&term, cr, sizeof(cr));
    assert(term.x == 40);
    assert(term.y == 504);
    assert(plato_fb_get_pixel(&term.fb, 7, 500) == true);

    term.y = 504;
    const uint8_t vt[] = { 0x0B };
    plato_terminal_feed(&term, vt, sizeof(vt));
    assert(term.y == 8);
    assert(plato_fb_get_pixel(&term.fb, 7, 500) == true);

    printf("[PASS] test_plato_wrap_without_scroll\n");
}

static void append_word18(uint8_t *stream, size_t *len, uint32_t word) {
    stream[(*len)++] = (uint8_t)(0x40 | (word & 0x3F));
    stream[(*len)++] = (uint8_t)(0x40 | ((word >> 6) & 0x3F));
    stream[(*len)++] = (uint8_t)(0x40 | ((word >> 12) & 0x3F));
}

static uint16_t __attribute__((unused)) test_ram_word(const plato_terminal_t *term, uint16_t address) {
    return (uint16_t)term->ram[address] | ((uint16_t)term->ram[(uint16_t)(address + 1u)] << 8);
}

static void test_beep_callback(void *context) {
    int *count = (int *)context;
    (*count)++;
}

void test_load_echo_beep(void) {
    plato_terminal_t term;
    plato_terminal_init(&term);
    int beep_count = 0;
    uint8_t stream[5];
    size_t len = 0;

    term.beep_callback = test_beep_callback;
    term.beep_context = &beep_count;

    stream[len++] = 0x1B;
    stream[len++] = 'Y';
    append_word18(stream, &len, 0x7B);
    plato_terminal_feed(&term, stream, len);

    assert(beep_count == 1);
    assert(term.decoder.state == STATE_NORMAL);
    assert(term.decoder.word_idx == 0);
    printf("[PASS] test_load_echo_beep\n");
}

void test_mode2_ram_and_dynamic_fonts(void) {
    plato_terminal_t term;
    plato_terminal_init(&term);
    uint8_t stream[256];
    size_t len = 0;

    assert(test_ram_word(&term, PLATO_C2ORIGIN_ADDRESS) == PLATO_DEFAULT_M2_ORIGIN);
    assert(test_ram_word(&term, PLATO_C3ORIGIN_ADDRESS) == PLATO_DEFAULT_M3_ORIGIN);

    /* Sequenza reale ridotta degli scacchi: riposiziona M2 a 0x3800 e M3 a 0x3C00. */
    stream[len++] = 0x1B; stream[len++] = 'W'; append_word18(stream, &len, PLATO_C2ORIGIN_ADDRESS);
    stream[len++] = 0x1B; stream[len++] = 'S';
    append_word18(stream, &len, 0x3800);
    append_word18(stream, &len, 0x3C00);
    assert(len < sizeof(stream));
    plato_terminal_feed(&term, stream, len);
    assert(test_ram_word(&term, PLATO_C2ORIGIN_ADDRESS) == 0x3800);
    assert(test_ram_word(&term, PLATO_C3ORIGIN_ADDRESS) == 0x3C00);
    assert(term.decoder.load_address == 0x230A);
    assert(term.decoder.mode2_active == true);

    /* P e S sono lo stesso Mode 2. Carica M2[0x20], prima colonna accesa. */
    len = 0;
    stream[len++] = 0x1B; stream[len++] = 'W'; append_word18(stream, &len, 0x3800);
    stream[len++] = 0x1B; stream[len++] = 'P';
    append_word18(stream, &len, 0xFFFF);
    for (int i = 1; i < 8; i++) append_word18(stream, &len, 0);
    plato_terminal_feed(&term, stream, len);
    const uint8_t *m2 = plato_font_get_glyph(&term.font, PLATO_CHARSET_M2, 0x20);
    (void)m2;
    for (int row = 0; row < 16; row++) assert(m2[row] == 0x80);
    assert(test_ram_word(&term, 0x3800) == 0xFFFF);
    assert(term.decoder.load_address == 0x3810);

    /* Carica M3[0x20], ultima colonna accesa. */
    len = 0;
    stream[len++] = 0x1B; stream[len++] = 'W'; append_word18(stream, &len, 0x3C00);
    stream[len++] = 0x1B; stream[len++] = 'S';
    for (int i = 0; i < 7; i++) append_word18(stream, &len, 0);
    append_word18(stream, &len, 0xFFFF);
    plato_terminal_feed(&term, stream, len);
    const uint8_t *m3 = plato_font_get_glyph(&term.font, PLATO_CHARSET_M3, 0x20);
    (void)m3;
    for (int row = 0; row < 16; row++) assert(m3[row] == 0x01);
    assert(test_ram_word(&term, 0x3C0E) == 0xFFFF);
    assert(term.decoder.load_address == 0x3C10);

    /* Q e R consumano una Word ma non modificano il memory address register. */
    len = 0;
    stream[len++] = 0x1B; stream[len++] = 'Q'; append_word18(stream, &len, 0x0123);
    stream[len++] = 0x1B; stream[len++] = 'R'; append_word18(stream, &len, 0x0456);
    plato_terminal_feed(&term, stream, len);
    assert(term.decoder.load_address == 0x3C10);
    assert(term.decoder.mode2_active == true);

    /* Scrittura RAM generale e wrap 0xFFFF -> 0x0000. */
    len = 0;
    stream[len++] = 0x1B; stream[len++] = 'W'; append_word18(stream, &len, 0xFFFF);
    stream[len++] = 0x1B; stream[len++] = 'P'; append_word18(stream, &len, 0x1234);
    plato_terminal_feed(&term, stream, len);
    assert(term.ram[0xFFFF] == 0x34);
    assert(term.ram[0x0000] == 0x12);
    assert(term.decoder.load_address == 0x0001);

    printf("[PASS] test_mode2_ram_and_dynamic_fonts\n");
}

void test_paint_parameter_consumed(void) {
    plato_terminal_t term;
    plato_terminal_init(&term);

    const uint8_t stream[] = {
        0x1B, 'c',
        0x40, 0x40,
        0x1F, 'Z'
    };

    plato_terminal_feed(&term, stream, sizeof(stream));

    assert(term.decoder.state == STATE_NORMAL);
    assert(term.decoder.skip_bytes_remaining == 0);
    assert(term.decoder.data_mode == PLATO_MODE_ALPHA);

    /* I parametri Paint non devono essere visualizzati: viene disegnata soltanto Z. */
    assert(term.x == 8);
    assert(term.y == 496);

    printf("[PASS] test_paint_parameter_consumed\n");
}

void test_form_feed_preserves_charset(void) {
    plato_terminal_t term;
    plato_terminal_init(&term);

    const uint8_t stream[] = {
        0x1B, 'D',       /* seleziona M2 */
        0x1B, 0x0C       /* full erase */
    };

    plato_terminal_feed(&term, stream, sizeof(stream));

    assert(term.decoder.charset == PLATO_CHARSET_M2);
    assert(term.decoder.data_mode == PLATO_MODE_ALPHA);
    assert(term.decoder.screen_mode == PLATO_SCREEN_REWRITE);

    printf("[PASS] test_form_feed_preserves_charset\n");
}

void test_ascii_to_plato_keycodes(void) {
    assert(plato_protocol_keycode_for_ascii('0') == 0x00);
    assert(plato_protocol_keycode_for_ascii('5') == 0x05);
    assert(plato_protocol_keycode_for_ascii('6') == 0x06);
    assert(plato_protocol_keycode_for_ascii('a') == 0x41);
    assert(plato_protocol_keycode_for_ascii('A') == 0x61);
    assert(plato_protocol_keycode_for_ascii(0xFF) == UINT16_MAX);
    printf("[PASS] test_ascii_to_plato_keycodes\n");
}

int main(void) {
    test_ascii_to_plato_keycodes();
    test_coord_decompression();
    test_telnet_iac_color_sequences();
    test_mode7_persistent_words();
    test_load_char_mem_persistent_words();
    test_load_echo_beep();
    test_mode2_ram_and_dynamic_fonts();
    test_plato_wrap_without_scroll();
	test_paint_parameter_consumed();
	test_form_feed_preserves_charset();
    return 0;
}
