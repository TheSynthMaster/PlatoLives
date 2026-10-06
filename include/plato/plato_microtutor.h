#ifndef PLATO_MICROTUTOR_H
#define PLATO_MICROTUTOR_H

#include <stdint.h>
#include <stdbool.h>
#include "plato/z80emu.h"

typedef struct plato_terminal plato_terminal_t;

typedef struct {
    void *fp; /* FILE * */
    char filepath[1024];
    long position;
    int rcnt;
    int wcnt;
    uint16_t chksum;
} plato_floppy_t;

typedef struct plato_microtutor {
    Z80_STATE cpu;
    plato_terminal_t *term;
    bool running;
    bool halted;
    uint32_t total_cycles;
    uint8_t io_ports[256];
    bool logging_enabled;
    void *log_file;
    int32_t last_key;
    bool large;
    bool in_r_exec;
    int r_exec_wait_ms;
    uint16_t yield_pc;

    /* MicroTutor CDC IST-II Floppy Controller & Local Boot */
    plato_floppy_t floppy[2];
    bool local_boot;
    uint8_t mtdrivefunc;
    uint8_t mtdrivetemp;
    int mt_data_phase;
    uint8_t mt_disk_unit;
    uint8_t mt_disk_track;
    uint8_t mt_disk_sector;
    uint8_t mt_disk1;
    uint8_t mt_disk2;
    uint8_t mt_disk_check1;
    uint8_t mt_disk_check2;
    long mt_seek_pos;
    uint8_t mt_can_resp;
    uint8_t mt_single_data;
    bool clock_phase;
} plato_microtutor_t;

void plato_microtutor_init(plato_microtutor_t *mt, plato_terminal_t *term);
void plato_microtutor_reset(plato_microtutor_t *mt);
void plato_microtutor_start(plato_microtutor_t *mt, uint16_t start_addr);
void plato_microtutor_stop(plato_microtutor_t *mt);
void plato_microtutor_progmode(plato_microtutor_t *mt, uint32_t word, uint16_t origin);
void plato_microtutor_mode5(plato_microtutor_t *mt, uint32_t word);
void plato_microtutor_mode6(plato_microtutor_t *mt, uint32_t word);
void plato_microtutor_emulate(plato_microtutor_t *mt);
int plato_microtutor_run_cycles(plato_microtutor_t *mt, int cycles);
void plato_microtutor_send_key(plato_microtutor_t *mt, uint16_t key);
void plato_microtutor_send_ascii_key(plato_microtutor_t *mt, uint8_t ch);
void plato_microtutor_set_logging(plato_microtutor_t *mt, bool enabled);
void plato_microtutor_log_msg(plato_microtutor_t *mt, const char *fmt, ...);
bool plato_microtutor_dump_ram(plato_microtutor_t *mt, const char *filename);
bool plato_microtutor_boot_mte(plato_microtutor_t *mt, const char *filepath);
int plato_microtutor_check_resident(void *context, Z80_STATE *state);

#endif /* PLATO_MICROTUTOR_H */
