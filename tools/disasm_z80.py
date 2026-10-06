#!/usr/bin/env python3
import os
import sys

# Mappa delle chiamate residenti CDC IST-II MicroTutor
RESIDENTS = {
    0x003D: "R_MAIN",   0x0040: "R_INIT",   0x0043: "R_DOT",
    0x0046: "R_LINE",   0x0049: "R_CHARS",  0x004C: "R_BLOCK",
    0x004F: "R_INPX",   0x0052: "R_INPY",   0x0055: "R_OUTX",
    0x0058: "R_OUTY",   0x005B: "R_MODE",   0x005E: "R_DIR",
    0x0061: "R_EXTIN",  0x0064: "R_EXTOUT", 0x0067: "R_ERASE",
    0x006A: "R_SCRN",   0x006D: "R_INPUT",  0x0070: "R_CCR",
    0x0073: "R_SCALE",  0x0076: "R_ROT",    0x0079: "R_EXEC",
    0x007C: "R_GJOB",   0x007F: "R_XJOB",   0x0082: "R_ALARM",
    0x0085: "R_FCOLOR", 0x0088: "R_BCOLOR", 0x008B: "R_PAINT",
    0x008E: "R_CURS",   0x0091: "R_KEYP",   0x0094: "R_MARK",
    0x0097: "R_WAIT16", 0x0098: "R_WAIT16_HL", 0x0099: "R_WAIT16_DE",
}

def load_ram():
    candidates = [
        os.path.expanduser("~/Desktop/z80ram.dmp"),
        "z80ram.dmp",
        os.path.expanduser("~/z80ram.dmp")
    ]
    for c in candidates:
        if os.path.isfile(c):
            with open(c, "rb") as f:
                return bytearray(f.read())
    print("ERRORE: z80ram.dmp non trovato.")
    sys.exit(1)

ram = load_ram()

def r16(addr):
    if addr + 1 < len(ram):
        return ram[addr] | (ram[addr + 1] << 8)
    return 0

