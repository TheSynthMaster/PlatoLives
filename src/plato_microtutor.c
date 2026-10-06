#include "plato/plato_microtutor.h"
#include "plato/plato_terminal.h"
#include "plato/plato_graphics.h"
#include "plato/plato_transport.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#define R_MAIN   0x3D
#define R_INIT   0x40
#define R_DOT    0x43
#define R_LINE   0x46
#define R_CHARS  0x49
#define R_BLOCK  0x4C
#define R_INPX   0x4F
#define R_INPY   0x52
#define R_OUTX   0x55
#define R_OUTY   0x58
#define R_XMIT   0x5B
#define R_MODE   0x5E
#define R_STEPX  0x61
#define R_STEPY  0x64
#define R_WE     0x67
#define R_DIR    0x6A
#define R_INPUT  0x6D
#define R_SSF    0x70
#define R_CCR    0x73
#define R_EXTOUT 0x76
#define R_EXEC   0x79
#define R_GJOB   0x7C
#define R_XJOB   0x7F
#define R_RETURN 0x82
#define R_CHRCV  0x85
#define R_ALARM  0x88
#define R_PRINT  0x8B
#define R_FCOLOR 0x8E
#define R_BCOLOR 0x91
#define R_PAINT  0x94
#define R_WAIT16 0x97
#define R_DUMMY2 0x9A
#define R_DUMMY3 0x9D

#define M_CCR    0x22F4
#define M_DIR    0x22F8

#define CALL8080 0xCD
#define JUMP8080 0xC3
#define RET8080  0xC9

uint8_t plato_microtutor_read_byte(void *context, uint16_t address) {
    plato_microtutor_t *mt = (plato_microtutor_t *)context;
    if (!mt || !mt->term) return 0xFF;
    return mt->term->ram[address];
}

void plato_microtutor_write_byte(void *context, uint16_t address, uint8_t value) {
    plato_microtutor_t *mt = (plato_microtutor_t *)context;
    if (!mt || !mt->term) return;
    mt->term->ram[address] = value;
}

/* --- CDC IST-II Floppy Controller Emulation --- */

static void floppy_calc_check(plato_floppy_t *fl, uint8_t b) {
    uint8_t cupper = (uint8_t)((fl->chksum >> 8) & 0xFF);
    uint8_t clower = (uint8_t)(fl->chksum & 0xFF);
    cupper ^= b;
    int x = (int)cupper << 1;
    if (x & 0x100) x = (x | 1) & 0xFF;
    cupper = (uint8_t)x;
    clower ^= b;
    int y = 0;
    if (clower & 1) y = 0x80;
    x = (int)(clower >> 1);
    x = (x | y) & 0xFF;
    clower = (uint8_t)x;
    fl->chksum = (uint16_t)(((uint16_t)cupper << 8) | clower);
}

static void floppy_read_reset(plato_floppy_t *fl) {
    fl->rcnt = 1;
    fl->chksum = 0;
}

static void floppy_write_reset(plato_floppy_t *fl) {
    fl->wcnt = 1;
}

static void floppy_seek(plato_floppy_t *fl, long loc) {
    if (!fl->fp) return;
    fseek((FILE *)fl->fp, loc, SEEK_SET);
    fl->position = loc;
    fl->rcnt = 1;
    fl->wcnt = 1;
}

static uint8_t floppy_read_byte(plato_floppy_t *fl) {
    if (!fl->fp) return 0;
    if (fl->rcnt == 129) {
        fl->rcnt++;
        return (uint8_t)(fl->chksum & 0xFF);
    } else if (fl->rcnt == 130) {
        uint8_t ret = (uint8_t)((fl->chksum >> 8) & 0xFF);
        floppy_read_reset(fl);
        return ret;
    }
    uint8_t b = 0;
    if (fread(&b, 1, 1, (FILE *)fl->fp) == 1) {
        fl->position++;
    }
    fl->rcnt++;
    floppy_calc_check(fl, b);
    return b;
}

static void floppy_write_byte(plato_floppy_t *fl, uint8_t val) {
    if (!fl->fp) return;
    if (fl->wcnt > 129) {
        floppy_write_reset(fl);
        return;
    }
    if (fl->wcnt > 128) {
        fl->wcnt++;
        return;
    }
    fwrite(&val, 1, 1, (FILE *)fl->fp);
    fl->position++;
    fl->wcnt++;
    if (fl->wcnt > 130) {
        floppy_write_reset(fl);
    }
}

static bool floppy_open(plato_floppy_t *fl, const char *filepath) {
    if (fl->fp) {
        fclose((FILE *)fl->fp);
        fl->fp = NULL;
    }
    FILE *f = fopen(filepath, "r+b");
    if (!f) {
        f = fopen(filepath, "rb");
    }
    if (!f) return false;
    fl->fp = f;
    strncpy(fl->filepath, filepath, sizeof(fl->filepath) - 1);
    fl->filepath[sizeof(fl->filepath) - 1] = '\0';
    fl->position = 0;
    fl->rcnt = 1;
    fl->wcnt = 1;
    fl->chksum = 0;
    return true;
}

static void floppy_close(plato_floppy_t *fl) {
    if (fl->fp) {
        fclose((FILE *)fl->fp);
        fl->fp = NULL;
    }
    fl->filepath[0] = '\0';
}

