#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "plato/plato_terminal.h"
#include "plato/plato_microtutor.h"

int main(void) {
    printf("[*] Inizio test_microtutor (Z80)...\n");

    plato_terminal_t term;
    plato_terminal_init(&term);

    assert(term.mt.term == &term);
    assert(!term.mt.running);
    assert(!term.mt.halted);

    /*
     * Test Z80 assembly (ciclo decremento con salto relativo JR NZ):
     * 0000: 0E 03       LD C, 3
     * 0002: 3E 00       LD A, 0
     * 0004: 3C          INC A
     * 0005: 0D          DEC C
     * 0006: 20 FC       JR NZ, -4 (torna a 0004)
     * 0008: 32 34 12    LD (0x1234), A
     * 000B: D3 02       OUT (0x02), A
     * 000D: 76          HALT
     */
    const uint8_t code[] = {
        0x0E, 0x03,
        0x3E, 0x00,
        0x3C,
        0x0D,
        0x20, 0xFC,
        0x32, 0x34, 0x12,
        0xD3, 0x02,
        0x76
    };

    memcpy(&term.ram[0x0000], code, sizeof(code));

    plato_microtutor_start(&term.mt, 0x0000);
    assert(term.mt.running);

    int cycles = plato_microtutor_run_cycles(&term.mt, 200);
    assert(cycles > 0);

    assert(term.mt.cpu.registers.byte[Z80_A] == 3);
    assert(term.ram[0x1234] == 3);
    assert(term.mt.io_ports[0x02] == 3);
    assert(term.mt.halted);

    printf("[+] Eseguiti %d cicli CPU Z80.\n", cycles);
    printf("[+] Verifica RAM[0x1234] = %d (atteso 3)\n", term.ram[0x1234]);
    printf("[+] Verifica CPU A = %d (atteso 3)\n", term.mt.cpu.registers.byte[Z80_A]);
    printf("[+] Verifica CPU Halted = %s\n", term.mt.halted ? "true" : "false");
    printf("[+] test_microtutor Z80 SUPERATO CON SUCCESSO!\n");

    return 0;
}
