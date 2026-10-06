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
    if (!ps->gain || !ps->energy || !ps->source || !ps->tmp ||
        !ps->local || !ps->wide || !ps->crisp || !ps->logical) {
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
                output[i] = PLASMA_BG_PIXEL;
                continue;
            }

            float neighborhood = plasmaClamp((localGlow - 0.25f) * 2.50f, 0.0f, 1.0f);
            neighborhood = neighborhood * neighborhood * (3.0f - 2.0f * neighborhood);
            float fusedCore = core;
            if (core > 0.05f) fusedCore += 0.82f * neighborhood * (1.0f - core);
            float density = plasmaClamp(fusedCore + 0.62f * localGlow + 0.22f * wideGlow, 0.0f, 1.5f);
            float hot = plasmaClamp((density - 0.92f) * 2.0f, 0.0f, 1.0f);
            float red = 0.055f + 0.95f * fusedCore + 0.58f * localGlow + 0.30f * wideGlow;
            float green = 0.016f + 0.31f * fusedCore + 0.060f * localGlow + 0.020f * wideGlow + 0.08f * hot;
            float blue = 0.004f + 0.018f * fusedCore + 0.003f * localGlow + 0.010f * hot;
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

            float chroma_r = (sum_avg > 0.001f) ? (r_avg / sum_avg) : 0.0f;
            float chroma_g = (sum_avg > 0.001f) ? (g_avg / sum_avg) : 0.0f;
            float chroma_b = (sum_avg > 0.001f) ? (b_avg / sum_avg) : 0.0f;

            float m_r = mask_tbl[sx][0];
            float m_g = mask_tbl[sx][1];
            float m_b = mask_tbl[sx][2];

            float bloom_val = glow * prof->bloom_amt * 1.5f;
            float r_final = (r_beam * m_r * v_beam) + (chroma_r * bloom_val);
            float g_final = (g_beam * m_g * v_beam) + (chroma_g * bloom_val);
            float b_final = (b_beam * m_b * v_beam) + (chroma_b * bloom_val);

            r_final *= prof->gain;
            g_final *= prof->gain;
            b_final *= prof->gain;

            float max_c = fmaxf(r_final, fmaxf(g_final, b_final));
            if (max_c > 1.0f) {
                float boost = (max_c - 1.0f) * 0.20f;
                r_final += boost;
                g_final += boost;
                b_final += boost;
            }

            output[i] = plasmaRGBA(plasmaClamp(r_final, 0.0f, 1.0f),
                                   plasmaClamp(g_final, 0.0f, 1.0f),
                                   plasmaClamp(b_final, 0.0f, 1.0f));
        }
    }
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

    /* Rileva se l'utente ha cambiato modalità o parametri dal menu -> Forza re-init totale istantaneo */
    if (opt->state->displayMode != prof->display_mode ||
        opt->state->plasmaDistortion != prof->plasma_distortion ||
        opt->state->crtDistortion != prof->crt_distortion ||
        opt->state->crtBeamLevel != prof->crt_beam_level ||
        opt->state->decayDuration != new_decay_dur ||
        opt->state->crtDecayDuration != new_crt_decay_dur) {

        opt->state->displayMode = prof->display_mode;
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