def disasm_one(pc):
    op = ram[pc]
    addr = pc
    bytes_used = 1
    asm = ""

    # Tabella registri
    r8 = ["B", "C", "D", "E", "H", "L", "(HL)", "A"]
    r16_sp = ["BC", "DE", "HL", "SP"]
    r16_af = ["BC", "DE", "HL", "AF"]
    cond = ["NZ", "Z", "NC", "C", "PO", "PE", "P", "M"]

    # Istruzioni singole
    if op == 0x00: asm = "NOP"
    elif op == 0x76: asm = "HALT"
    elif op == 0xC9: asm = "RET"
    elif op == 0xEB: asm = "EX DE, HL"
    elif op == 0x08: asm = "EX AF, AF'"
    elif op == 0xD9: asm = "EXX"
    elif op == 0xE3: asm = "EX (SP), HL"
    elif op == 0xE9: asm = "JP (HL)"
    elif op == 0xF9: asm = "LD SP, HL"
    elif op == 0x07: asm = "RLCA"
    elif op == 0x0F: asm = "RRCA"
    elif op == 0x17: asm = "RLA"
    elif op == 0x1F: asm = "RRA"
    elif op == 0x27: asm = "DAA"
    elif op == 0x2F: asm = "CPL"
    elif op == 0x37: asm = "SCF"
    elif op == 0x3F: asm = "CCF"
    elif op == 0xFB: asm = "EI"
    elif op == 0xF3: asm = "DI"

    # Salto relativo JR
    elif op == 0x18:
        disp = ram[pc + 1]; target = (pc + 2 + (disp if disp < 128 else disp - 256)) & 0xFFFF
        asm = f"JR 0x{target:04X}"; bytes_used = 2
    elif op in (0x20, 0x28, 0x30, 0x38):
        c = ["NZ", "Z", "NC", "C"][(op >> 3) & 3]
        disp = ram[pc + 1]; target = (pc + 2 + (disp if disp < 128 else disp - 256)) & 0xFFFF
        asm = f"JR {c}, 0x{target:04X}"; bytes_used = 2

    # Salto assoluto JP nn
    elif op == 0xC3: nn = r16(pc + 1); asm = f"JP 0x{nn:04X}"; bytes_used = 3
    elif op in (0xC2, 0xCA, 0xD2, 0xDA, 0xE2, 0xEA, 0xF2, 0xFA):
        c = cond[(op >> 3) & 7]; nn = r16(pc + 1)
        asm = f"JP {c}, 0x{nn:04X}"; bytes_used = 3

    # CALL nn
    elif op == 0xCD:
        nn = r16(pc + 1); res = RESIDENTS.get(nn, "")
        comment = f" ; *** {res} ***" if res else ""
        asm = f"CALL 0x{nn:04X}{comment}"; bytes_used = 3
    elif op in (0xC4, 0xCC, 0xD4, 0xDC, 0xE4, 0xEC, 0xF4, 0xFC):
        c = cond[(op >> 3) & 7]; nn = r16(pc + 1); res = RESIDENTS.get(nn, "")
        comment = f" ; *** {res} ***" if res else ""
        asm = f"CALL {c}, 0x{nn:04X}{comment}"; bytes_used = 3

    # RET c
    elif op in (0xC0, 0xC8, 0xD0, 0xD8, 0xE0, 0xE8, 0xF0, 0xF8):
        c = cond[(op >> 3) & 7]; asm = f"RET {c}"

    # PUSH / POP
    elif (op & 0xCF) == 0xC5: r = r16_af[(op >> 4) & 3]; asm = f"PUSH {r}"
    elif (op & 0xCF) == 0xC1: r = r16_af[(op >> 4) & 3]; asm = f"POP {r}"

    # LD r16, nn
    elif (op & 0xCF) == 0x01: r = r16_sp[(op >> 4) & 3]; nn = r16(pc + 1); asm = f"LD {r}, 0x{nn:04X}"; bytes_used = 3
    elif op == 0x3A: nn = r16(pc + 1); asm = f"LD A, (0x{nn:04X})"; bytes_used = 3
    elif op == 0x32: nn = r16(pc + 1); asm = f"LD (0x{nn:04X}), A"; bytes_used = 3
    elif op == 0x2A: nn = r16(pc + 1); asm = f"LD HL, (0x{nn:04X})"; bytes_used = 3
    elif op == 0x22: nn = r16(pc + 1); asm = f"LD (0x{nn:04X}), HL"; bytes_used = 3

    # LD r, n
    elif (op & 0xC7) == 0x06: r = r8[(op >> 3) & 7]; n = ram[pc + 1]; asm = f"LD {r}, 0x{n:02X}"; bytes_used = 2
    # INC / DEC r16
    elif (op & 0xCF) == 0x03: r = r16_sp[(op >> 4) & 3]; asm = f"INC {r}"
    elif (op & 0xCF) == 0x0B: r = r16_sp[(op >> 4) & 3]; asm = f"DEC {r}"
    # ADD HL, r16
    elif (op & 0xCF) == 0x09: r = r16_sp[(op >> 4) & 3]; asm = f"ADD HL, {r}"
    # INC / DEC r8
    elif (op & 0xC7) == 0x04: r = r8[(op >> 3) & 7]; asm = f"INC {r}"
    elif (op & 0xC7) == 0x05: r = r8[(op >> 3) & 7]; asm = f"DEC {r}"

    # LD r, r'
    elif 0x40 <= op <= 0x7F:
        d = r8[(op >> 3) & 7]; s = r8[op & 7]; asm = f"LD {d}, {s}"

    # ALU A, r
    elif 0x80 <= op <= 0xBF:
        alu = ["ADD A,", "ADC A,", "SUB", "SBC A,", "AND", "XOR", "OR", "CP"][(op >> 3) & 7]
        s = r8[op & 7]; asm = f"{alu} {s}"

    # ALU A, n
    elif (op & 0xC7) == 0xC6:
        alu = ["ADD A,", "ADC A,", "SUB", "SBC A,", "AND", "XOR", "OR", "CP"][(op >> 3) & 7]
        n = ram[pc + 1]; asm = f"{alu} 0x{n:02X}"; bytes_used = 2

    # Prefisso CB (Rotazioni e Bit)
    elif op == 0xCB:
        cb = ram[pc + 1]; b = (cb >> 3) & 7; r = r8[cb & 7]; bytes_used = 2
        if cb < 0x40:
            rot = ["RLC", "RRC", "RL", "RR", "SLA", "SRA", "SLL", "SRL"][(cb >> 3) & 7]
            asm = f"{rot} {r}"
        elif cb < 0x80: asm = f"BIT {b}, {r}"
        elif cb < 0xC0: asm = f"RES {b}, {r}"
        else: asm = f"SET {b}, {r}"

    # Prefisso ED
    elif op == 0xED:
        ed = ram[pc + 1]; bytes_used = 2
        if ed == 0xB0: asm = "LDIR"
        elif ed == 0xB8: asm = "LDDR"
        elif ed == 0x4D: asm = "RETI"
        elif ed == 0x45: asm = "RETN"
        elif (ed & 0xCF) == 0x42: r = r16_sp[(ed >> 4) & 3]; asm = f"SBC HL, {r}"
        elif (ed & 0xCF) == 0x4A: r = r16_sp[(ed >> 4) & 3]; asm = f"ADC HL, {r}"
        elif (ed & 0xC7) == 0x43: r = r16_sp[(ed >> 4) & 3]; nn = r16(pc + 2); asm = f"LD (0x{nn:04X}), {r}"; bytes_used = 4
        elif (ed & 0xC7) == 0x4B: r = r16_sp[(ed >> 4) & 3]; nn = r16(pc + 2); asm = f"LD {r}, (0x{nn:04X})"; bytes_used = 4
        else: asm = f"ED 0x{ed:02X}"

    else:
        asm = f"DB 0x{op:02X}"

    hex_b = " ".join(f"{ram[addr + k]:02X}" for k in range(bytes_used))
    return bytes_used, hex_b, asm

def main():
    if len(sys.argv) < 3:
        print("Uso: python3 tools/disasm_z80.py <start_hex> <end_hex_or_count> [titolo]")
        sys.exit(1)

    start = int(sys.argv[1], 16)
    arg2 = sys.argv[2]
    title = sys.argv[3] if len(sys.argv) > 3 else f"Disassemblaggio 0x{start:04X}"

    if arg2.startswith("0x") or arg2.startswith("0X"):
        end = int(arg2, 16)
    elif len(arg2) > 4:
        end = int(arg2, 16)
    else:
        val = int(arg2, 10 if arg2.isdigit() else 16)
        end = (start + val * 3) if val < 500 else val

    print("============================================================")
    print(f" {title} (0x{start:04X} - 0x{end:04X})")
    print("============================================================\n")

    pc = start
    while pc < end and pc < len(ram):
        used, h, asm = disasm_one(pc)
        print(f"  0x{pc:04X}:  {h:<12}  {asm}")
        pc += used
    print()

if __name__ == "__main__":
    main()
