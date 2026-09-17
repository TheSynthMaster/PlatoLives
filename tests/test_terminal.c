#include "plato/plato_terminal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_full_stream(void) {
    plato_terminal_t term;
    plato_terminal_init(&term);

    /* 1. ESC FF: pulizia schermo
     * 2. GS (0x1D): transizione a Line Mode
     * 3. Coordinata 1 (10, 10): imposta il punto iniziale (nessuna linea tracciata)
     * 4. Coordinata 2 (10, 20): traccia la linea da (10, 10) a (10, 20)
     */
    uint8_t stream[] = {
        0x1B, 0x0C,
        0x1D,
        0x20, 0x6A, 0x20, 0x4A, /* Primo punto (10, 10) */
        0x74, 0x4A              /* Secondo punto (10, 20) */
    };
    plato_terminal_feed(&term, stream, sizeof(stream));

    assert(term.x == 10);
    assert(term.y == 20);
    assert(plato_fb_get_pixel(&term.fb, 10, 10) == true);
    assert(plato_fb_get_pixel(&term.fb, 10, 15) == true);
    assert(plato_fb_get_pixel(&term.fb, 10, 20) == true);

    printf("[PASS] test_full_stream\n");
}

static void test_text_grid_and_copy(void) {
    plato_terminal_t term;
    plato_terminal_init(&term);

    /* 1. Matrice vuota all'avvio */
    char buf[256];
    size_t len = plato_terminal_get_text_area(&term, 0, 0, 10, 0, false, buf, sizeof(buf));
    assert(len == 0);
    (void)len;
    assert(buf[0] == '\0');

    /* 2. Scrittura di "PLATO" su riga 0 (y=496, x=0, 8, 16, 24, 32) */
    plato_terminal_put_char(&term, 0, 496, 'P', PLATO_CHARSET_M0, PLATO_SCREEN_WRITE, 1);
    plato_terminal_put_char(&term, 8, 496, 'L', PLATO_CHARSET_M0, PLATO_SCREEN_WRITE, 1);
    plato_terminal_put_char(&term, 16, 496, 'A', PLATO_CHARSET_M0, PLATO_SCREEN_WRITE, 1);
    plato_terminal_put_char(&term, 24, 496, 'T', PLATO_CHARSET_M0, PLATO_SCREEN_WRITE, 1);
    plato_terminal_put_char(&term, 32, 496, 'O', PLATO_CHARSET_M0, PLATO_SCREEN_WRITE, 1);

    /* 3. Carattere M2 ridefinito su riga 0 (x=40): deve essere scartato */
    plato_terminal_put_char(&term, 40, 496, 'X', PLATO_CHARSET_M2, PLATO_SCREEN_WRITE, 1);
    plato_terminal_put_char(&term, 48, 496, '!', PLATO_CHARSET_M0, PLATO_SCREEN_WRITE, 1);

    /* 4. Estrazione area riga 0 verbatim: 'X' diventa spazio */
    len = plato_terminal_get_text_area(&term, 0, 0, 6, 0, false, buf, sizeof(buf));
    assert(len > 0);
    assert(strcmp(buf, "PLATO  !") == 0);

    /* 5. Estrazione area riga 0 compact: doppi spazi collassati in singolo */
    len = plato_terminal_get_text_area(&term, 0, 0, 6, 0, true, buf, sizeof(buf));
    assert(len > 0);
    assert(strcmp(buf, "PLATO !") == 0);

    /* 6. Estrazione multiriga con riga vuota intermedia */
    plato_terminal_put_char(&term, 0, 464, 'T', PLATO_CHARSET_M0, PLATO_SCREEN_WRITE, 1);
    plato_terminal_put_char(&term, 8, 464, 'E', PLATO_CHARSET_M0, PLATO_SCREEN_WRITE, 1);
    plato_terminal_put_char(&term, 16, 464, 'S', PLATO_CHARSET_M0, PLATO_SCREEN_WRITE, 1);
    plato_terminal_put_char(&term, 24, 464, 'T', PLATO_CHARSET_M0, PLATO_SCREEN_WRITE, 1);

    /* Verbatim: mantiene riga 1 vuota */
    len = plato_terminal_get_text_area(&term, 0, 0, 6, 2, false, buf, sizeof(buf));
    assert(strcmp(buf, "PLATO  !\n\nTEST") == 0);

    /* Compact: scarta la riga 1 vuota e collassa spazi */
    len = plato_terminal_get_text_area(&term, 0, 0, 6, 2, true, buf, sizeof(buf));
    assert(strcmp(buf, "PLATO !\nTEST") == 0);

    /* 7. Azzeramento su ESC 0x0C */
    uint8_t clear_cmd[] = { 0x1B, 0x0C };
    plato_terminal_feed(&term, clear_cmd, sizeof(clear_cmd));
    len = plato_terminal_get_text_area(&term, 0, 0, 6, 2, false, buf, sizeof(buf));
    assert(len == 0);
    (void)len;
    assert(buf[0] == '\0');

    printf("[PASS] test_text_grid_and_copy\n");
}

int main(void) {
    test_full_stream();
    test_text_grid_and_copy();
    return 0;
}