uint8_t plato_microtutor_input_byte(void *context, uint8_t port) {
    plato_microtutor_t *mt = (plato_microtutor_t *)context;
    if (!mt) return 0xFF;

    switch (port) {
        case 0x00:
            return mt->io_ports[0x00];
        case 0x01:
            mt->io_ports[0x00] &= ~0x01;
            return mt->io_ports[0x01];
        case 0x2A:
            return 0x37;
        case 0x2B:
            return 1;
        case 0xAA:
            return 0x38;
        case 0xAB:
            return 0x01;
        case 0xAE: /* CDC floppy data port */
            switch (mt->mtdrivefunc) {
                case 0: /* read next byte from disk */
                    return floppy_read_byte(&mt->floppy[mt->mt_disk_unit & 1]);
                case 2: /* write data - noop on read */
                    return 0;
                case 11: /* read millisecond clock - chiamato 2 volte (fase bassa e alta) */
                    {
                        uint16_t temp = (uint16_t)(mt->term ? mt->term->mclock_acc_ms : 0);
                        uint8_t ret = mt->clock_phase ? (uint8_t)(temp & 0xFF) : (uint8_t)((temp >> 8) & 0xFF);
                        mt->clock_phase = !mt->clock_phase;
                        return ret;
                    }
                case 4:
                    return mt->mt_single_data;
                default:
                    return 0;
            }
        case 0xAF: /* CDC floppy control port */
            {
                uint8_t ret = mt->mt_can_resp;
                switch (mt->mtdrivefunc) {
                    case 0:
                    case 2:
                    case 11:
                        mt->mt_can_resp = 0x50;
                        break;
                    default:
                        break;
                }
                return ret;
            }
        default:
            return mt->io_ports[port];
    }
}

void plato_microtutor_output_byte(void *context, uint8_t port, uint8_t value) {
    plato_microtutor_t *mt = (plato_microtutor_t *)context;
    if (!mt || !mt->term) return;

    if (port != 0x00 && port != 0xAE && port != 0xAF) {
        plato_microtutor_log_msg(mt, "[Z80 OUT] port:0x%02X val:0x%02X (%d)\n", port, value, value);
    }
    mt->io_ports[port] = value;

    switch (port) {
        case 0x02:
            if (value != 0) {
                plato_terminal_beep(mt->term);
            }
            break;

        case 0x2B:
        case 0xAB:
            break;

        case 0xAE: /* CDC floppy data port */
            switch (mt->mtdrivefunc) {
                case 0: /* read setup */
                case 2: /* write setup & data */
                    switch (mt->mt_data_phase++) {
                        case 1: mt->mt_disk_unit = value; break;
                        case 2: mt->mt_disk_track = value; break;
                        case 3: mt->mt_disk_sector = value; break;
                        case 4: mt->mt_disk1 = value; break;
                        case 5: mt->mt_disk2 = value; break;
                        case 6: mt->mt_disk_check1 = value; break;
                        case 7:
                            mt->mt_disk_check2 = value;
                            mt->mt_seek_pos = (128L * 64L * (long)mt->mt_disk_track) +
                                              (128L * ((long)mt->mt_disk_sector - 1L));
                            if (mt->mt_seek_pos >= 0) {
                                floppy_seek(&mt->floppy[mt->mt_disk_unit & 1], mt->mt_seek_pos);
                            }
                            break;
                        default: /* write data */
                            floppy_write_byte(&mt->floppy[mt->mt_disk_unit & 1], value);
                            mt->mt_can_resp = 0x50;
                            break;
                    }
                    break;
                case 10: /* format */
                    switch (mt->mt_data_phase++) {
                        case 1: mt->mt_disk_unit = value; break;
                        case 2: mt->mt_disk_track = value; break;
                        case 3: mt->mt_disk_sector = value; break;
                        case 4: mt->mt_disk1 = value; break;
                        case 5: mt->mt_disk2 = value; break;
                        default: break;
                    }
                    break;
                default:
                    break;
            }
            break;

        case 0xAF: /* CDC floppy control port */
            {
                uint8_t comp = (uint8_t)(~value);
                bool ok = (comp == mt->mtdrivetemp);
                if (ok) {
                    mt->mtdrivefunc = mt->mtdrivetemp;
                    mt->mtdrivetemp = 0xCB;
                    mt->mt_data_phase = 1;
                    switch (mt->mtdrivefunc) {
                        case 0:
                        case 2:
                        case 10:
                        case 11:
                            mt->mt_can_resp = 0x4A;
                            mt->clock_phase = true;
                            break;
                        case 4:
                            mt->mt_can_resp = 0x4A;
                            mt->mt_single_data = 0x02;
                            if (mt->floppy[1].fp) {
                                mt->mt_single_data |= 0x80;
                            }
                            break;
                        case 8:
                            mt->mt_can_resp = 0x50;
                            break;
                        default:
                            mt->mt_single_data = 2;
                            break;
                    }
                } else {
                    mt->mtdrivetemp = value;
                    mt->mt_can_resp = 0x48;
                }
            }
            break;

        default:
            break;
    }
}

bool plato_microtutor_dump_ram(plato_microtutor_t *mt, const char *filename) {
    if (!mt || !mt->term) return false;
    char path[1024];
    const char *fname = (filename && filename[0]) ? filename : "z80ram.dmp";
    const char *home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");
    if (home) {
        snprintf(path, sizeof(path), "%s/Desktop/%s", home, fname);
    } else {
        snprintf(path, sizeof(path), "%s", fname);
    }
    FILE *f = fopen(path, "wb");
    if (!f && home) {
        snprintf(path, sizeof(path), "%s/%s", home, fname);
        f = fopen(path, "wb");
    }
    if (!f) {
        snprintf(path, sizeof(path), "%s", fname);
        f = fopen(path, "wb");
    }
    if (f) {
        size_t written = fwrite(mt->term->ram, 1, 65536, f);
        fclose(f);
        printf("[*] Z80 RAM dump salvato in: %s (%zu bytes)\n", path, written);
        plato_microtutor_log_msg(mt, "[Z80 DUMP] RAM salvata in: %s (%zu bytes)\n", path, written);
        return true;
    }
    return false;
}
static void patch_color(uint8_t *RAM, uint8_t low_getvar);

