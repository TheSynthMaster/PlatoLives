#include "plato/plato_optical.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PLASMA_SCALE 4
#define PLASMA_WIDTH (PLATO_WIDTH * PLASMA_SCALE)
#define PLASMA_HEIGHT (PLATO_HEIGHT * PLASMA_SCALE)
#define PLASMA_PIXELS ((size_t)PLASMA_WIDTH * PLASMA_HEIGHT)

#define PLASMA_TILE_LOGICAL 32
#define PLASMA_TILE_SIZE (PLASMA_TILE_LOGICAL * PLASMA_SCALE)
#define PLASMA_TILE_COLS (PLASMA_WIDTH / PLASMA_TILE_SIZE)
#define PLASMA_TILE_ROWS (PLASMA_HEIGHT / PLASMA_TILE_SIZE)
#define PLASMA_TILE_COUNT (PLASMA_TILE_COLS * PLASMA_TILE_ROWS)
#define PLASMA_PARTIAL_LIMIT 180

#define PLASMA_BG_PIXEL 0xFF0E0401u

struct PLATOPlasmaState {
    int displayMode;
    float *gain;
    float *energy;
    float *source;
    float *tmp;
    float *local;
    float *wide;
    uint32_t *crisp;
    uint32_t *logical;
    
    double lastTime;
    double animateUntil;
    
    double decayDuration;
    float decayTau;
    
    double crtDecayDuration;
    float crtDecayTau;
    
    uint8_t dirtySource[PLASMA_TILE_COUNT];
    uint8_t dirtyOutput[PLASMA_TILE_COUNT];
    bool tilesInitialized;
    
    int crtBeamLevel;
    int crtDistortion;
    int plasmaDistortion;
    int phosphorColor;

    /* Motore Fisico Tektronix 4014 (DVBST) */
    float *tek_flare;       /* Energia transitoria del raggio (Transient Flare) */
    float *tek_stored;      /* Carica bistabile del dielettrico (Stored State) */
    double tek_erase_time;  /* Timestamp inizio scarica dielettrica (Erase Flash) */
    double tek_erase_grace_until; /* Deadline Grace Period anti-strobe (1.0s di soppressione) */
    double tek_pacer_last_sec;    /* Timestamp ultimo accredito bit Pacer Tektronix */
    double tek_pacer_credit_bits; /* Credito di bit residuo per disegno progressivo */
};

struct plato_optical {
    struct PLATOPlasmaState *state;
    plato_terminal_t *term;
    bool last_was_full;
};

bool plato_optical_is_full_frame(const plato_optical_t *opt) {
    return opt ? opt->last_was_full : false;
}

