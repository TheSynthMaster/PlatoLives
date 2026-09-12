#include "plato/plato_terminal.h"
#include <assert.h>
#include <stdio.h>

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

int main(void) {
    test_full_stream();
    return 0;
}