static void apply_mte_patches(plato_microtutor_t *mt, uint8_t plevel) {
    uint8_t *ram = mt->term->ram;
    if (plevel == 2) {
        ram[0x68D9] = 0xCD; ram[0x68DA] = 0x97; ram[0x68DB] = 0x00; /* CALL R_WAIT16 */
        ram[0x602C] = 0x00; ram[0x602D] = 0x00; ram[0x602E] = 0x00; /* NOP off-line check */
        ram[0x5D26] = 0x10; ram[0x5D27] = 0x80;
        ram[0x8010] = 0xCD; ram[0x8011] = 0x2F; ram[0x8012] = 0x60;
        ram[0x8013] = 0xC3; ram[0x8014] = 0x22; ram[0x8015] = 0x5D;
    } else if (plevel == 3) {
        ram[0x6978] = 0xCD; ram[0x6979] = 0x97; ram[0x697A] = 0x00; /* CALL R_WAIT16 */
        ram[0x60D5] = 0x00; ram[0x60D6] = 0x00; /* NOP off-line check */
        ram[0x600D] = 0xC9; /* RET */
    } else if (plevel == 4) {
        ram[0x6967] = 0xCD; ram[0x6968] = 0x97; ram[0x6969] = 0x00; /* CALL R_WAIT16 */
        ram[0x6061] = 0x00; ram[0x6062] = 0x00; /* NOP off-line check */
        ram[0x5F5C] = 0xC9; /* RET */
        patch_color(ram, 0xE5);
    } else if (plevel >= 5) {
        ram[0x6967] = 0xCD; ram[0x6968] = 0x97; ram[0x6969] = 0x00; /* CALL R_WAIT16 */
        ram[0x6061] = 0x00; ram[0x6062] = 0x00; /* NOP off-line check */
        ram[0x5F5C] = 0xC9; /* RET */
        patch_color(ram, 0xEB);
    }
}

bool plato_microtutor_boot_mte(plato_microtutor_t *mt, const char *filepath) {
    if (!mt || !mt->term || !filepath || !filepath[0]) return false;

    if (!floppy_open(&mt->floppy[0], filepath)) {
        plato_microtutor_log_msg(mt, "[MTE BOOT] Errore apertura file '%s'\n", filepath);
        return false;
    }

    floppy_seek(&mt->floppy[0], 25);
    uint8_t boot1 = floppy_read_byte(&mt->floppy[0]);
    if (boot1 == 0) {
        plato_microtutor_log_msg(mt, "[MTE BOOT] Impossibile avviare: nessun boot router impostato (offset 25 == 0)\n");
        floppy_close(&mt->floppy[0]);
        return false;
    }

    floppy_seek(&mt->floppy[0], 36);
    uint8_t plevel = floppy_read_byte(&mt->floppy[0]);
    floppy_read_reset(&mt->floppy[0]);

    uint16_t readnum = 82;
    if (plevel == 2) {
        readnum = 80;
    } else if (plevel == 3) {
        readnum = 81;
    } else if (plevel >= 4 && plevel <= 6) {
        readnum = 82;
    }

    floppy_seek(&mt->floppy[0], 21504);
    uint32_t address = 0x5300;
    for (uint16_t s = 0; s < readnum; s++) {
        for (uint16_t b = 0; b < 128; b++) {
            if (address < PLATO_TERMINAL_RAM_SIZE) {
                mt->term->ram[address++] = floppy_read_byte(&mt->floppy[0]);
            } else {
                floppy_read_byte(&mt->floppy[0]);
            }
        }
        floppy_read_byte(&mt->floppy[0]); /* omit check byte 1 */
        floppy_read_byte(&mt->floppy[0]); /* omit check byte 2 */
    }

    /* Applica patch cooperative per yield idle off-line (riduce CPU host a 0%) */
    apply_mte_patches(mt, plevel);

    /* Reset hardware CPU */
    Z80Reset(&mt->cpu);
    mt->cpu.pc = 0x5306; /* f.inix boot entry point */
    mt->running = true;
    mt->halted = false;
    mt->local_boot = true;
    mt->in_r_exec = false;
    mt->r_exec_wait_ms = 0;
    mt->yield_pc = 0x5306;
    mt->last_key = -1;

    /* Inizializza i colori per MicroTutor */
    mt->term->fb.color_enabled = true;
    mt->term->fb.bg_color = 0xFF000000u; /* Nero */
    mt->term->fb.fg_color = 0xFFFFFFFFu; /* Bianco */

    /* Inizializza l'identificativo hardware MicroTutor come IST-III/Viking (a colori) */
    mt->term->ram[0x22EB] = 0x3C; /* M.TYPE = 0x3C (Terminal ASCII, Memory 32K) */
    mt->term->ram[0x22E6] = 0x01; /* M.SBTYPE = 1 (IST III - a colori) */
    mt->term->ram[0x22E7] = 0x40; /* M.CONFIG = Touch Panel Present (Bit 7) */

    /* Pulisce lo schermo */
    plato_fb_clear(&mt->term->fb);
    plato_terminal_clear_text(mt->term);

    plato_microtutor_log_msg(mt, "[MTE BOOT] Boot eseguito con successo da '%s' (plevel=%u, settori=%u, PC=0x5306)\n",
                             filepath, plevel, readnum);

    /* Avvia l'esecuzione del blocco iniziale di cicli */
    plato_microtutor_emulate(mt);
    return true;
}