static inline float plasmaClamp(float value, float low, float high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static inline uint32_t plasmaRGBA(float red, float green, float blue) {
    uint32_t r = (uint32_t)(plasmaClamp(red, 0.0f, 1.0f) * 255.0f + 0.5f);
    uint32_t g = (uint32_t)(plasmaClamp(green, 0.0f, 1.0f) * 255.0f + 0.5f);
    uint32_t b = (uint32_t)(plasmaClamp(blue, 0.0f, 1.0f) * 255.0f + 0.5f);
    /* Formato BGRA Little-Endian (es. 0xFF006EFF: R=FF, G=6E, B=00) */
    return b | (g << 8) | (r << 16) | (0xFFu << 24);
}

static void plasmaFreeState(struct PLATOPlasmaState *ps) {
    if (!ps) return;
    free(ps->gain); free(ps->energy); free(ps->source); free(ps->tmp);
    free(ps->local); free(ps->wide); free(ps->crisp); free(ps->logical);
    free(ps->tek_flare); free(ps->tek_stored);
    ps->tek_flare = ps->tek_stored = NULL;
    ps->gain = ps->energy = ps->source = ps->tmp = ps->local = ps->wide = NULL;
    ps->crisp = ps->logical = NULL;
    ps->lastTime = 0.0;
    ps->animateUntil = 0.0;
    ps->tilesInitialized = false;
}

static bool plasmaAllocState(struct PLATOPlasmaState *ps) {
    if (!ps) return false;
    if (ps->gain) return true;
    size_t logicalPixels = (size_t)PLATO_WIDTH * PLATO_HEIGHT;
    ps->gain = (float *)calloc(logicalPixels, sizeof(float));
    ps->energy = (float *)calloc(logicalPixels, sizeof(float));
    ps->source = (float *)calloc(PLASMA_PIXELS, sizeof(float));
    ps->tmp = (float *)calloc(PLASMA_PIXELS, sizeof(float));
    ps->local = (float *)calloc(PLASMA_PIXELS, sizeof(float));
    ps->wide = (float *)calloc(PLASMA_PIXELS, sizeof(float));
    ps->crisp = (uint32_t *)calloc(PLASMA_PIXELS, sizeof(uint32_t));
    ps->logical = (uint32_t *)calloc(logicalPixels, sizeof(uint32_t));
    ps->tek_flare = (float *)calloc(PLASMA_PIXELS, sizeof(float));
    ps->tek_stored = (float *)calloc(PLASMA_PIXELS, sizeof(float));
    ps->tek_erase_time = 0.0;
    ps->tek_erase_grace_until = 0.0;
    ps->tek_pacer_last_sec = 0.0;
    ps->tek_pacer_credit_bits = 0.0;
    if (!ps->gain || !ps->energy || !ps->source || !ps->tmp ||
        !ps->local || !ps->wide || !ps->crisp || !ps->logical || !ps->tek_flare || !ps->tek_stored) {
        plasmaFreeState(ps);
        return false;
    }
    /* Generazione gain procedurale C11-compliant (rand) */
    for (size_t i = 0; i < logicalPixels; i++) {
        float randomValue = (float)rand() / (float)RAND_MAX;
        ps->gain[i] = 0.980f + 0.040f * randomValue;
    }
    return true;
}

static void plasmaExpandCrisp(const uint32_t *logical, uint32_t *expanded) {
    for (int y = 0; y < PLATO_HEIGHT; y++) {
        for (int sy = 0; sy < PLASMA_SCALE; sy++) {
            uint32_t *row = expanded + (size_t)(y * PLASMA_SCALE + sy) * PLASMA_WIDTH;
            for (int x = 0; x < PLATO_WIDTH; x++) {
                uint32_t pixel = logical[(size_t)y * PLATO_WIDTH + x];
                int base = x * PLASMA_SCALE;
                for (int sx = 0; sx < PLASMA_SCALE; sx++) row[base + sx] = pixel;
            }
        }
    }
}

static void plasmaBlurRows(const float *source, float *destination, int width, int height, int radius) {
    const int span = radius * 2 + 1;
    const float invSpan = 1.0f / (float)span;
    for (int y = 0; y < height; y++) {
        const float *sourceRow = source + (size_t)y * width;
        float *destinationRow = destination + (size_t)y * width;
        float sum = sourceRow[0] * (float)(radius + 1);
        for (int x = 1; x <= radius; x++) sum += sourceRow[x];

        int x = 0;
        int leftLimit = radius < width ? radius : width;
        for (; x < leftLimit; x++) {
            destinationRow[x] = sum * invSpan;
            sum += sourceRow[x + radius + 1] - sourceRow[0];
        }

        int midLimit = (width - 1 - radius > x) ? (width - 1 - radius) : x;
        for (; x < midLimit; x++) {
            destinationRow[x] = sum * invSpan;
            sum += sourceRow[x + radius + 1] - sourceRow[x - radius];
        }

        for (; x < width; x++) {
            destinationRow[x] = sum * invSpan;
            int removeX = x - radius;
            sum += sourceRow[width - 1] - sourceRow[removeX];
        }
    }
}

static void plasmaBlurCols(const float *source, float *destination, int width, int height, int radius) {
    const int span = radius * 2 + 1;
    const float invSpan = 1.0f / (float)span;
    float colSum[PLASMA_WIDTH];

    for (int x = 0; x < width; x++) {
        colSum[x] = source[x] * (float)(radius + 1);
    }
    for (int sy = 1; sy <= radius; sy++) {
        const float *row = source + (size_t)sy * width;
        for (int x = 0; x < width; x++) {
            colSum[x] += row[x];
        }
    }

    float *destRow0 = destination;
    for (int x = 0; x < width; x++) {
        destRow0[x] = colSum[x] * invSpan;
    }

    for (int y = 1; y < height; y++) {
        int addY = y + radius;
        if (addY >= height) addY = height - 1;
        int removeY = y - 1 - radius;
        if (removeY < 0) removeY = 0;

        const float *addRow = source + (size_t)addY * width;
        const float *remRow = source + (size_t)removeY * width;
        float *destRow = destination + (size_t)y * width;

        for (int x = 0; x < width; x++) {
            colSum[x] += addRow[x] - remRow[x];
            destRow[x] = colSum[x] * invSpan;
        }
    }
}

static void plasmaBoxBlur(const float *source, float *temporary, float *destination, int radius) {
    plasmaBlurRows(source, temporary, PLASMA_WIDTH, PLASMA_HEIGHT, radius);
    plasmaBlurCols(temporary, destination, PLASMA_WIDTH, PLASMA_HEIGHT, radius);
}

static void plasmaBoxBlurTile(const float *source, float *temporary, float *destination, int radius, int tileX, int tileY) {
    const int width = PLASMA_WIDTH, height = PLASMA_HEIGHT, span = radius * 2 + 1;
    const float invSpan = 1.0f / (float)span;
    int x0 = tileX * PLASMA_TILE_SIZE, x1 = x0 + PLASMA_TILE_SIZE;
    int y0 = tileY * PLASMA_TILE_SIZE, y1 = y0 + PLASMA_TILE_SIZE;
    int haloY0 = y0 - radius; if (haloY0 < 0) haloY0 = 0;
    int haloY1 = y1 + radius; if (haloY1 > height) haloY1 = height;

    bool hasHorizontalBorder = (x0 - 1 - radius < 0) || (x1 + radius >= width);

    for (int y = haloY0; y < haloY1; y++) {
        const float *sourceRow = source + (size_t)y * width;
        float *temporaryRow = temporary + (size_t)y * width;

        float sum = 0.0f;
        for (int k = -radius; k <= radius; k++) {
            int sx = x0 + k;
            if (sx < 0) sx = 0;
            else if (sx >= width) sx = width - 1;
            sum += sourceRow[sx];
        }
        temporaryRow[x0] = sum * invSpan;

        if (!hasHorizontalBorder) {
            for (int x = x0 + 1; x < x1; x++) {
                sum += sourceRow[x + radius] - sourceRow[x - 1 - radius];
                temporaryRow[x] = sum * invSpan;
            }
        } else {
            for (int x = x0 + 1; x < x1; x++) {
                int addX = x + radius;
                if (addX >= width) addX = width - 1;
                int removeX = x - 1 - radius;
                if (removeX < 0) removeX = 0;
                sum += sourceRow[addX] - sourceRow[removeX];
                temporaryRow[x] = sum * invSpan;
            }
        }
    }

    float colSum[PLASMA_TILE_SIZE];
    for (int i = 0; i < PLASMA_TILE_SIZE; i++) colSum[i] = 0.0f;
    for (int k = -radius; k <= radius; k++) {
        int sy = y0 + k;
        if (sy < 0) sy = 0;
        else if (sy >= height) sy = height - 1;
        const float *row = temporary + (size_t)sy * width;
        for (int i = 0; i < PLASMA_TILE_SIZE; i++) colSum[i] += row[x0 + i];
    }

    float *destRow0 = destination + (size_t)y0 * width;
    for (int i = 0; i < PLASMA_TILE_SIZE; i++) destRow0[x0 + i] = colSum[i] * invSpan;

    for (int y = y0 + 1; y < y1; y++) {
        int addY = y + radius;
        if (addY >= height) addY = height - 1;
        int removeY = y - 1 - radius;
        if (removeY < 0) removeY = 0;
        const float *addRow = temporary + (size_t)addY * width;
        const float *remRow = temporary + (size_t)removeY * width;
        float *destRow = destination + (size_t)y * width;
        for (int i = 0; i < PLASMA_TILE_SIZE; i++) {
            colSum[i] += addRow[x0 + i] - remRow[x0 + i];
            destRow[x0 + i] = colSum[i] * invSpan;
        }
    }
}

static void plasmaComposeTile(plato_optical_t *opt, uint32_t *output, int tileX, int tileY) {
    struct PLATOPlasmaState *p = opt->state;
    int x0 = tileX * PLASMA_TILE_SIZE, x1 = x0 + PLASMA_TILE_SIZE;
    int y0 = tileY * PLASMA_TILE_SIZE, y1 = y0 + PLASMA_TILE_SIZE;

    const float half_dim = 1024.0f;
    const float inv_half_dim = 1.0f / 1024.0f;
    const float k_cyl_x = 0.018f;
    const float k_bar_x = 0.018f;
    const float k_bar_y = 0.012f;
    int distMode = p->plasmaDistortion;

    for (int y = y0; y < y1; y++) {
        float v_norm = ((float)y - half_dim) * inv_half_dim;
        float v2 = v_norm * v_norm;
        size_t rowBase = (size_t)y * PLASMA_WIDTH;

        for (int x = x0; x < x1; x++) {
            size_t i = rowBase + (size_t)x;
            int src_x = x, src_y = y;

            if (distMode == 2) {
                float u_norm = ((float)x - half_dim) * inv_half_dim;
                float xs = half_dim + (u_norm * (1.0f + v2 * k_cyl_x)) * half_dim;
                if (xs < 0.0f || xs >= 2047.0f) { output[i] = 0xFF000000u; continue; }
                src_x = (int)xs; src_y = y;
            } else if (distMode == 1) {
                float u_norm = ((float)x - half_dim) * inv_half_dim;
                float u2 = u_norm * u_norm;
                float xs = half_dim + (u_norm * (1.0f + v2 * k_bar_x)) * half_dim;
                float ys = half_dim + (v_norm * (1.0f + u2 * k_bar_y)) * half_dim;
                if (xs < 0.0f || xs >= 2047.0f || ys < 0.0f || ys >= 2047.0f) { output[i] = 0xFF000000u; continue; }
                src_x = (int)xs; src_y = (int)ys;
            }

            size_t src_i = (size_t)src_y * PLASMA_WIDTH + (size_t)src_x;
            float core = p->source[src_i], localGlow = p->local[src_i], wideGlow = p->wide[src_i];

            if ((core + localGlow + wideGlow) < 0.001f) {
                output[i] = (p->phosphorColor == 1) ? 0xFF010E02u : ((p->phosphorColor == 2) ? 0xFF0E0700u : ((p->phosphorColor == 3) ? 0xFF0A0A0Au : PLASMA_BG_PIXEL));
                continue;
            }

            float neighborhood = plasmaClamp((localGlow - 0.25f) * 2.50f, 0.0f, 1.0f);
            neighborhood = neighborhood * neighborhood * (3.0f - 2.0f * neighborhood);
            float fusedCore = core;
            if (core > 0.05f) fusedCore += 0.82f * neighborhood * (1.0f - core);
            float density = plasmaClamp(fusedCore + 0.62f * localGlow + 0.22f * wideGlow, 0.0f, 1.5f);
            float hot = plasmaClamp((density - 0.92f) * 2.0f, 0.0f, 1.0f);
            float red, green, blue;

            if (p->phosphorColor == 1) {
                /* Calibrated Green P31 Gas Discharge (Phosphor simulation) */
                red   = 0.005f + 0.15f * fusedCore + 0.020f * localGlow + 0.010f * wideGlow + 0.35f * hot;
                green = 0.035f + 0.96f * fusedCore + 0.65f * localGlow + 0.32f * wideGlow + 0.15f * hot;
                blue  = 0.005f + 0.08f * fusedCore + 0.015f * localGlow + 0.008f * wideGlow + 0.30f * hot;
            } else if (p->phosphorColor == 2) {
                /* Calibrated Amber P40 Gas Discharge: carica, densa e calda ad alta emissione */
                red   = 0.050f + 1.12f * fusedCore + 0.72f * localGlow + 0.38f * wideGlow + 0.18f * hot;
                green = 0.024f + 0.82f * fusedCore + 0.48f * localGlow + 0.24f * wideGlow + 0.28f * hot;
                blue  = 0.002f + 0.04f * fusedCore + 0.010f * localGlow + 0.005f * wideGlow + 0.08f * hot;
            } else if (p->phosphorColor == 3) {
                /* Calibrated White P4 Gas Discharge: brillante, densa, incandescente ed energica */
                red   = 0.025f + 1.10f * fusedCore + 0.68f * localGlow + 0.34f * wideGlow + 0.30f * hot;
                green = 0.025f + 1.12f * fusedCore + 0.70f * localGlow + 0.35f * wideGlow + 0.30f * hot;
                blue  = 0.028f + 1.16f * fusedCore + 0.74f * localGlow + 0.37f * wideGlow + 0.30f * hot;
            } else {
                /* Authentic Neon Orange Gas Discharge */
                red   = 0.055f + 0.95f * fusedCore + 0.58f * localGlow + 0.30f * wideGlow;
                green = 0.016f + 0.31f * fusedCore + 0.060f * localGlow + 0.020f * wideGlow + 0.08f * hot;
                blue  = 0.004f + 0.018f * fusedCore + 0.003f * localGlow + 0.010f * hot;
            }
            output[i] = plasmaRGBA(red, green, blue);
        }
    }
}

static unsigned plasmaBuildCellSurface(plato_optical_t *opt, float deltaTime) {
    plato_terminal_t *term = opt->term;
    struct PLATOPlasmaState *p = opt->state;
    static const float cellProfile[PLASMA_SCALE][PLASMA_SCALE] = {
        { 0.65f, 0.82f, 0.82f, 0.65f },
        { 0.82f, 1.00f, 1.00f, 0.82f },
        { 0.82f, 1.00f, 1.00f, 0.82f },
        { 0.65f, 0.82f, 0.82f, 0.65f }
    };
    const float decay = (p->decayTau > 0.001f) ? expf(-deltaTime / p->decayTau) : 0.0f;
    memset(p->dirtySource, 0, sizeof(p->dirtySource));
    memset(p->dirtyOutput, 0, sizeof(p->dirtyOutput));
    
    for (int logicalY = 0; logicalY < PLATO_HEIGHT; logicalY++) {
        const uint8_t *fbRow = term->fb.pixels[logicalY];
        size_t rowLogicalBase = (size_t)logicalY * PLATO_WIDTH;
        for (int byteIdx = 0; byteIdx < PLATO_WIDTH / 8; byteIdx++) {
            uint8_t byteVal = fbRow[byteIdx];
            int baseX = byteIdx * 8;
            size_t logicalIndex = rowLogicalBase + (size_t)baseX;

            if (p->tilesInitialized && byteVal == 0) {
                bool hasEnergy = false;
                for (int b = 0; b < 8; b++) {
                    if (p->energy[logicalIndex + b] > 0.0f) {
                        hasEnergy = true;
                        break;
                    }
                }
                if (!hasEnergy) continue;
            }

            for (int bit = 0; bit < 8; bit++) {
                int logicalX = baseX + bit;
                size_t idx = logicalIndex + (size_t)bit;
                float oldEnergy = p->energy[idx];
                bool pixelOn = (byteVal & (0x80 >> bit)) != 0;
                float newEnergy = pixelOn ? p->gain[idx] : oldEnergy * decay;
                if (newEnergy < 0.0001f) newEnergy = 0.0f;
                
                if (!p->tilesInitialized || newEnergy != oldEnergy) {
                    p->energy[idx] = newEnergy;
                    int px = logicalX * PLASMA_SCALE;
                    int py = logicalY * PLASMA_SCALE;
                    for (int sy = 0; sy < PLASMA_SCALE; sy++) {
                        float *row = p->source + (size_t)(py + sy) * PLASMA_WIDTH + px;
                        for (int sx = 0; sx < PLASMA_SCALE; sx++) row[sx] = newEnergy * cellProfile[sy][sx];
                    }
                    int tileX = logicalX / PLASMA_TILE_LOGICAL;
                    int tileY = logicalY / PLASMA_TILE_LOGICAL;
                    p->dirtySource[tileY * PLASMA_TILE_COLS + tileX] = 1;
                    p->dirtyOutput[tileY * PLASMA_TILE_COLS + tileX] = 1;

                    int intraX = logicalX % PLASMA_TILE_LOGICAL;
                    int intraY = logicalY % PLASMA_TILE_LOGICAL;
                    int dxMin = (intraX < 3) ? -1 : 0;
                    int dxMax = (intraX >= PLASMA_TILE_LOGICAL - 3) ? 1 : 0;
                    int dyMin = (intraY < 3) ? -1 : 0;
                    int dyMax = (intraY >= PLASMA_TILE_LOGICAL - 3) ? 1 : 0;

                    for (int dy = dyMin; dy <= dyMax; dy++) {
                        for (int dx = dxMin; dx <= dxMax; dx++) {
                            int nx = tileX + dx, ny = tileY + dy;
                            if (nx >= 0 && nx < PLASMA_TILE_COLS && ny >= 0 && ny < PLASMA_TILE_ROWS) {
                                p->dirtyOutput[ny * PLASMA_TILE_COLS + nx] = 1;
                            }
                        }
                    }
                }
            }
        }
    }
    if (!p->tilesInitialized) {
        memset(p->dirtySource, 1, sizeof(p->dirtySource));
        memset(p->dirtyOutput, 1, sizeof(p->dirtyOutput));
        p->tilesInitialized = true;
    }
    unsigned count = 0;
    for (int i = 0; i < PLASMA_TILE_COUNT; i++) if (p->dirtyOutput[i]) count++;
    return count;
}

static unsigned crtBuildCellSurface(plato_optical_t *opt, float deltaTime) {
    plato_terminal_t *term = opt->term;
    struct PLATOPlasmaState *p = opt->state;
    float tau = (p->crtDecayTau > 0.001f) ? p->crtDecayTau : (float)(0.020 / 10.0);
    const float decay = expf(-deltaTime / tau);

    memset(p->dirtySource, 0, sizeof(p->dirtySource));
    memset(p->dirtyOutput, 0, sizeof(p->dirtyOutput));

    for (int logicalY = 0; logicalY < PLATO_HEIGHT; logicalY++) {
        const uint32_t *colorRow = term->fb.colors[logicalY];
        uint32_t *phosphorRow = p->logical + (size_t)logicalY * PLATO_WIDTH;
        size_t rowLogicalBase = (size_t)logicalY * PLATO_WIDTH;

        for (int logicalX = 0; logicalX < PLATO_WIDTH; logicalX++) {
            uint32_t bgra = colorRow[logicalX];
            size_t idx = rowLogicalBase + (size_t)logicalX;

            float oldEnergy = p->energy[idx];
            float newEnergy = 0.0f;

            if ((bgra & 0x00FFFFFFu) != 0) {
                phosphorRow[logicalX] = bgra;
                newEnergy = 1.0f;
            } else if (oldEnergy > 0.0001f) {
                newEnergy = oldEnergy * decay;
                if (newEnergy < 0.0001f) {
                    newEnergy = 0.0f;
                    phosphorRow[logicalX] = 0xFF000000u;
                }
            } else {
                newEnergy = 0.0f;
                phosphorRow[logicalX] = 0xFF000000u;
            }

            if (!p->tilesInitialized || newEnergy != oldEnergy) {
                p->energy[idx] = newEnergy;
                int px = logicalX * PLASMA_SCALE;
                int py = logicalY * PLASMA_SCALE;

                for (int sy = 0; sy < PLASMA_SCALE; sy++) {
                    float *row = p->source + (size_t)(py + sy) * PLASMA_WIDTH + px;
                    for (int sx = 0; sx < PLASMA_SCALE; sx++) {
                        row[sx] = newEnergy;
                    }
                }

                int tileX = logicalX / PLASMA_TILE_LOGICAL;
                int tileY = logicalY / PLASMA_TILE_LOGICAL;
                p->dirtySource[tileY * PLASMA_TILE_COLS + tileX] = 1;
                p->dirtyOutput[tileY * PLASMA_TILE_COLS + tileX] = 1;

                int intraX = logicalX % PLASMA_TILE_LOGICAL;
                int intraY = logicalY % PLASMA_TILE_LOGICAL;
                int dxMin = (intraX < 2) ? -1 : 0;
                int dxMax = (intraX >= PLASMA_TILE_LOGICAL - 2) ? 1 : 0;
                int dyMin = (intraY < 2) ? -1 : 0;
                int dyMax = (intraY >= PLASMA_TILE_LOGICAL - 2) ? 1 : 0;

                for (int dy = dyMin; dy <= dyMax; dy++) {
                    for (int dx = dxMin; dx <= dxMax; dx++) {
                        int nx = tileX + dx, ny = tileY + dy;
                        if (nx >= 0 && nx < PLASMA_TILE_COLS && ny >= 0 && ny < PLASMA_TILE_ROWS) {
                            p->dirtyOutput[ny * PLASMA_TILE_COLS + nx] = 1;
                        }
                    }
                }
            }
        }
    }

    if (!p->tilesInitialized) {
        memset(p->dirtySource, 1, sizeof(p->dirtySource));
        memset(p->dirtyOutput, 1, sizeof(p->dirtyOutput));
        p->tilesInitialized = true;
    }

    unsigned count = 0;
    for (int i = 0; i < PLASMA_TILE_COUNT; i++) if (p->dirtyOutput[i]) count++;
    return count;
}

typedef struct {
    float w_3tap[4][3];
    float scanline[4];
    float bloom_amt;
    float gain;
} CRTBeamProfileParams;

static const CRTBeamProfileParams kCRTProfiles[3] = {
    {
        .w_3tap = {
            { 0.3474f, 0.6440f, 0.0086f },
            { 0.1305f, 0.8315f, 0.0380f },
            { 0.0380f, 0.8315f, 0.1305f },
            { 0.0086f, 0.6440f, 0.3474f }
        },
        .scanline = { 1.00f, 1.05f, 1.00f, 0.78f },
        .bloom_amt = 0.32f,
        .gain = 1.22f
    },
    {
        .w_3tap = {
            { 0.4000f, 0.5800f, 0.0200f },
            { 0.1800f, 0.7600f, 0.0600f },
            { 0.0600f, 0.7600f, 0.1800f },
            { 0.0200f, 0.5800f, 0.4000f }
        },
        .scanline = { 1.00f, 1.05f, 1.00f, 0.80f },
        .bloom_amt = 0.38f,
        .gain = 1.24f
    },
    {
        .w_3tap = {
            { 0.4400f, 0.5200f, 0.0400f },
            { 0.2200f, 0.7000f, 0.0800f },
            { 0.0800f, 0.7000f, 0.2200f },
            { 0.0400f, 0.5200f, 0.4400f }
        },
        .scanline = { 1.00f, 1.05f, 1.00f, 0.82f },
        .bloom_amt = 0.45f,
        .gain = 1.26f
    }
};

static void crtComposeTile(plato_optical_t *opt, uint32_t *output, int tileX, int tileY) {
    struct PLATOPlasmaState *p = opt->state;

    int x0 = tileX * PLASMA_TILE_SIZE, x1 = x0 + PLASMA_TILE_SIZE;
    int y0 = tileY * PLASMA_TILE_SIZE, y1 = y0 + PLASMA_TILE_SIZE;

    int profileIdx = p->crtBeamLevel;
    if (profileIdx < 0 || profileIdx > 2) profileIdx = 0;
    const CRTBeamProfileParams *prof = &kCRTProfiles[profileIdx];

    static const float mask_tbl[4][3] = {
        { 1.08f, 0.94f, 0.94f },
        { 0.94f, 1.08f, 0.94f },
        { 0.94f, 0.94f, 1.08f },
        { 0.88f, 0.88f, 0.88f }
    };

    const float half_dim = 1024.0f;
    const float inv_half_dim = 1.0f / 1024.0f;
    const float k_cyl_x = 0.018f;
    const float k_bar_x = 0.018f;
    const float k_bar_y = 0.012f;
    int distMode = p->crtDistortion;

    for (int y = y0; y < y1; y++) {
        float v_norm = ((float)y - half_dim) * inv_half_dim;
        float v2 = v_norm * v_norm;
        size_t rowBase = (size_t)y * PLASMA_WIDTH;

        for (int x = x0; x < x1; x++) {
            size_t i = rowBase + (size_t)x;
            int src_x = x, src_y = y;

            if (distMode == 2) {
                float u_norm = ((float)x - half_dim) * inv_half_dim;
                float xs = half_dim + (u_norm * (1.0f + v2 * k_cyl_x)) * half_dim;
                if (xs < 0.0f || xs >= 2047.0f) { output[i] = 0xFF000000u; continue; }
                src_x = (int)xs; src_y = y;
            } else if (distMode == 1) {
                float u_norm = ((float)x - half_dim) * inv_half_dim;
                float u2 = u_norm * u_norm;
                float xs = half_dim + (u_norm * (1.0f + v2 * k_bar_x)) * half_dim;
                float ys = half_dim + (v_norm * (1.0f + u2 * k_bar_y)) * half_dim;
                if (xs < 0.0f || xs >= 2047.0f || ys < 0.0f || ys >= 2047.0f) { output[i] = 0xFF000000u; continue; }
                src_x = (int)xs; src_y = (int)ys;
            }

            int logicalY = src_y / PLASMA_SCALE;
            int sy = src_y % PLASMA_SCALE;

            float v_beam = prof->scanline[sy];
            float wy_p = prof->w_3tap[sy][0];
            float wy_c = prof->w_3tap[sy][1];
            float wy_n = prof->w_3tap[sy][2];

            int prevY = logicalY > 0 ? logicalY - 1 : 0;
            int nextY = logicalY < PLATO_HEIGHT - 1 ? logicalY + 1 : PLATO_HEIGHT - 1;

            const uint32_t *colorRow_prev = p->logical + (size_t)prevY * PLATO_WIDTH;
            const uint32_t *colorRow_curr = p->logical + (size_t)logicalY * PLATO_WIDTH;
            const uint32_t *colorRow_next = p->logical + (size_t)nextY * PLATO_WIDTH;

            const float *energyRow_prev = p->energy + (size_t)prevY * PLATO_WIDTH;
            const float *energyRow_curr = p->energy + (size_t)logicalY * PLATO_WIDTH;
            const float *energyRow_next = p->energy + (size_t)nextY * PLATO_WIDTH;

            int logicalX = src_x / PLASMA_SCALE;
            int sx = src_x % PLASMA_SCALE;

            float wx_p = prof->w_3tap[sx][0];
            float wx_c = prof->w_3tap[sx][1];
            float wx_n = prof->w_3tap[sx][2];

            int prevX = logicalX > 0 ? logicalX - 1 : 0;
            int nextX = logicalX < PLATO_WIDTH - 1 ? logicalX + 1 : PLATO_WIDTH - 1;

            size_t src_i = (size_t)src_y * PLASMA_WIDTH + (size_t)src_x;
            float glow = 0.40f * p->local[src_i] + 0.60f * p->wide[src_i];

            uint32_t c_pp = colorRow_prev[prevX], c_pc = colorRow_prev[logicalX], c_pn = colorRow_prev[nextX];
            uint32_t c_cp = colorRow_curr[prevX], c_cc = colorRow_curr[logicalX], c_cn = colorRow_curr[nextX];
            uint32_t c_np = colorRow_next[prevX], c_nc = colorRow_next[logicalX], c_nn = colorRow_next[nextX];

            float e_pp = energyRow_prev[prevX], e_pc = energyRow_prev[logicalX], e_pn = energyRow_prev[nextX];
            float e_cp = energyRow_curr[prevX], e_cc = energyRow_curr[logicalX], e_cn = energyRow_curr[nextX];
            float e_np = energyRow_next[prevX], e_nc = energyRow_next[logicalX], e_nn = energyRow_next[nextX];

            if ((e_pp + e_pc + e_pn + e_cp + e_cc + e_cn + e_np + e_nc + e_nn) < 0.0001f && glow < 0.001f) {
                output[i] = 0xFF000000u;
                continue;
            }

            float r_p = (((float)((c_pp >> 16) & 0xFFu) * e_pp) * wx_p + ((float)((c_pc >> 16) & 0xFFu) * e_pc) * wx_c + ((float)((c_pn >> 16) & 0xFFu) * e_pn) * wx_n) * (1.0f / 255.0f);
            float g_p = (((float)((c_pp >> 8)  & 0xFFu) * e_pp) * wx_p + ((float)((c_pc >> 8)  & 0xFFu) * e_pc) * wx_c + ((float)((c_pn >> 8)  & 0xFFu) * e_pn) * wx_n) * (1.0f / 255.0f);
            float b_p = (((float)(c_pp         & 0xFFu) * e_pp) * wx_p + ((float)(c_pc         & 0xFFu) * e_pc) * wx_c + ((float)(c_pn         & 0xFFu) * e_pn) * wx_n) * (1.0f / 255.0f);

            float r_c = (((float)((c_cp >> 16) & 0xFFu) * e_cp) * wx_p + ((float)((c_cc >> 16) & 0xFFu) * e_cc) * wx_c + ((float)((c_cn >> 16) & 0xFFu) * e_cn) * wx_n) * (1.0f / 255.0f);
            float g_c = (((float)((c_cp >> 8)  & 0xFFu) * e_cp) * wx_p + ((float)((c_cc >> 8)  & 0xFFu) * e_cc) * wx_c + ((float)((c_cn >> 8)  & 0xFFu) * e_cn) * wx_n) * (1.0f / 255.0f);
            float b_c = (((float)(c_cp         & 0xFFu) * e_cp) * wx_p + ((float)(c_cc         & 0xFFu) * e_cc) * wx_c + ((float)(c_cn         & 0xFFu) * e_cn) * wx_n) * (1.0f / 255.0f);

            float r_n = (((float)((c_np >> 16) & 0xFFu) * e_np) * wx_p + ((float)((c_nc >> 16) & 0xFFu) * e_nc) * wx_c + ((float)((c_nn >> 16) & 0xFFu) * e_nn) * wx_n) * (1.0f / 255.0f);
            float g_n = (((float)((c_np >> 8)  & 0xFFu) * e_np) * wx_p + ((float)((c_nc >> 8)  & 0xFFu) * e_nc) * wx_c + ((float)((c_nn >> 8)  & 0xFFu) * e_nn) * wx_n) * (1.0f / 255.0f);
            float b_n = (((float)(c_np         & 0xFFu) * e_np) * wx_p + ((float)(c_nc         & 0xFFu) * e_nc) * wx_c + ((float)(c_nn         & 0xFFu) * e_nn) * wx_n) * (1.0f / 255.0f);

            float r_beam = r_p * wy_p + r_c * wy_c + r_n * wy_n;
            float g_beam = g_p * wy_p + g_c * wy_c + g_n * wy_n;
            float b_beam = b_p * wy_p + b_c * wy_c + b_n * wy_n;

            float r_avg = (r_p + r_c + r_n) * 0.3333f;
            float g_avg = (g_p + g_c + g_n) * 0.3333f;
            float b_avg = (b_p + b_c + b_n) * 0.3333f;
            float sum_avg = r_avg + g_avg + b_avg;

            float m_r = mask_tbl[sx][0];
            float m_g = mask_tbl[sx][1];
            float m_b = mask_tbl[sx][2];

            float r_b = r_beam, g_b = g_beam, b_b = b_beam;
            float mask_r = m_r, mask_g = m_g, mask_b = m_b;
            float chroma_r = 0.0f, chroma_g = 0.0f, chroma_b = 0.0f;

            if (p->phosphorColor != 0) {
                /* Modalita' Monocromatica Pura CRT: fascio intenso ad altissima saturazione */
                float mono_p = e_pp * wx_p + e_pc * wx_c + e_pn * wx_n;
                float mono_c = e_cp * wx_p + e_cc * wx_c + e_cn * wx_n;
                float mono_n = e_np * wx_p + e_nc * wx_c + e_nn * wx_n;
                float mono_beam = mono_p * wy_p + mono_c * wy_c + mono_n * wy_n;
                float mono_sum_avg = (mono_p + mono_c + mono_n) * 0.3333f;

                /* Fosforo uniforme e puro senza la griglia a fori RGB della shadow mask */
                float m_mono = 0.96f + 0.04f * ((m_r + m_g + m_b) * 0.3333f);
                mask_r = m_mono; mask_g = m_mono; mask_b = m_mono;

                float bloom_energy = (mono_sum_avg > 0.001f) ? plasmaClamp(mono_sum_avg * 1.5f, 0.0f, 1.20f) : 0.0f;

                if (p->phosphorColor == 1) {
                    /* Green P31: Verde smeraldo puro, intenso ed elettrico (zero desaturazione) */
                    r_b = mono_beam * 0.04f;
                    g_b = mono_beam * 1.45f;
                    b_b = mono_beam * 0.06f;
                    chroma_r = 0.02f * bloom_energy;
                    chroma_g = 1.00f * bloom_energy;
                    chroma_b = 0.04f * bloom_energy;
                } else if (p->phosphorColor == 2) {
                    /* Amber P40: Ambra dorata radiosa, calda, viva e penetrante */
                    r_b = mono_beam * 1.65f;
                    g_b = mono_beam * 0.95f;
                    b_b = mono_beam * 0.02f;
                    chroma_r = 1.15f * bloom_energy;
                    chroma_g = 0.65f * bloom_energy;
                    chroma_b = 0.01f * bloom_energy;
                } else {
                    /* White P4: Bianco "paper-white" brillante, cristallino ed incisivo */
                    r_b = mono_beam * 1.55f;
                    g_b = mono_beam * 1.58f;
                    b_b = mono_beam * 1.62f;
                    chroma_r = 0.85f * bloom_energy;
                    chroma_g = 0.88f * bloom_energy;
                    chroma_b = 0.92f * bloom_energy;
                }
            } else {
                /* Authentic Color CRT Shadow Mask Triad */
                chroma_r = (sum_avg > 0.001f) ? (r_avg / sum_avg) : 0.0f;
                chroma_g = (sum_avg > 0.001f) ? (g_avg / sum_avg) : 0.0f;
                chroma_b = (sum_avg > 0.001f) ? (b_avg / sum_avg) : 0.0f;
            }

            float bloom_val = glow * prof->bloom_amt * 1.5f;
            float r_final = (r_b * mask_r * v_beam) + (chroma_r * bloom_val);
            float g_final = (g_b * mask_g * v_beam) + (chroma_g * bloom_val);
            float b_final = (b_b * mask_b * v_beam) + (chroma_b * bloom_val);

            r_final *= prof->gain;
            g_final *= prof->gain;
            b_final *= prof->gain;

            float max_c = fmaxf(r_final, fmaxf(g_final, b_final));
            if (max_c > 1.0f) {
                float boost = (max_c - 1.0f) * 0.20f;
                if (p->phosphorColor == 1) {
                    /* Il boost P31 amplifica solo il verde puro (INALTERATO) */
                    g_final += boost;
                } else if (p->phosphorColor == 2) {
                    /* Il boost P40 amplifica lo spettro ambra radioso */
                    r_final += boost * 1.15f;
                    g_final += boost * 0.65f;
                } else {
                    /* Bianco o Color CRT: boost radioso a spettro completo */
                    r_final += boost * 1.10f;
                    g_final += boost * 1.10f;
                    b_final += boost * 1.10f;
                }
            }

            output[i] = plasmaRGBA(plasmaClamp(r_final, 0.0f, 1.0f),
                                   plasmaClamp(g_final, 0.0f, 1.0f),
                                   plasmaClamp(b_final, 0.0f, 1.0f));
        }
    }
}


/* ========================================================================= */
/* TEKTRONIX 4014 DVBST BEAM INTEGRATOR (Spazio Continuo 2048x2048)          */
/* ========================================================================= */

static void plato_tek_splat_point(float *flare, float *stored, int width, int height,
                                  float x, float y, float intensity, float sigma, float stored_ratio) {
    int r = (int)ceilf(sigma * 3.0f);
    int x0 = (int)floorf(x - r);
    int x1 = (int)ceilf(x + r);
    int y0 = (int)floorf(y - r);
    int y1 = (int)ceilf(y + r);

    if (x0 < 0) x0 = 0;
    if (x1 >= width) x1 = width - 1;
    if (y0 < 0) y0 = 0;
    if (y1 >= height) y1 = height - 1;

    float inv_two_sig_sq = 1.0f / (2.0f * sigma * sigma);

    for (int py = y0; py <= y1; py++) {
        float dy = (float)py - y;
        float dy2 = dy * dy;
        float *row_f = flare + py * width;
        float *row_s = stored + py * width;

        for (int px = x0; px <= x1; px++) {
            float dx = (float)px - x;
            float d2 = dx * dx + dy2;
            float w = expf(-d2 * inv_two_sig_sq);
            float val = intensity * w;

            row_f[px] += val;
            float s_val = row_s[px] + val * stored_ratio;
            row_s[px] = (s_val > 1.0f) ? 1.0f : s_val;
        }
    }
}

static void plato_tek_splat_line(float *flare, float *stored, int width, int height,
                                 float x0, float y0, float x1, float y1) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float dist = sqrtf(dx * dx + dy * dy);

    if (dist < 0.5f) {
        /* Punto isolato con Dwell spot */
        plato_tek_splat_point(flare, stored, width, height, x0, y0, 1.4f, 1.5f, 0.85f);
        return;
    }

    /* 1. Dwell Time ai vertici: sovraesposizione all'inizio e alla fine del vettore */
    plato_tek_splat_point(flare, stored, width, height, x0, y0, 0.9f, 1.6f, 0.7f);
    plato_tek_splat_point(flare, stored, width, height, x1, y1, 0.9f, 1.6f, 0.7f);

    /* 2. Tracciamento continuo del raggio sub-pixel lungo il vettore */
    float step = 0.8f;
    int steps = (int)ceilf(dist / step);
    float inv_steps = 1.0f / (float)steps;
    float x_step = dx * inv_steps;
    float y_step = dy * inv_steps;

    float cx = x0;
    float cy = y0;
    const float sigma = 1.0f;
    const float inv_two_sig_sq = 1.0f / (2.0f * sigma * sigma);
    const float intensity = 0.45f;
    const int r = 2;

    for (int i = 0; i <= steps; i++) {
        int px_min = (int)(cx - r);
        int px_max = (int)(cx + r);
        int py_min = (int)(cy - r);
        int py_max = (int)(cy + r);

        if (px_min < 0) px_min = 0;
        if (px_max >= width) px_max = width - 1;
        if (py_min < 0) py_min = 0;
        if (py_max >= height) py_max = height - 1;

        for (int py = py_min; py <= py_max; py++) {
            float d_y = (float)py - cy;
            float d_y2 = d_y * d_y;
            float *row_f = flare + py * width;
            float *row_s = stored + py * width;

            for (int px = px_min; px <= px_max; px++) {
                float d_x = (float)px - cx;
                float d2 = d_x * d_x + d_y2;
                float w = expf(-d2 * inv_two_sig_sq);
                float val = intensity * w;

                row_f[px] += val;
                float s_val = row_s[px] + val * 0.7f;
                row_s[px] = (s_val > 1.0f) ? 1.0f : s_val;
            }
        }

        cx += x_step;
        cy += y_step;
    }
}


