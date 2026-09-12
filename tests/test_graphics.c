#include "plato/plato_graphics.h"
#include <assert.h>
#include <stdio.h>

void test_point_and_modes(void) {
    plato_framebuffer_t fb;
    plato_fb_init(&fb);

    plato_draw_point(&fb, 100, 100, PLATO_SCREEN_WRITE);
    assert(plato_fb_get_pixel(&fb, 100, 100) == true);

    plato_draw_point(&fb, 100, 100, PLATO_SCREEN_ERASE);
    assert(plato_fb_get_pixel(&fb, 100, 100) == false);

    printf("[PASS] test_point_and_modes\n");
}

void test_line_drawing(void) {
    plato_framebuffer_t fb;
    plato_fb_init(&fb);

    plato_draw_line(&fb, 10, 10, 15, 10, PLATO_SCREEN_WRITE);
    for (int x = 10; x <= 15; x++) {
        assert(plato_fb_get_pixel(&fb, x, 10) == true);
    }
    assert(plato_fb_get_pixel(&fb, 16, 10) == false);

    printf("[PASS] test_line_drawing\n");
}

int main(void) {
    test_point_and_modes();
    test_line_drawing();
    return 0;
}