void plato_microtutor_set_logging(plato_microtutor_t *mt, bool enabled) {
    if (!mt) return;
    if (enabled && !mt->logging_enabled) {
        char log_path[1024];
        const char *home = getenv("HOME");
        if (!home) home = getenv("USERPROFILE");
        if (home) {
            snprintf(log_path, sizeof(log_path), "%s/Desktop/PlatoLives_z80.log", home);
        } else {
            snprintf(log_path, sizeof(log_path), "PlatoLives_z80.log");
        }
        FILE *f = fopen(log_path, "w");
        if (!f && home) {
            snprintf(log_path, sizeof(log_path), "%s/PlatoLives_z80.log", home);
            f = fopen(log_path, "w");
        }
        if (f) {
            printf("[*] Z80/8080 Diagnostic Log attivo in: %s\n", log_path);
            time_t now = time(NULL);
            struct tm *t_info = localtime(&now);
            char ts[32] = {0};
            if (t_info) strftime(ts, sizeof(ts), "%H:%M:%S", t_info);

            fprintf(f, "=======================================================\n");
            fprintf(f, " PLATOLIVES Z80 / MICROTUTOR DIAGNOSTIC LOG\n");
            fprintf(f, " started=%s pid=%d\n", ts, (int)getpid());
            fprintf(f, "=======================================================\n\n");
            fflush(f);
            mt->log_file = (void *)f;
            mt->logging_enabled = true;
        }
    } else if (!enabled && mt->logging_enabled) {
        if (mt->log_file) {
            FILE *f = (FILE *)mt->log_file;
            fprintf(f, "\n=== Z80 LOG CLOSED (total cycles: %lu) ===\n", (unsigned long)mt->total_cycles);
            fclose(f);
            mt->log_file = NULL;
        }
        mt->logging_enabled = false;
    }
}

void plato_microtutor_log_msg(plato_microtutor_t *mt, const char *fmt, ...) {
    if (!mt || !mt->logging_enabled || !mt->log_file) return;
    
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    long ms = ts.tv_nsec / 1000000;
    
    va_list args;
    va_start(args, fmt);
    
    /* Prefisso con timestamp al millisecondo */
    fprintf((FILE *)mt->log_file, "[%02d:%02d:%02d.%03ld] ", 
            (int)((ts.tv_sec / 3600) % 24), 
            (int)((ts.tv_sec / 60) % 60), 
            (int)(ts.tv_sec % 60), 
            ms);
            
    vfprintf((FILE *)mt->log_file, fmt, args);
    va_end(args);
    fflush((FILE *)mt->log_file);
}

static void patch_color(uint8_t *RAM, uint8_t low_getvar) {
    RAM[0x66AA] = JUMP8080;
    RAM[0x66AB] = 0x00;
    RAM[0x66AC] = 0x80;

    RAM[0x8000] = CALL8080;
    RAM[0x8001] = 0xB0;
    RAM[0x8002] = 0x66;
    RAM[0x8003] = 0x21;
    RAM[0x8004] = 0x25;
    RAM[0x8005] = 0x7D;
    RAM[0x8006] = CALL8080;
    RAM[0x8007] = 0x90;
    RAM[0x8008] = 0x00;

    RAM[0x8009] = CALL8080;
    RAM[0x800A] = low_getvar;
    RAM[0x800B] = 0x71;

    RAM[0x800C] = CALL8080;
    RAM[0x800D] = 0xBD;
    RAM[0x800E] = 0x66;
    RAM[0x800F] = 0x21;
    RAM[0x8010] = 0x25;
    RAM[0x8011] = 0x7D;

    RAM[0x8012] = CALL8080;
    RAM[0x8013] = 0x93;
    RAM[0x8014] = 0x00;

    RAM[0x8015] = JUMP8080;
    RAM[0x8016] = 0x52;
    RAM[0x8017] = 0x61;

    RAM[0x66C3] = 0x20;
    RAM[0x66C4] = 0x80;
    RAM[0x8020] = 0x21;
    RAM[0x8021] = 0x00;
    RAM[0x8022] = 0x00;
    RAM[0x8023] = CALL8080;
    RAM[0x8024] = 0x94;
    RAM[0x8025] = 0x00;
    RAM[0x8026] = JUMP8080;
    RAM[0x8027] = 0x5D;
    RAM[0x8028] = 0x61;
}

static void mtutor_apply_patches(plato_microtutor_t *mt) {
    uint8_t *RAM = mt->term->ram;
    int release = RAM[0x530B];
    if (release == 8) release = RAM[0x530A];

    plato_microtutor_log_msg(mt, "[PPT PATCH] MicroTutor Release Level: %d (RAM[530A]=%02X RAM[530B]=%02X)\n",
                             release, RAM[0x530A], RAM[0x530B]);

    switch (release) {
        case 2:
            RAM[0x68D9] = CALL8080; RAM[0x68DA] = R_WAIT16; RAM[0x68DB] = 0;
            RAM[0x602C] = 0; RAM[0x602D] = 0; RAM[0x602E] = 0;
            RAM[0x5D26] = 0x10; RAM[0x5D27] = 0x80;
            RAM[0x8010] = CALL8080; RAM[0x8011] = 0x2F; RAM[0x8012] = 0x60;
            RAM[0x8013] = JUMP8080; RAM[0x8014] = 0x22; RAM[0x8015] = 0x5D;
            break;
        case 3:
            RAM[0x6978] = CALL8080; RAM[0x6979] = R_WAIT16; RAM[0x697A] = 0;
            RAM[0x60D5] = 0; RAM[0x60D6] = 0;
            RAM[0x600D] = RET8080;
            break;
        case 4:
            RAM[0x6967] = CALL8080; RAM[0x6968] = R_WAIT16; RAM[0x6969] = 0;
            RAM[0x6061] = 0; RAM[0x6062] = 0;
            RAM[0x5F5C] = RET8080;
            patch_color(RAM, 0xE5);
            break;
        case 5:
        case 6:
            RAM[0x6967] = CALL8080; RAM[0x6968] = R_WAIT16; RAM[0x6969] = 0;
            RAM[0x6061] = 0; RAM[0x6062] = 0;
            RAM[0x5F5C] = RET8080;
            patch_color(RAM, 0xEB);
            break;
        default:
            break;
    }
}

static const uint8_t PLATO_M0_TO_ASCII[64] = {
    ':', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
    'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
    'x', 'y', 'z', '0', '1', '2', '3', '4',
    '5', '6', '7', '8', '9', '+', '-', '*',
    '/', '(', ')', '$', '=', ' ', ',', '.',
    '/', '[', ']', '%', '*', '<', '\'', '"',
    '!', ';', '<', '>', '_', '?', '>', ' '
};