static void plato_tek_resync_box_from_fb(float *flare, float *stored, int width, int height,
                                         const plato_framebuffer_t *fb,
                                         int x0, int y0, int x1, int y1) {
    if (!fb || !flare || !stored) return;

    if (x0 < 0) x0 = 0;
    if (x1 >= PLATO_WIDTH) x1 = PLATO_WIDTH - 1;
    if (y0 < 0) y0 = 0;
    if (y1 >= PLATO_HEIGHT) y1 = PLATO_HEIGHT - 1;
    if (x0 > x1 || y0 > y1) return;

    /* Estendi di 1 pixel PLATO (4 subpixel) per assorbire e ripulire l'intero alone gaussiano (sigma=1.15, r=3) */
    int px_min = (x0 > 0 ? x0 - 1 : 0) * PLASMA_SCALE;
    int px_max = (x1 < PLATO_WIDTH - 1 ? x1 + 2 : PLATO_WIDTH) * PLASMA_SCALE - 1;
    int py_min = ((PLATO_HEIGHT - 1) - (y1 < PLATO_HEIGHT - 1 ? y1 + 1 : y1)) * PLASMA_SCALE;
    int py_max = ((PLATO_HEIGHT - 1) - (y0 > 0 ? y0 - 1 : 0) + 1) * PLASMA_SCALE - 1;

    if (px_min < 0) px_min = 0;
    if (px_max >= width) px_max = width - 1;
    if (py_min < 0) py_min = 0;
    if (py_max >= height) py_max = height - 1;

    /* 1. Azzeramento locale chirurgico del dielettrico nel rettangolo con margine alone */
    for (int py = py_min; py <= py_max; py++) {
        float *row_s = stored + (size_t)py * width;
        float *row_f = flare + (size_t)py * width;
        for (int px = px_min; px <= px_max; px++) {
            row_s[px] = 0.0f;
            row_f[px] = 0.0f;
        }
    }

    /* 2. Ricostruzione perfetta dei soli punti realmente accesi in fb in quel rettangolo */
    int scan_x0 = (x0 > 0 ? x0 - 1 : 0);
    int scan_x1 = (x1 < PLATO_WIDTH - 1 ? x1 + 1 : PLATO_WIDTH - 1);
    int scan_y0 = (y0 > 0 ? y0 - 1 : 0);
    int scan_y1 = (y1 < PLATO_HEIGHT - 1 ? y1 + 1 : PLATO_HEIGHT - 1);

    for (int py = scan_y0; py <= scan_y1; py++) {
        int ry = (PLATO_HEIGHT - 1) - py;
        float cy = (float)ry * (float)PLASMA_SCALE + 2.0f;
        for (int px = scan_x0; px <= scan_x1; px++) {
            if (plato_fb_get_pixel(fb, px, py)) {
                float cx = (float)px * (float)PLASMA_SCALE + 2.0f;
                plato_tek_splat_point(flare, stored, width, height, cx, cy, 1.15f, 1.75f, 0.95f);
            }
        }
    }
}

