import signal
import sys
import os
import subprocess

try:
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)
except Exception:
    pass

print("=" * 70)
print("GENERAZIONE TEST OFFLINE 'STELE DI ROSETTA' TEKTRONIX 4014 (4096x3120)")
print("=" * 70)

c_code = """#include "plato/plato_tek_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define WIDTH  4096
#define HEIGHT 3120

/* Buffer immagine RGB 24-bit (4096 * 3120 * 3 = ~38.3 MB) */
static uint8_t *framebuffer = NULL;

/* Colore fosforo P1 Tektronix autentico (Verde vivo su fondo DVBST scuro) */
static const uint8_t P1_R = 0x33;
static const uint8_t P1_G = 0xFF;
static const uint8_t P1_B = 0x66;

static const uint8_t BG_R = 0x02;
static const uint8_t BG_G = 0x0B;
static const uint8_t BG_B = 0x05;

static void plot_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        int idx = (y * WIDTH + x) * 3;
        framebuffer[idx + 0] = r;
        framebuffer[idx + 1] = g;
        framebuffer[idx + 2] = b;
    }
}

/* Resa ottica diagnostica: coordinate subpixel, corpo anisotropo e dwell ai vertici. */
static void add_phosphor(float cx, float cy, float sigma_x, float sigma_y, float intensity) {
    const int rx = (int)ceilf(3.0f * sigma_x), ry = (int)ceilf(3.0f * sigma_y);
    const float ix = 1.0f / (2.0f * sigma_x * sigma_x), iy = 1.0f / (2.0f * sigma_y * sigma_y);
    for (int py = (int)floorf(cy - ry); py <= (int)ceilf(cy + ry); py++) {
        for (int px = (int)floorf(cx - rx); px <= (int)ceilf(cx + rx); px++) {
            if (px < 0 || px >= WIDTH || py < 0 || py >= HEIGHT) continue;
            const float dx = (float)px - cx, dy = (float)py - cy;
            const float w = intensity * expf(-(dx * dx * ix + dy * dy * iy));
            if (w < 0.002f) continue;
            const int idx = (py * WIDTH + px) * 3;
            framebuffer[idx + 0] = (uint8_t)fminf(255.0f, framebuffer[idx + 0] + w * P1_R);
            framebuffer[idx + 1] = (uint8_t)fminf(255.0f, framebuffer[idx + 1] + w * P1_G);
            framebuffer[idx + 2] = (uint8_t)fminf(255.0f, framebuffer[idx + 2] + w * P1_B);
        }
    }
}
static void draw_vector(float x0, float y0, float x1, float y1) {
    const float dx = x1 - x0, dy = y1 - y0;
    const int steps = (int)ceilf(fmaxf(fabsf(dx), fabsf(dy)) * 1.5f);
    add_phosphor(x0, y0, 1.6f, 1.6f, 1.15f);
    for (int i = 0; i <= steps; i++) {
        const float t = steps ? (float)i / (float)steps : 0.0f;
        add_phosphor(x0 + dx * t, y0 + dy * t, 1.0f, 1.35f, 0.50f);
    }
    add_phosphor(x1, y1, 1.6f, 1.6f, 1.15f);
}
/* Disegna un singolo glifo con la parametrizzazione subpixel di PlatoLives. */
static void draw_tek_glyph(uint8_t ascii_char, float x_origin, float y_origin, float step_x, float step_y) {
    const plato_tek_glyph_t *glyph = plato_tek_font_get_glyph(ascii_char);
    if (!glyph || !glyph->points) return;
    const float unit_x = step_x / 1.15f, unit_y = step_y / 1.20f;
    float last_px = 0.0f, last_py = 0.0f;
    for (uint8_t i = 0; i < glyph->count; i++) {
        const plato_tek_stroke_point_t *pt = &glyph->points[i];
        const float px = x_origin + (1.25f + (float)pt->x * 1.15f) * unit_x;
        const float py = y_origin + (3.0f + ((float)pt->y - 2.0f) * 1.20f) * unit_y;
        if (pt->pen_down) draw_vector(last_px, last_py, px, py);
        last_px = px;
        last_py = py;
    }
}
/* Disegna una riga di testo con metrica Tektronix specifica */
static void draw_tek_string(const char *str, float x, float y, float cell_w, float step_x, float step_y) {
    size_t len = strlen(str);
    for (size_t i = 0; i < len; i++) {
        uint8_t c = (uint8_t)str[i];
        if (c != ' ') {
            draw_tek_glyph(c, x, y, step_x, step_y);
        }
        x += cell_w;
    }
}

int main(void) {
    printf("[+] Allocazione canvas 4096x3120...\\n");
    framebuffer = (uint8_t *)malloc(WIDTH * HEIGHT * 3);
    if (!framebuffer) {
        fprintf(stderr, "[-] Impossibile allocare framebuffer!\\n");
        return 1;
    }

    /* Inizializzazione sfondo DVBST */
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        framebuffer[i * 3 + 0] = BG_R;
        framebuffer[i * 3 + 1] = BG_G;
        framebuffer[i * 3 + 2] = BG_B;
    }

    /* =========================================================================
     * METRICHE NATIVE TEKTRONIX 4014
     * ========================================================================= */
    /* Size 1 (74 cpl): cella 56x88, passi 7.0 x 8.0 */
    const float S1_CELL_W = 56.0f, S1_CELL_H = 88.0f, S1_SX = 7.0f, S1_SY = 8.0f;
    /* Size 2 (81 cpl): cella 51x82, passi 6.375 x 7.45 */
    const float S2_CELL_W = 51.0f, S2_CELL_H = 82.0f, S2_SX = 6.375f, S2_SY = 7.45f;
    /* Size 3 (121 cpl): cella 34x53, passi 4.25 x 4.81 */
    const float S3_CELL_W = 34.0f, S3_CELL_H = 53.0f, S3_SX = 4.25f, S3_SY = 4.81f;
    /* Size 4 (133 cpl): cella 31x48, passi 3.875 x 4.36 */
    const float S4_CELL_W = 31.0f, S4_CELL_H = 48.0f, S4_SX = 3.875f, S4_SY = 4.36f;

    /* Testi esatti dello specimen "Stele di Rosetta" (Parentesi annidate corrette: {[( )]}) */
    const char *header_1 = "        THE 4014 AND THE 4015 OFFER AS ADDITIONAL FEATURES, FOUR";
    const char *header_2 = "      DIFFERENT ALPHANUMERIC FORMATS AND CHARACTER SIZES AS FOLLOWS-";

    const char *sec1_title = "          74 characters per line with 35 lines per page.";
    const char *sec1_line1 = "      ABCDEFGHIJKLMNOPQRSTUVWXYZ  abcdefghijklmnopqrstuvwxyz";
    const char *sec1_line2 = "      1234567890 {[( )]} <=> /|\\\\ ?!\\\"#$%&'*`@:; +- _ ^~";

    const char *sec2_title = "          81 characters per line with 38 lines per page.";
    const char *sec2_line1 = "      ABCDEFGHIJKLMNOPQRSTUVWXYZ  abcdefghijklmnopqrstuvwxyz";
    const char *sec2_line2 = "      1234567890 {[( )]} <=> /|\\\\ ?!\\\"#$%&'*`@:; +- _ ^~";

    const char *sec3_title = "          121 characters per line with 58 lines per page.";
    const char *sec3_line1 = "      ABCDEFGHIJKLMNOPQRSTUVWXYZ  abcdefghijklmnopqrstuvwxyz";
    const char *sec3_line2 = "      1234567890 {[( )]} <=> /|\\\\ ?!\\\"#$%&'*`@:; +- _ ^~";

    const char *sec4_title = "          133 characters per line with 64 lines per page.";
    const char *sec4_line1 = "      ABCDEFGHIJKLMNOPQRSTUVWXYZ  abcdefghijklmnopqrstuvwxyz";
    const char *sec4_line2 = "      1234567890 {[( )]} <=> /|\\\\ ?!\\\"#$%&'*`@:; +- _ ^~";

    const char *footer     = "THE 4015 WILL OFFER THE COMPLETE APL CHARACTER SET IDENTICAL TO OUR 4013";

    printf("[+] Rendering specimen forense in corso...\\n");

    /* Header (Size 1) */
    float y = 140.0f;
    draw_tek_string(header_1, 0, y, S1_CELL_W, S1_SX, S1_SY); y += S1_CELL_H;
    draw_tek_string(header_2, 0, y, S1_CELL_W, S1_SX, S1_SY); y += S1_CELL_H * 2.0f;

    /* Formato 1: 74 cpl */
    draw_tek_string(sec1_title, 0, y, S1_CELL_W, S1_SX, S1_SY); y += S1_CELL_H * 1.8f;
    draw_tek_string(sec1_line1, 0, y, S1_CELL_W, S1_SX, S1_SY); y += S1_CELL_H;
    draw_tek_string(sec1_line2, 0, y, S1_CELL_W, S1_SX, S1_SY); y += S1_CELL_H * 2.0f;

    /* Formato 2: 81 cpl */
    draw_tek_string(sec2_title, 0, y, S2_CELL_W, S2_SX, S2_SY); y += S2_CELL_H * 1.8f;
    draw_tek_string(sec2_line1, 0, y, S2_CELL_W, S2_SX, S2_SY); y += S2_CELL_H;
    draw_tek_string(sec2_line2, 0, y, S2_CELL_W, S2_SX, S2_SY); y += S2_CELL_H * 2.0f;

    /* Formato 3: 121 cpl */
    draw_tek_string(sec3_title, 0, y, S3_CELL_W, S3_SX, S3_SY); y += S3_CELL_H * 1.3f;
    draw_tek_string(sec3_line1, 0, y, S3_CELL_W, S3_SX, S3_SY); y += S3_CELL_H;
    draw_tek_string(sec3_line2, 0, y, S3_CELL_W, S3_SX, S3_SY); y += S3_CELL_H * 1.8f;

    /* Formato 4: 133 cpl */
    draw_tek_string(sec4_title, 0, y, S4_CELL_W, S4_SX, S4_SY); y += S4_CELL_H * 1.3f;
    draw_tek_string(sec4_line1, 0, y, S4_CELL_W, S4_SX, S4_SY); y += S4_CELL_H;
    draw_tek_string(sec4_line2, 0, y, S4_CELL_W, S4_SX, S4_SY); y += S4_CELL_H * 2.0f;

    /* Footer APL (Size 4) */
    draw_tek_string(footer, 0, y + 50.0f, S4_CELL_W, S4_SX, S4_SY);

    /* Salvataggio su file PPM P6 */
    printf("[+] Salvataggio file tek_rosetta.ppm...\\n");
    FILE *fp = fopen("tek_rosetta.ppm", "wb");
    if (!fp) {
        perror("[-] Impossibile creare tek_rosetta.ppm");
        return 1;
    }
    fprintf(fp, "P6\\n%d %d\\n255\\n", WIDTH, HEIGHT);
    fwrite(framebuffer, 1, WIDTH * HEIGHT * 3, fp);
    fclose(fp);

    free(framebuffer);
    printf("[+] File tek_rosetta.ppm generato con successo!\\n");
    return 0;
}
"""