static const uint8_t PLATO_M1_TO_ASCII[64] = {
    '#', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z', '~', '"', '^', '\'', '`',
    '^', '>', 'v', '<', '~', 'S', 'D', 'U',
    'n', '{', '}', '&', '!', ' ', '|', 'o',
    '=', 'a', 'b', 'd', 'l', 'm', 'p', 'r',
    's', 'w', '<', '>', 'O', '@', '\\', ' '
};

uint8_t plato_microtutor_fetch_byte(void *context, uint16_t address) {
    plato_microtutor_t *mt = (plato_microtutor_t *)context;
    if (!mt || !mt->term) return 0xFF;

    if (address >= 0x5975 && address <= 0x59FF) {
        uint8_t opcode = mt->term->ram[address];
        plato_microtutor_log_msg(
            mt,
            "[Z80 FETCH] PC:0x%04X OP:0x%02X\\n",
            address,
            opcode);
        return opcode;
    }

    if (address >= 0x003D && address <= 0x00A0) {
        mt->cpu.pc = address;
        int action = plato_microtutor_check_resident(mt, &mt->cpu);
        if (action == 3) {
            /* Fine blocco di programma (R_MAIN / R_INIT): arresta CPU come in pterm (return 2) */
            mt->running = false;
            mt->in_r_exec = false;
            mt->yield_pc = 0x003D;
            return 0x76; /* HALT */
        }
        if (action == 4) {
            /* Yield cooperativo (R_EXEC o R_WAIT16): POP virtuale e pausa temporizzata */
            uint16_t sp = mt->cpu.registers.word[Z80_SP];
            uint16_t ret_pc = (uint16_t)mt->term->ram[sp] |
                              ((uint16_t)mt->term->ram[(uint16_t)(sp + 1u)] << 8);
            mt->cpu.registers.word[Z80_SP] += 2;
            mt->in_r_exec = true;
            mt->yield_pc = ret_pc;
            return 0x76; /* HALT */
        }
        /* Routine gestita in C: emetti RET per fare il POP del return address e riprendere il chiamante */
        return 0xC9; /* RET */
    }
    return mt->term->ram[address];
}