static void plato_tek_splat_char(float *flare, float *stored, int width, int height,
                                 const plato_font_t *font, plato_charset_t charset, uint8_t ch,
                                 float x, float y, int scale, plato_screen_mode_t mode,
                                 const plato_framebuffer_t *fb) {
    if (!font) return;
    const uint8_t *glyph = plato_font_get_glyph(font, charset, ch);
    if (!glyph) return;

    if (scale <= 1) scale = 1;
    float y_base = y + ((scale > 1) ? (float)(scale - 1) * 5.0f : 0.0f);

    /* 1. In modalita REWRITE o ERASE, ricostruiamo localmente la cella da fb con padding alone */
    if (fb && (mode == PLATO_SCREEN_REWRITE || mode == PLATO_SCREEN_ERASE)) {
        int x0 = (int)x;
        int x1 = x0 + PLATO_CHAR_WIDTH * scale - 1;
        int y0 = (int)y_base;
        int y1 = y0 + PLATO_CHAR_HEIGHT * scale - 1;
        plato_tek_resync_box_from_fb(flare, stored, width, height, fb, x0, y0, x1, y1);
        return;
    }

    if (ch == ' ' || ch == 0x20 || ch == 0) return;

    /* 2. In modalita WRITE (puramente additiva), splat gaussiano continuo dei soli punti accesi */
    for (int row = 0; row < PLATO_CHAR_HEIGHT; row++) {
        uint8_t bits = glyph[row];
        if (bits == 0) continue;

        float py = y_base + (float)((PLATO_CHAR_HEIGHT - 1 - row) * scale);
        float screen_y = ((float)(PLATO_HEIGHT - 1) - py) * (float)PLASMA_SCALE + 2.0f;

        for (int col = 0; col < PLATO_CHAR_WIDTH; col++) {
            if (bits & (0x80 >> col)) {
                float cx = (x + (float)col * (float)scale) * (float)PLASMA_SCALE + 2.0f;
                plato_tek_splat_point(flare, stored, width, height, cx, screen_y, 1.15f, 1.75f, 0.95f);
            }
        }
    }
}