with open("/tmp/test_tek_rosetta.c", "w", encoding="utf-8") as f:
    f.write(c_code)

print("[+] Compilazione test con motore vettoriale nativo src/plato_tek_font.c...")
proc_cc = subprocess.run(["clang", "-O3", "-Iinclude", "/tmp/test_tek_rosetta.c", "src/plato_tek_font.c", "-o", "/tmp/tek_rosetta"])
if proc_cc.returncode != 0:
    print(f"[-] Compilazione fallita con codice: {proc_cc.returncode}")
    sys.exit(1)

print("[+] Esecuzione rendering 4096x3120...")
subprocess.run(["/tmp/tek_rosetta"])

print("[+] Conversione ad alta risoluzione in PNG tramite macOS sips...")
subprocess.run(["sips", "-s", "format", "png", "tek_rosetta.ppm", "--out", "tek_rosetta.png"])
if os.path.exists("tek_rosetta.ppm"):
    os.remove("tek_rosetta.ppm")

if os.path.exists("tek_rosetta.png"):
    size_mb = os.path.getsize("tek_rosetta.png") / (1024 * 1024)
    print(f"\\n[OK] IMMAGINE GENERATA: tek_rosetta.png ({size_mb:.2f} MB)")
    print("[*] Esegui: 'open tek_rosetta.png' per visualizzarla al 100% di zoom.")

sys.stdout.flush()