int plato_microtutor_check_resident(void *context, Z80_STATE *state) {
    plato_microtutor_t *mt = (plato_microtutor_t *)context;
    if (!mt || !mt->term) return 0;
    plato_terminal_t *term = mt->term;
    uint16_t pc = (uint16_t)(state->pc & 0xFFFF);

    switch (pc) {
        case R_MAIN:
            plato_microtutor_log_msg(mt, "[Z80 RESIDENT] PC:0x%04X (R_MAIN) -> Program Block Ended\n", pc);
            mt->running = false;
            return 3;

        case R_INIT:
            plato_microtutor_log_msg(mt, "[Z80 RESIDENT] PC:0x%04X (R_INIT) -> Program Ended\n", pc);
            mt->running = false;
            return 3;

        case R_DOT: {
            int x = (int)(state->registers.word[Z80_HL] & 0x1FF);
            int y = (int)(state->registers.word[Z80_DE] & 0x1FF);
            plato_draw_point(&term->fb, x, y, PLATO_SCREEN_WRITE);
            term->x = x;
            term->y = y;
            plato_microtutor_log_msg(mt, "[Z80 RESIDENT] R_DOT (%d,%d)\n", x, y);
            return 1;
        }

        case R_LINE: {
            int x = (int)(state->registers.word[Z80_HL] & 0x1FF);
            int y = (int)(state->registers.word[Z80_DE] & 0x1FF);
            plato_draw_line(&term->fb, term->x, term->y, x, y, PLATO_SCREEN_WRITE);
            plato_microtutor_log_msg(mt, "[Z80 RESIDENT] R_LINE (%d,%d)->(%d,%d)\n", term->x, term->y, x, y);
            term->x = x;
            term->y = y;
            return 1;
        }

        case R_CHARS: {
            uint16_t cp = state->registers.word[Z80_HL];
            uint8_t c = term->ram[cp++];
            bool uncover = false;
            uint8_t m_slot = (uint8_t)((term->ram[M_CCR] & 0x0Eu) >> 1);
            if (m_slot > 3) m_slot = 0;

            while (cp < 65535u) {
                if (c == 077 && term->ram[cp] == 0) break;

                uint8_t code6 = c & 077u;
                if (code6 == 077u) {
                    uncover = true;
                    c = term->ram[cp++];
                    continue;
                }

                int scale = mt->large ? 2 : 1;

                if (uncover) {
                    uncover = false;
                    switch (code6) {
                        case 010: /* Backspace */
                            term->x = (term->x - 8 * scale) & 0x1FF;
                            break;
                        case 011: /* Tab */
                            term->x = (term->x + 8 * scale) & 0x1FF;
                            break;
                        case 012: /* Linefeed */
                            term->y = (term->y - 16 * scale) & 0x1FF;
                            break;
                        case 013: /* Vertical Tab */
                            term->y = (term->y + 16 * scale) & 0x1FF;
                            break;
                        case 015: /* Carriage Return a M_MARGIN */
                            {
                                uint16_t margin = (uint16_t)term->ram[0x22F0] | ((uint16_t)term->ram[0x22F1] << 8);
                                term->x = (int)(margin & 0x1FF);
                                term->y = (term->y - 16 * scale) & 0x1FF;
                            }
                            break;
                        case 020: m_slot = 0; break;
                        case 021: m_slot = 1; break;
                        case 022: m_slot = 2; break;
                        case 023: m_slot = 3; break;
                        case 034: mt->large = false; break;
                        case 035: mt->large = true; break;
                        default: break;
                    }
                } else {
                    uint8_t eff_slot = m_slot;
                    if (c > 0x3Fu) {
                        eff_slot = (m_slot + 1u) & 3u;
                    }

                    if (eff_slot <= 1) {
                        uint8_t ascii = (eff_slot == 0) ? PLATO_M0_TO_ASCII[code6] : PLATO_M1_TO_ASCII[code6];
                        if (ascii != 0) {
                            plato_draw_char(&term->fb, &term->font, PLATO_CHARSET_M0, ascii, term->x, term->y, PLATO_SCREEN_WRITE, scale);
                            plato_terminal_put_char(term, term->x, term->y, ascii, PLATO_CHARSET_M0, PLATO_SCREEN_WRITE, scale);
                        }
                    } else {
                        plato_charset_t charset = (eff_slot == 2) ? PLATO_CHARSET_M2 : PLATO_CHARSET_M3;
                        uint8_t ch = (uint8_t)(0x20u + code6);
                        plato_draw_char(&term->fb, &term->font, charset, ch, term->x, term->y, PLATO_SCREEN_WRITE, scale);
                        plato_terminal_put_char(term, term->x, term->y, ch, charset, PLATO_SCREEN_WRITE, scale);
                    }
                    term->x = (term->x + 8 * scale) & 0x1FF;
                }
                c = term->ram[cp++];
            }
            return 1;
        }

        case R_BLOCK: {
            uint16_t cp = state->registers.word[Z80_HL];
            int x1 = (int)(term->ram[cp] | (term->ram[(uint16_t)(cp + 1u)] << 8)) & 0x1FF;
            int y1 = (int)(term->ram[(uint16_t)(cp + 2u)] | (term->ram[(uint16_t)(cp + 3u)] << 8)) & 0x1FF;
            int x2 = (int)(term->ram[(uint16_t)(cp + 4u)] | (term->ram[(uint16_t)(cp + 5u)] << 8)) & 0x1FF;
            int y2 = (int)(term->ram[(uint16_t)(cp + 6u)] | (term->ram[(uint16_t)(cp + 7u)] << 8)) & 0x1FF;
            plato_draw_block(&term->fb, x1, y1, x2, y2, PLATO_SCREEN_ERASE);
            plato_microtutor_log_msg(mt, "[Z80 RESIDENT] R_BLOCK (%d,%d)-(%d,%d)\n", x1, y1, x2, y2);
            return 1;
        }

        case R_INPX:
            state->registers.word[Z80_HL] = (uint16_t)term->x;
            return 1;

        case R_INPY:
            state->registers.word[Z80_HL] = (uint16_t)term->y;
            return 1;

        case R_OUTX:
            term->x = (int)(state->registers.word[Z80_HL] & 0x1FF);
            return 1;

        case R_OUTY:
            term->y = (int)(state->registers.word[Z80_HL] & 0x1FF);
            return 1;

        case R_XMIT: {
            uint16_t k = state->registers.word[Z80_HL];
            plato_microtutor_log_msg(mt, "[Z80 RESIDENT] R_XMIT key: 0x%04X\n", k);
            if (term->transport && term->transport->connected) {
                uint8_t out[3] = { 0x1B, (uint8_t)(0x40u | (k & 0x3Fu)), (uint8_t)(0x60u | ((k >> 6) & 0x0Fu)) };
                plato_transport_send(term->transport, out, 3);
            }
            return 1;
        }

        case R_MODE:
            plato_microtutor_log_msg(mt, "[Z80 RESIDENT] R_MODE L: 0x%02X\n", state->registers.byte[Z80_L]);
            if (state->registers.byte[Z80_L] & 1u) {
                plato_fb_clear(&term->fb);
                plato_terminal_clear_text(term);
            }
            return 1;

        case R_STEPX:
            term->x = (term->x + ((term->ram[M_DIR] & 2u) ? -1 : 1)) & 0x1FF;
            return 1;

        case R_STEPY:
            term->y = (term->y + ((term->ram[M_DIR] & 1u) ? -1 : 1)) & 0x1FF;
            return 1;

        case R_WE:
            plato_draw_point(&term->fb, term->x, term->y, PLATO_SCREEN_WRITE);
            return 1;

        case R_DIR:
            term->ram[M_DIR] = state->registers.byte[Z80_L] & 3u;
            return 1;

        case R_CCR:
            term->ram[M_CCR] = state->registers.byte[Z80_L];
            return 1;

        case R_EXEC:
            mt->r_exec_wait_ms = 30; /* RESIDENTMSEC = 30 ms */
            plato_microtutor_log_msg(mt, "[Z80 RESIDENT] R_EXEC -> Yield 30 ms\n");
            return 4;

        case R_ALARM:
            plato_terminal_beep(term);
            return 1;

        case R_FCOLOR: {
            uint32_t r = state->registers.byte[Z80_H];
            uint32_t g = state->registers.byte[Z80_L];
            uint32_t b = state->registers.byte[Z80_D];
            term->fb.fg_color = (0xFF000000u) | (r << 16) | (g << 8) | b;
            term->fb.color_enabled = true;
            return 1;
        }
        case 0x8F: { /* R_FCOLOR + 1: da RAM */
            uint16_t de = state->registers.word[Z80_DE];
            uint32_t r = term->ram[de];
            uint32_t g = term->ram[(uint16_t)(de + 1u)];
            uint32_t b = term->ram[(uint16_t)(de + 2u)];
            term->fb.fg_color = (0xFF000000u) | (r << 16) | (g << 8) | b;
            term->fb.color_enabled = true;
            return 1;
        }
        case 0x90: { /* R_FCOLOR + 2: floating */
            uint16_t loc = state->registers.word[Z80_HL];
            uint8_t exp = term->ram[(uint16_t)(loc + 1u)];
            uint32_t cb = ((uint32_t)term->ram[(uint16_t)(loc + 2u)] << 16) |
                          ((uint32_t)term->ram[(uint16_t)(loc + 3u)] << 8) |
                          ((uint32_t)term->ram[(uint16_t)(loc + 4u)]);
            if (0x18 >= exp) cb >>= (0x18 - exp);
            term->fb.fg_color = (0xFF000000u) | (((cb >> 16) & 0xFFu) << 16) | (((cb >> 8) & 0xFFu) << 8) | (cb & 0xFFu);
            term->fb.color_enabled = true;
            return 1;
        }

        case R_BCOLOR: {
            uint32_t r = state->registers.byte[Z80_H];
            uint32_t g = state->registers.byte[Z80_L];
            uint32_t b = state->registers.byte[Z80_D];
            term->fb.bg_color = (0xFF000000u) | (r << 16) | (g << 8) | b;
            term->fb.color_enabled = true;
            return 1;
        }
        case 0x92: { /* R_BCOLOR + 1: da RAM */
            uint16_t de = state->registers.word[Z80_DE];
            uint32_t r = term->ram[de];
            uint32_t g = term->ram[(uint16_t)(de + 1u)];
            uint32_t b = term->ram[(uint16_t)(de + 2u)];
            term->fb.bg_color = (0xFF000000u) | (r << 16) | (g << 8) | b;
            term->fb.color_enabled = true;
            return 1;
        }
        case 0x93: { /* R_BCOLOR + 2: floating */
            uint16_t loc = state->registers.word[Z80_HL];
            uint8_t exp = term->ram[(uint16_t)(loc + 1u)];
            uint32_t cb = ((uint32_t)term->ram[(uint16_t)(loc + 2u)] << 16) |
                          ((uint32_t)term->ram[(uint16_t)(loc + 3u)] << 8) |
                          ((uint32_t)term->ram[(uint16_t)(loc + 4u)]);
            if (0x18 >= exp) cb >>= (0x18 - exp);
            term->fb.bg_color = (0xFF000000u) | (((cb >> 16) & 0xFFu) << 16) | (((cb >> 8) & 0xFFu) << 8) | (cb & 0xFFu);
            term->fb.color_enabled = true;
            return 1;
        }

        case R_PAINT:
            plato_draw_paint(&term->fb, term->x, term->y, PLATO_SCREEN_WRITE);
            return 1;

        case R_WAIT16:
            mt->r_exec_wait_ms = 15;
            plato_microtutor_log_msg(mt, "[Z80 RESIDENT] R_WAIT16 -> Pause 15 ms\n");
            return 4;

        case 0x98: {
            int ms = (int)state->registers.word[Z80_HL];
            mt->r_exec_wait_ms = (ms > 0) ? ms : 1;
            plato_microtutor_log_msg(mt, "[Z80 RESIDENT] R_WAIT16+1 -> Pause %d ms (HL)\n", ms);
            return 4;
        }

        case 0x99: {
            uint16_t de = state->registers.word[Z80_DE];
            int ms = (int)term->ram[de] | ((int)term->ram[(uint16_t)(de + 1u)] << 8);
            mt->r_exec_wait_ms = (ms > 0) ? ms : 1;
            plato_microtutor_log_msg(mt, "[Z80 RESIDENT] R_WAIT16+2 -> Pause %d ms ([DE])\n", ms);
            return 4;
        }

        case R_INPUT:
            state->registers.word[Z80_HL] = (uint16_t)mt->last_key;
            mt->last_key = -1;
            return 1;

        case R_DUMMY2:
        case R_DUMMY3:
        case R_GJOB:
        case R_XJOB:
        case R_RETURN:
        case R_CHRCV:
        case R_PRINT:
        case R_SSF:
        case R_EXTOUT:
            return 1;

        default:
            plato_microtutor_log_msg(mt, "[Z80 RESIDENT UNKNOWN] PC:0x%04X\n", pc);
            return 0;
    }
}