static void plato_tek_drain_queue(plato_optical_t *opt, double now_sec, int tek_baud) {
    if (!opt || !opt->term || !opt->state) return;
    struct PLATOPlasmaState *p = opt->state;

    if (tek_baud > 0) {
        double dt = p->tek_pacer_last_sec > 0.0 ? (now_sec - p->tek_pacer_last_sec) : 0.0;
        if (dt < 0.0) dt = 0.0;
        if (dt > 0.250) dt = 0.250;
        p->tek_pacer_last_sec = now_sec;
        p->tek_pacer_credit_bits += dt * (double)tek_baud;
        double max_credit = (double)tek_baud * 0.250;
        if (max_credit < 40.0) max_credit = 40.0; /* Garantisce drain di almeno 1 vettore linea (40 bit) a 110/150 Baud */
        if (p->tek_pacer_credit_bits > max_credit) p->tek_pacer_credit_bits = max_credit;
    } else {
        p->tek_pacer_last_sec = now_sec;
        p->tek_pacer_credit_bits = 0.0;
    }

    while (opt->term->vector_queue.tail != opt->term->vector_queue.head) {
        if (tek_baud > 0) {
            plato_vector_event_t *ev_peek = &opt->term->vector_queue.events[opt->term->vector_queue.tail];
            double cost_bits = 40.0; /* 4 byte seriali standard per coordinata Tektronix 4014 (10 bit/byte) */
            if (ev_peek->type == VEC_EVENT_CHAR) cost_bits = 10.0;
            else if (ev_peek->type == VEC_EVENT_ERASE) cost_bits = 20.0;

            if (p->tek_pacer_credit_bits < cost_bits) {
                break; /* Budget esaurito per questo frame: i vettori restanti rimangono in coda! */
            }
            p->tek_pacer_credit_bits -= cost_bits;
        }
        plato_vector_event_t ev = opt->term->vector_queue.events[opt->term->vector_queue.tail];
        opt->term->vector_queue.tail = (opt->term->vector_queue.tail + 1) % MAX_VECTOR_EVENTS;

        if (ev.type == VEC_EVENT_LINE) {
            float x0 = ev.x0 * (float)PLASMA_SCALE;
            float y0 = ((float)(PLATO_HEIGHT - 1) - ev.y0) * (float)PLASMA_SCALE;
            float x1 = ev.x1 * (float)PLASMA_SCALE;
            float y1 = ((float)(PLATO_HEIGHT - 1) - ev.y1) * (float)PLASMA_SCALE;
            if (ev.mode == PLATO_SCREEN_ERASE || ev.mode == PLATO_SCREEN_INVERSE) {
                /* Pulizia linea */
                float dx = x1 - x0, dy = y1 - y0;
                float dist = sqrtf(dx * dx + dy * dy);
                int steps = (int)ceilf(dist / 1.0f);
                float inv = steps > 0 ? (1.0f / (float)steps) : 1.0f;
                float cx = x0, cy = y0;
                for (int s = 0; s <= steps; s++) {
                    int r = 3;
                    int bx0 = (int)(cx - r); if (bx0 < 0) bx0 = 0;
                    int bx1 = (int)(cx + r); if (bx1 >= PLASMA_WIDTH) bx1 = PLASMA_WIDTH - 1;
                    int by0 = (int)(cy - r); if (by0 < 0) by0 = 0;
                    int by1 = (int)(cy + r); if (by1 >= PLASMA_HEIGHT) by1 = PLASMA_HEIGHT - 1;
                    for (int py = by0; py <= by1; py++) {
                        float *row_s = p->tek_stored + py * PLASMA_WIDTH;
                        for (int px = bx0; px <= bx1; px++) row_s[px] = 0.0f;
                    }
                    cx += dx * inv; cy += dy * inv;
                }
            } else {
                plato_tek_splat_line(p->tek_flare, p->tek_stored, PLASMA_WIDTH, PLASMA_HEIGHT, x0, y0, x1, y1);
            }
        } else if (ev.type == VEC_EVENT_POINT) {
            float x = ev.x0 * (float)PLASMA_SCALE;
            float y = ((float)(PLATO_HEIGHT - 1) - ev.y0) * (float)PLASMA_SCALE;
            if (ev.mode == PLATO_SCREEN_ERASE) {
                int r = 3;
                int bx0 = (int)(x - r); if (bx0 < 0) bx0 = 0;
                int bx1 = (int)(x + r); if (bx1 >= PLASMA_WIDTH) bx1 = PLASMA_WIDTH - 1;
                int by0 = (int)(y - r); if (by0 < 0) by0 = 0;
                int by1 = (int)(y + r); if (by1 >= PLASMA_HEIGHT) by1 = PLASMA_HEIGHT - 1;
                for (int py = by0; py <= by1; py++) {
                    float *row_s = p->tek_stored + py * PLASMA_WIDTH;
                    for (int px = bx0; px <= bx1; px++) row_s[px] = 0.0f;
                }
            } else {
                plato_tek_splat_point(p->tek_flare, p->tek_stored, PLASMA_WIDTH, PLASMA_HEIGHT, x, y, 1.4f, 1.5f, 0.85f);
            }
        } else if (ev.type == VEC_EVENT_BLOCK) {
            float min_x_p = (ev.x0 < ev.x1 ? ev.x0 : ev.x1);
            float max_x_p = (ev.x0 < ev.x1 ? ev.x1 : ev.x0);
            float min_y_p = (ev.y0 < ev.y1 ? ev.y0 : ev.y1);
            float max_y_p = (ev.y0 < ev.y1 ? ev.y1 : ev.y0);

            if (ev.mode == PLATO_SCREEN_ERASE || ev.mode == PLATO_SCREEN_INVERSE) {
                int bx0 = (int)floorf(min_x_p);
                int bx1 = (int)ceilf(max_x_p);
                int by0 = (int)floorf(min_y_p);
                int by1 = (int)ceilf(max_y_p);
                plato_tek_resync_box_from_fb(p->tek_flare, p->tek_stored, PLASMA_WIDTH, PLASMA_HEIGHT,
                                             &opt->term->fb, bx0, by0, bx1, by1);
            } else if (ev.mode == PLATO_SCREEN_WRITE || ev.mode == PLATO_SCREEN_REWRITE) {
                float bx0_f = min_x_p * (float)PLASMA_SCALE;
                float bx1_f = max_x_p * (float)PLASMA_SCALE + (float)PLASMA_SCALE;
                float by0_f = ((float)(PLATO_HEIGHT - 1) - max_y_p) * (float)PLASMA_SCALE;
                float by1_f = ((float)(PLATO_HEIGHT - 1) - min_y_p) * (float)PLASMA_SCALE + (float)PLASMA_SCALE;

                int px0 = (int)floorf(bx0_f); if (px0 < 0) px0 = 0;
                int px1 = (int)ceilf(bx1_f);  if (px1 >= PLASMA_WIDTH) px1 = PLASMA_WIDTH - 1;
                int py0 = (int)floorf(by0_f); if (py0 < 0) py0 = 0;
                int py1 = (int)ceilf(by1_f);  if (py1 >= PLASMA_HEIGHT) py1 = PLASMA_HEIGHT - 1;

                for (int py = py0; py <= py1; py++) {
                    float *row_s = p->tek_stored + py * PLASMA_WIDTH;
                    float *row_f = p->tek_flare + py * PLASMA_WIDTH;
                    for (int px = px0; px <= px1; px++) {
                        row_s[px] = 0.95f;
                        row_f[px] = 0.50f;
                    }
                }
            }
        } else if (ev.type == VEC_EVENT_CHAR) {
            plato_screen_mode_t mode = (plato_screen_mode_t)ev.mode;
            if (mode == PLATO_SCREEN_REWRITE || mode == PLATO_SCREEN_ERASE) {
                int scale = ev.scale > 1 ? (int)ev.scale : 1;
                int x0 = (int)ev.x0;
                int x1 = x0 + PLATO_CHAR_WIDTH * scale - 1;
                int y0 = (int)ev.y0 + (scale > 1 ? (scale - 1) * 5 : 0);
                int y1 = y0 + PLATO_CHAR_HEIGHT * scale - 1;

                while (opt->term->vector_queue.tail != opt->term->vector_queue.head) {
                    plato_vector_event_t *next =
                        &opt->term->vector_queue.events[opt->term->vector_queue.tail];
                    int next_scale = next->scale > 1 ? (int)next->scale : 1;
                    int next_x0 = (int)next->x0;
                    int next_x1 = next_x0 + PLATO_CHAR_WIDTH * next_scale - 1;
                    int next_y0 = (int)next->y0 + (next_scale > 1 ? (next_scale - 1) * 5 : 0);
                    int next_y1 = next_y0 + PLATO_CHAR_HEIGHT * next_scale - 1;

                    if (next->type != VEC_EVENT_CHAR ||
                        next->mode != ev.mode ||
                        next_scale != scale ||
                        next_y0 != y0 ||
                        next_y1 != y1 ||
                        next_x0 > x1 + 1 ||
                        next_x1 < x0 - 1) {
                        break;
                    }

                    if (next_x0 < x0) x0 = next_x0;
                    if (next_x1 > x1) x1 = next_x1;
                    opt->term->vector_queue.tail =
                        (opt->term->vector_queue.tail + 1) % MAX_VECTOR_EVENTS;
                }

                plato_tek_resync_box_from_fb(p->tek_flare, p->tek_stored,
                                             PLASMA_WIDTH, PLASMA_HEIGHT,
                                             &opt->term->fb, x0, y0, x1, y1);
            } else {
                plato_tek_splat_char(p->tek_flare, p->tek_stored,
                                     PLASMA_WIDTH, PLASMA_HEIGHT,
                                     &opt->term->font,
                                     (plato_charset_t)ev.charset, ev.ch,
                                     ev.x0, ev.y0, (int)ev.scale, mode,
                                     &opt->term->fb);
            }
        } else if (ev.type == VEC_EVENT_ERASE) {
            memset(p->tek_stored, 0, (size_t)PLASMA_PIXELS * sizeof(float));
            /* Grace Period anti-strobe di 1.0 secondo */
            if (now_sec >= p->tek_erase_grace_until) {
                p->tek_erase_time = now_sec;
            }
            p->tek_erase_grace_until = now_sec + 1.0;
        }
    }
}


