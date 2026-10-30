#ifndef LINUX_HUD_H
#define LINUX_HUD_H

#include <SDL2/SDL.h>
#include <stdbool.h>

typedef struct {
    double fps_total;
    double fps_partial;
    double fps_full;
    double cpu_percent;
    int display_mode;
    int persistence_ms;
    int distortion;
    int phosphor_color;
    int tek_baud;
    int tek_queue_count;
    int pdp1_font;
    int pdp1_refresh_rate;
    int pdp1_preset;
    char user_info[128];
    bool connected;
} linux_hud_telemetry_t;

void linux_hud_toggle(void);
bool linux_hud_is_open(void);
void linux_hud_update_data(const linux_hud_telemetry_t *telem);
void linux_hud_update_queue(int tek_queue_count, int display_mode, int tek_baud);
void linux_hud_handle_event(const SDL_Event *ev);
void linux_hud_render(void);
void linux_hud_destroy(void);

#endif /* LINUX_HUD_H */