void plato_microtutor_init(plato_microtutor_t *mt, plato_terminal_t *term) {
    if (!mt) return;
    memset(mt, 0, sizeof(*mt));
    mt->term = term;
    mt->last_key = -1; mt->large = false; Z80Reset(&mt->cpu);
}

void plato_microtutor_reset(plato_microtutor_t *mt) {
    if (!mt) return;
    plato_terminal_t *t = mt->term;
    bool log_en = mt->logging_enabled;
    void *lf = mt->log_file;

    memset(mt, 0, sizeof(*mt));
    mt->term = t;
    mt->logging_enabled = log_en;
    mt->log_file = lf;
    mt->last_key = -1; mt->large = false; Z80Reset(&mt->cpu);
}

void plato_microtutor_start(plato_microtutor_t *mt, uint16_t start_addr) {
    if (!mt) return;
    mt->cpu.pc = start_addr;
    mt->running = true;
    mt->halted = false;
    mt->total_cycles = 0;
    plato_microtutor_log_msg(mt, "[Z80 START] CPU avviata all'indirizzo 0x%04X\n", start_addr);
}

void plato_microtutor_stop(plato_microtutor_t *mt) {
    if (!mt) return;
    mt->running = false;
}

void plato_microtutor_emulate(plato_microtutor_t *mt) {
    if (!mt || !mt->running || mt->in_r_exec) return;
    /* Budget coerente con MicroEmulate di pterm: 500.000.000 cicli */
    plato_microtutor_run_cycles(mt, 500000000);
}

void plato_microtutor_progmode(plato_microtutor_t *mt, uint32_t word, uint16_t origin) {
    if (!mt || !mt->term) return;

    mtutor_apply_patches(mt);

    mt->cpu.registers.byte[Z80_C] = (uint8_t)((word >> 16) & 0xFFu);
    mt->cpu.registers.byte[Z80_D] = (uint8_t)((word >> 8) & 0xFFu);
    mt->cpu.registers.byte[Z80_E] = (uint8_t)(word & 0xFFu);

    mt->cpu.registers.word[Z80_SP] = 0xFFFF;

    /* Push fake return address R_MAIN (0x003D) */
    mt->term->ram[--mt->cpu.registers.word[Z80_SP]] = 0x00;
    mt->term->ram[--mt->cpu.registers.word[Z80_SP]] = 0x3D;

    uint16_t start_pc = (uint16_t)mt->term->ram[origin] |
                       ((uint16_t)mt->term->ram[(uint16_t)(origin + 1u)] << 8);

    if (start_pc == 0) {
        plato_microtutor_log_msg(mt, "[PPT PROGMODE] origin:0x%04X is 0, aborting.\n", origin);
        return;
    }

    mt->cpu.pc = start_pc;
    mt->running = true;
    mt->in_r_exec = false;
    mt->r_exec_wait_ms = 0;
    mt->halted = false;
    mt->total_cycles = 0;

    plato_microtutor_log_msg(mt,
        "[PPT PROGMODE] origin:0x%04X -> PC:0x%04X, word:0x%05X (C:%02X D:%02X E:%02X)\n",
        origin, start_pc, word, mt->cpu.registers.byte[Z80_C],
        mt->cpu.registers.byte[Z80_D], mt->cpu.registers.byte[Z80_E]);

    plato_microtutor_emulate(mt);
}