/* ========================================================================= */
/* TEKTRONIX 4014 DVBST RENDER ENGINE (Transient Flare + Stored + Erase Flash) */
/* ========================================================================= */

static void tektronixRender(plato_optical_t *opt, uint32_t *output, double now_sec, int tek_baud) {
    if (!opt || !opt->state || !opt->term || !output) return;
    struct PLATOPlasmaState *p = opt->state;
    if (!p->tek_flare || !p->tek_stored) return;

    float dt = p->lastTime > 0.0 ? (float)(now_sec - p->lastTime) : (1.0f / 60.0f);
    p->lastTime = now_sec;
    dt = plasmaClamp(dt, 0.0005f, 0.1f);

    /* 1. Assorbi i vettori dalla coda vettoriale con pacing Tektronix */
    plato_tek_drain_queue(opt, now_sec, tek_baud);

    /* 2. Fisica dell'Erase Flash (Scarica Dielettrica violenta di ~400ms) */
    float erase_flash = 0.0f;
    if (p->tek_erase_time > 0.0) {
        double elapsed = now_sec - p->tek_erase_time;
        if (elapsed < 0.400) {
            float t = (float)(elapsed / 0.400);
            erase_flash = (1.0f - t) * (1.0f - t); /* Decadimento quadratico */
        } else {
            p->tek_erase_time = 0.0;
        }
    }

    /* 3. Decadimento del Transient Flare (Bagliore transitorio accecante, ~45ms) */
    float flare_decay = expf(-dt / 0.045f);
    bool has_activity = (erase_flash > 0.001f);

    /* 4. Sintesi Cromatica P31 Phosphor HDR nello spazio 2048x2048 */
    for (size_t i = 0; i < (size_t)PLASMA_PIXELS; i++) {
        float f = p->tek_flare[i];
        float s = p->tek_stored[i];

        f *= flare_decay;
        if (f < 0.001f) f = 0.0f;
        else has_activity = true;
        p->tek_flare[i] = f;

        /* Flood Gun Ambient Glow: sottile emissione di fondo reale dello schermo DVBST */
        float bg_glow = 0.015f;

        if (f <= 0.0001f && s <= 0.0001f && erase_flash <= 0.0001f) {
            /* Nero caldo fluorescente P31 delle flood guns a riposo */
            output[i] = plasmaRGBA(bg_glow * 0.3f, bg_glow * 1.0f, bg_glow * 0.4f);
            continue;
        }

        /* Stored state bistabile: brillantezza piena al 92% (invece di 60%) */
        float stored_lum = (s > 1.0f ? 1.0f : s) * 0.92f;

        /* Modello Iper-Saturazione Termoluminescente (White-Hot Core)
           flare_base: alone fluorescente P31 lineare (Emerald Bloom)
           core_burn: sovraccarico del fosforo che satura RGB a bianco (Decadimento quadratico rapido) */
        float flare_base = f * 2.20f;
        float core_burn  = f * f * 2.00f;

        /* P31 Phosphor Spectrum radioso: Verde smeraldo profondo, Bloom verde brillante, Core BIANCO incandescente */
        float r = bg_glow * 0.3f + stored_lum * 0.16f + flare_base * 0.30f + core_burn + erase_flash * 0.35f;
        float g = bg_glow * 1.0f + stored_lum * 0.96f + flare_base * 1.00f + core_burn + erase_flash * 0.98f;
        float b = bg_glow * 0.4f + stored_lum * 0.30f + flare_base * 0.10f + core_burn + erase_flash * 0.40f;

        output[i] = plasmaRGBA(r, g, b);
    }

    if (has_activity || opt->term->vector_queue.tail != opt->term->vector_queue.head) {
        opt->state->animateUntil = now_sec + 0.080;
    }
    opt->last_was_full = true;
}

static void crtRender(plato_optical_t *opt, uint32_t *output, double now_sec) {
    struct PLATOPlasmaState *p = opt->state;
    float deltaTime = p->lastTime > 0.0 ? (float)(now_sec - p->lastTime) : 1.0f / 60.0f;
    p->lastTime = now_sec;
    deltaTime = plasmaClamp(deltaTime, 0.0f, 0.1f);

    unsigned dirtyTiles = crtBuildCellSurface(opt, deltaTime);
    if (dirtyTiles == 0) {
        opt->last_was_full = false;
        return;
    }

    bool fullFrame = (p->crtDistortion != 0) || (dirtyTiles > PLASMA_PARTIAL_LIMIT);
    opt->last_was_full = fullFrame;

    if (fullFrame) {
        plasmaBoxBlur(p->source, p->tmp, p->local, 4);
        plasmaBoxBlur(p->source, p->tmp, p->wide, 12);
        for (int y = 0; y < PLASMA_TILE_ROWS; y++) {
            for (int x = 0; x < PLASMA_TILE_COLS; x++) {
                crtComposeTile(opt, output, x, y);
            }
        }
    } else {
        for (int y = 0; y < PLASMA_TILE_ROWS; y++) {
            for (int x = 0; x < PLASMA_TILE_COLS; x++) {
                if (p->dirtyOutput[y * PLASMA_TILE_COLS + x]) {
                    plasmaBoxBlurTile(p->source, p->tmp, p->local, 4, x, y);
                    plasmaBoxBlurTile(p->source, p->tmp, p->wide, 12, x, y);
                    crtComposeTile(opt, output, x, y);
                }
            }
        }
    }
}