void plato_microtutor_mode6(plato_microtutor_t *mt, uint32_t word) {
    if (!mt || !mt->term) return;

    mt->in_r_exec = false;
    mt->r_exec_wait_ms = 0;

    mt->cpu.registers.byte[Z80_C] = (uint8_t)((word >> 16) & 0xFFu);
    mt->cpu.registers.byte[Z80_D] = (uint8_t)((word >> 8) & 0xFFu);
    mt->cpu.registers.byte[Z80_E] = (uint8_t)(word & 0xFFu);

    uint16_t m6_pc = (uint16_t)mt->term->ram[0x2302] |
                    ((uint16_t)mt->term->ram[0x2303] << 8);

    if (m6_pc == 0) {
        plato_microtutor_log_msg(mt, "[PPT MODE6] M6ORIGIN is 0, ignoring interrupt.\n");
        return;
    }

    mt->term->ram[--mt->cpu.registers.word[Z80_SP]] = (uint8_t)((mt->cpu.pc >> 8) & 0xFFu);
    mt->term->ram[--mt->cpu.registers.word[Z80_SP]] = (uint8_t)(mt->cpu.pc & 0xFFu);

    mt->cpu.pc = m6_pc;
    mt->running = true;
    mt->halted = false;

    plato_microtutor_log_msg(mt, "[PPT MODE6] Jump to 0x%04X\n", m6_pc);

    plato_microtutor_emulate(mt);
}

void plato_microtutor_mode5(plato_microtutor_t *mt, uint32_t word) {
    if (!mt || !mt->term) return;

    mt->in_r_exec = false;
    mt->r_exec_wait_ms = 0;

    uint8_t c = (uint8_t)((word >> 16) & 0xFFu);
    uint8_t d = (uint8_t)((word >> 8) & 0xFFu);

    if (!(((c & 3) == 0) && (d > 0))) {
        plato_microtutor_progmode(mt, word, 0x2300);
        return;
    }

    mt->cpu.registers.byte[Z80_C] = c;
    mt->cpu.registers.byte[Z80_D] = d;
    mt->cpu.registers.byte[Z80_E] = (uint8_t)(word & 0xFFu);

    uint16_t m5_pc = (uint16_t)mt->term->ram[0x2300] |
                    ((uint16_t)mt->term->ram[0x2301] << 8);

    if (m5_pc == 0) return;

    mt->term->ram[--mt->cpu.registers.word[Z80_SP]] = (uint8_t)((mt->cpu.pc >> 8) & 0xFFu);
    mt->term->ram[--mt->cpu.registers.word[Z80_SP]] = (uint8_t)(mt->cpu.pc & 0xFFu);

    mt->cpu.pc = m5_pc;
    mt->running = true;
    mt->halted = false;

    plato_microtutor_emulate(mt);
}

int plato_microtutor_run_cycles(plato_microtutor_t *mt, int cycles) {
    if (!mt || !mt->running || mt->in_r_exec || cycles <= 0) return 0;

    int executed = Z80Emulate(&mt->cpu, cycles, mt);
    mt->total_cycles += (uint32_t)executed;

    if (mt->in_r_exec || !mt->running) {
        mt->cpu.pc = mt->yield_pc;
    }

    return executed;
}

void plato_microtutor_send_ascii_key(plato_microtutor_t *mt, uint8_t ch) {
    static const uint8_t mtutorcvt[128] = {
         60,  26,  24,  27,  17,  49,   6,  18,
         19,  53,  12,  21,  29,  22,  56,  61,
         16,  58,  25,  16,  50,  21,  59,  48,
         55,  51,  23,  27,  28,  57,  54,  96,

         64, 126, 127,  46,  36,  37,  10,  43,
         41, 123,  40,  14,  95,  15,  94,  93,
          0,   1,   2,   3,   4,   5,   6,   7,
          8,   9, 124,  92,  32,  91,  33, 125,

         42,  97,  98,  99, 100, 101, 102, 103,
        104, 105, 106, 107, 108, 109, 110, 111,
        112, 113, 114, 115, 116, 117, 118, 119,
        120, 121, 122,  34,  45,  35,  13,  38,

         11,  65,  66,  67,  68,  69,  70,  71,
         72,  73,  74,  75,  76,  77,  78,  79,
         80,  81,  82,  83,  84,  85,  86,  87,
         88,  89,  90,  20,  39,  28,  47,  52
    };
    uint8_t zkey = (ch < 128) ? mtutorcvt[ch] : ch;
    plato_microtutor_send_key(mt, zkey);
}

void plato_microtutor_send_key(plato_microtutor_t *mt, uint16_t key) {
    if (!mt) return;
    mt->last_key = (int32_t)key;
    mt->io_ports[0x00] |= 0x01;
    mt->io_ports[0x01] = (uint8_t)(key & 0xFF);
    if (!mt->running) {
        mt->running = true;
        mt->halted = false;
    }
    mt->in_r_exec = false;
    mt->r_exec_wait_ms = 0;
    plato_microtutor_log_msg(mt, "[Z80 KEY] Deliver keycode: %d (0x%04X)\n", key, key);
    plato_microtutor_emulate(mt);
}