static void plasmaRender(plato_optical_t *opt, uint32_t *output, double now_sec) {
    struct PLATOPlasmaState *p = opt->state;
    plato_terminal_render_rgba(opt->term, p->logical);
    float deltaTime = p->lastTime > 0.0 ? (float)(now_sec - p->lastTime) : 1.0f / 60.0f;
    p->lastTime = now_sec; deltaTime = plasmaClamp(deltaTime, 0.0f, 0.1f);
    unsigned dirtyTiles = plasmaBuildCellSurface(opt, deltaTime);
    if (dirtyTiles == 0) {
        opt->last_was_full = false;
        return;
    }
    bool fullFrame = (p->plasmaDistortion != 0) || (dirtyTiles > PLASMA_PARTIAL_LIMIT);
    opt->last_was_full = fullFrame;
    
    if (fullFrame) {
        plasmaBoxBlur(p->source, p->tmp, p->local, 4);
        plasmaBoxBlur(p->source, p->tmp, p->wide, 12);
        for (int y = 0; y < PLASMA_TILE_ROWS; y++) {
            for (int x = 0; x < PLASMA_TILE_COLS; x++) plasmaComposeTile(opt, output, x, y);
        }
    } else {
        for (int y = 0; y < PLASMA_TILE_ROWS; y++) {
            for (int x = 0; x < PLASMA_TILE_COLS; x++) {
                if (p->dirtyOutput[y * PLASMA_TILE_COLS + x]) {
                    plasmaBoxBlurTile(p->source, p->tmp, p->local, 4, x, y);
                    plasmaBoxBlurTile(p->source, p->tmp, p->wide, 12, x, y);
                    plasmaComposeTile(opt, output, x, y);
                }
            }
        }
    }
}

plato_optical_t* plato_optical_create(void) {
    plato_optical_t *opt = calloc(1, sizeof(plato_optical_t));
    if (!opt) return NULL;
    opt->state = calloc(1, sizeof(struct PLATOPlasmaState));
    if (!plasmaAllocState(opt->state)) {
        free(opt->state);
        free(opt);
        return NULL;
    }
    return opt;
}

void plato_optical_invalidate(plato_optical_t *opt) {
    if (opt && opt->state) {
        opt->state->tilesInitialized = false;
        opt->state->lastTime = 0.0;
        opt->state->tek_erase_time = 0.0;
        opt->state->tek_erase_grace_until = 0.0;
    }
}

void plato_optical_destroy(plato_optical_t *opt) {
    if (!opt) return;
    if (opt->state) {
        plasmaFreeState(opt->state);
        free(opt->state);
    }
    free(opt);
}

bool plato_optical_render(plato_optical_t *opt, plato_terminal_t *term, const plato_profile_t *prof, double now_sec, uint32_t *out_bgra) {
    if (!opt || !opt->state || !term || !out_bgra || !prof) return false;

    opt->term = term;

    double new_decay_dur = (double)prof->persistence_ms / 1000.0;
    double new_crt_decay_dur = (double)prof->crt_persistence_ms / 1000.0;

    /* Sincronizza sempre la palette nel terminale logico */
    term->palette = (plato_palette_t)prof->phosphor_color;

    /* Rileva se l'utente ha cambiato modalità o parametri dal menu -> Forza re-init totale istantaneo */
    if (opt->state->displayMode != prof->display_mode ||
        opt->state->plasmaDistortion != prof->plasma_distortion ||
        opt->state->crtDistortion != prof->crt_distortion ||
        opt->state->crtBeamLevel != prof->crt_beam_level ||
        opt->state->phosphorColor != prof->phosphor_color ||
        opt->state->decayDuration != new_decay_dur ||
        opt->state->crtDecayDuration != new_crt_decay_dur) {

        opt->state->displayMode = prof->display_mode;
        opt->state->phosphorColor = prof->phosphor_color;
        opt->state->decayDuration = new_decay_dur;
        opt->state->decayTau = (float)(new_decay_dur / 10.0);
        opt->state->plasmaDistortion = prof->plasma_distortion;

        opt->state->crtDecayDuration = new_crt_decay_dur;
        opt->state->crtDecayTau = (float)(new_crt_decay_dur / 10.0);
        opt->state->crtBeamLevel = prof->crt_beam_level;
        opt->state->crtDistortion = prof->crt_distortion;

        opt->state->tilesInitialized = false; /* Forza re-inizializzazione e ridisegno di tutti i tile */
        opt->state->lastTime = 0.0;
        double dur = (prof->display_mode == 4) ? new_crt_decay_dur : new_decay_dur;
        if (prof->display_mode == 5) dur = 0.50;
        if (dur < 0.10) dur = 0.10;
        opt->state->animateUntil = now_sec + dur;
    }

    if (opt->state->decayTau < 0.001f) opt->state->decayTau = 0.001f;
    if (opt->state->crtDecayTau < 0.001f) opt->state->crtDecayTau = 0.001f;

    bool was_dirty = term->fb.dirty;
    if (term->fb.dirty) {
        double dur = 0.0;
        if (prof->display_mode == 4) dur = opt->state->crtDecayDuration;
        else if (prof->display_mode == 0 || prof->display_mode == 2) dur = opt->state->decayDuration;
        else dur = 0.0; /* Crisp mono e Crisp color: nessun decadimento fosfori */
        opt->state->animateUntil = now_sec + dur;
        term->fb.dirty = false;
    }

    if (prof->display_mode == 1 || prof->display_mode == 3) {
        plato_terminal_render_rgba(term, opt->state->logical);
        plasmaExpandCrisp(opt->state->logical, out_bgra);
        opt->last_was_full = false;
    } else if (prof->display_mode == 4) {
        crtRender(opt, out_bgra, now_sec);
    } else if (prof->display_mode == 5) {
        if (!opt->state->tilesInitialized) {
            memset(opt->state->tek_flare, 0, (size_t)PLASMA_PIXELS * sizeof(float));
            memset(opt->state->tek_stored, 0, (size_t)PLASMA_PIXELS * sizeof(float));
            for (int y = 0; y < PLATO_HEIGHT; y++) {
                int ry = (PLATO_HEIGHT - 1) - y;
                for (int x = 0; x < PLATO_WIDTH; x++) {
                    if (plato_fb_get_pixel(&term->fb, x, y)) {
                        float cx = (float)x * (float)PLASMA_SCALE + 2.0f;
                        float cy = (float)ry * (float)PLASMA_SCALE + 2.0f;
                        plato_tek_splat_point(opt->state->tek_flare, opt->state->tek_stored,
                                              PLASMA_WIDTH, PLASMA_HEIGHT, cx, cy, 1.15f, 1.75f, 0.95f);
                    }
                }
            }
            opt->state->tilesInitialized = true;
        }
        tektronixRender(opt, out_bgra, now_sec, prof->tek_baud_rate);
    } else {
        plasmaRender(opt, out_bgra, now_sec);
        if (prof->display_mode == 2) {
            plato_terminal_render_rgba(term, opt->state->logical);
            plasmaExpandCrisp(opt->state->logical, opt->state->crisp);
            size_t half = (PLASMA_WIDTH / 2) * sizeof(uint32_t);
            for (int y = 0; y < PLASMA_HEIGHT; y++) {
                memcpy(out_bgra + y * PLASMA_WIDTH + PLASMA_WIDTH / 2,
                       opt->state->crisp + y * PLASMA_WIDTH + PLASMA_WIDTH / 2, half);
            }
        }
    }
    return was_dirty || (now_sec < opt->state->animateUntil);
}

void plato_optical_unwarp_touch(int *x, int *y, int display_mode, int plasma_distortion, int crt_distortion) {
    if (!x || !y) return;
    int activeDist = 0;
    if (display_mode == 4) {
        activeDist = crt_distortion;
    } else if (display_mode == 0 || display_mode == 2) {
        activeDist = plasma_distortion;
    }
    if (activeDist == 0) return;

    /* Coordinate normalizzate nel range [-1.0, 1.0] attorno al centro di 512x512 */
    float u = ((float)(*x) - 255.5f) / 255.5f;
    float v = ((float)(*y) - 255.5f) / 255.5f;
    float u_src = u, v_src = v;

    const float k_cyl_x = 0.018f;
    const float k_bar_x = 0.018f;
    const float k_bar_y = 0.012f;

    if (activeDist == 2) {
        /* Distorsione Cilindrica (curvatura solo orizzontale in funzione di v^2) */
        u_src = u * (1.0f + (v * v) * k_cyl_x);
    } else if (activeDist == 1) {
        /* Distorsione a Barilotto (curvatura sia orizzontale che verticale) */
        u_src = u * (1.0f + (v * v) * k_bar_x);
        v_src = v * (1.0f + (u * u) * k_bar_y);
    }

    int nx = (int)(255.5f + u_src * 255.5f + 0.5f);
    int ny = (int)(255.5f + v_src * 255.5f + 0.5f);
    if (nx < 0) nx = 0; if (nx >= PLATO_WIDTH) nx = PLATO_WIDTH - 1;
    if (ny < 0) ny = 0; if (ny >= PLATO_HEIGHT) ny = PLATO_HEIGHT - 1;
    *x = nx;
    *y = ny;
}
