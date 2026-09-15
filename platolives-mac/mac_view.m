#import "mac_view.h"
#import <QuartzCore/QuartzCore.h>
#include <math.h>

NSString *PLATOKeyboardReferenceText(void) {
    return @"PLATO KEYBOARD REFERENCE\n\n"
           @"NEXT          Return / Enter / Ctrl+N\n"
           @"SHIFT-NEXT    Shift+Return / Shift+Ctrl+N\n"
           @"BACK          Esc / F8 / Ctrl+B\n"
           @"SHIFT-BACK    Shift+Esc / Shift+F8 / Shift+Ctrl+B\n"
           @"STOP          F10 / F4 / Ctrl+S / Option+S\n"
           @"SHIFT-STOP    Shift + any STOP shortcut\n"
           @"ERASE         Backspace / Delete / Left Arrow / Ctrl+R\n"
           @"SHIFT-ERASE   Shift + any ERASE shortcut\n"
           @"HELP          F1 / F6 / Ctrl+H / Option+H\n"
           @"SHIFT-HELP    Shift + any HELP shortcut\n"
           @"LAB           F2 / F7 / Ctrl+L / Option+L\n"
           @"SHIFT-LAB     Shift + any LAB shortcut\n"
           @"DATA          F3 / F9 / Ctrl+D / Option+D\n"
           @"SHIFT-DATA    Shift + any DATA shortcut\n"
           @"EDIT          F5 / Ctrl+E / Option+E\n"
           @"SHIFT-EDIT    Shift+F5 / Shift+Ctrl+E\n"
           @"COPY          Ctrl+C / Option+C\n"
           @"SHIFT-COPY    Shift+Ctrl+C / Shift+Option+C\n"
           @"MICRO         Ctrl+M / Option+M\n"
           @"FONT          Ctrl+F / Option+F\n"
           @"SUPER         Up Arrow / Page Up / Ctrl+P\n"
           @"SHIFT-SUPER   Shift + any SUPER shortcut (Ascend)\n"
           @"SUB           Down Arrow / Page Down / Ctrl+Y\n"
           @"SHIFT-SUB     Shift + any SUB shortcut (Descend)\n"
           @"ANS           Ctrl+A / Ctrl+/ / Option+A\n"
           @"TERM          Ctrl+T / Option+T / Shift+Ctrl+A\n"
           @"SQUARE        Ctrl+Q / Option+Q\n"
           @"ACCESS        Shift+Ctrl+Q / Shift+Option+Q\n"
           @"TAB           Tab / Right Arrow\n"
           @"ASSIGN (<=)   Option+Left Arrow\n"
           @"MULTIPLY (*)  Ctrl+X / Option+X\n"
           @"DIVIDE (/)    Ctrl+G / Option+G\n\n"
           @"APPLICATION SHORTCUTS\n\n"
           @"NEW WINDOW    Cmd+N\n"
           @"CLOSE WINDOW  Cmd+W\n"
           @"PROFILES      Cmd+,\n"
           @"COPY SCREEN   Cmd+C\n"
           @"PASTE TEXT    Cmd+V (throttled)\n"
           @"CANCEL PASTE  Cmd+.\n"
           @"FULL SCREEN   F11\n"
           @"PERF HUD      F12\n";
}

static CGRect platoDisplayRect(NSRect bounds) {
    CGFloat scale = fmin(bounds.size.width / PLATO_WIDTH, bounds.size.height / PLATO_HEIGHT);
    if (scale < 1.0) scale = 1.0;
    CGFloat drawWidth = PLATO_WIDTH * scale, drawHeight = PLATO_HEIGHT * scale;
    return CGRectMake((bounds.size.width - drawWidth) / 2.0, (bounds.size.height - drawHeight) / 2.0, drawWidth, drawHeight);
}

typedef NS_ENUM(NSInteger, PLATODisplayMode) {
    PLATODisplayModeCrisp = 0,
    PLATODisplayModeRealPlasma = 1,
    PLATODisplayModeSplit = 2,
    PLATODisplayModeCrispColor = 3,
    PLATODisplayModeRealColorCRT = 4
};

#define PLASMA_SCALE 4
#define PLASMA_WIDTH (PLATO_WIDTH * PLASMA_SCALE)
#define PLASMA_HEIGHT (PLATO_HEIGHT * PLASMA_SCALE)
#define PLASMA_PIXELS ((size_t)PLASMA_WIDTH * PLASMA_HEIGHT)
#define PLASMA_BYTES (PLASMA_PIXELS * sizeof(uint32_t))
#define PLASMA_TILE_LOGICAL 32
#define PLASMA_TILE_SIZE (PLASMA_TILE_LOGICAL * PLASMA_SCALE)
#define PLASMA_TILE_COLS (PLASMA_WIDTH / PLASMA_TILE_SIZE)
#define PLASMA_TILE_ROWS (PLASMA_HEIGHT / PLASMA_TILE_SIZE)
#define PLASMA_TILE_COUNT (PLASMA_TILE_COLS * PLASMA_TILE_ROWS)
#define PLASMA_PARTIAL_LIMIT 180
#define PLASMA_BG_PIXEL 0xFF0E0401u

struct PLATOPlasmaState {
    PLATODisplayMode displayMode;
    float *gain;
    float *energy;
    float *source;
    float *tmp;
    float *local;
    float *wide;
    uint32_t *crisp;
    uint32_t *logical;
    CFTimeInterval lastTime;
    CFTimeInterval animateUntil;
    double decayDuration;
    float decayTau;
    double crtDecayDuration;
    float crtDecayTau;
    uint8_t dirtySource[PLASMA_TILE_COUNT];
    uint8_t dirtyOutput[PLASMA_TILE_COUNT];
    BOOL tilesInitialized;
    int crtBeamLevel;
    int crtDistortion; // 0: None (Flat), 1: Barrel (Spherical), 2: Cylindrical (Default)
    int plasmaDistortion; // 0: None (Flat), 1: Barrel (Spherical), 2: Cylindrical (Default)
};

typedef struct {
    double logicalMs, expandMs, buildMs, blur4Ms, blur12Ms, composeMs, totalMs;
    double blur4HorizontalMs, blur4VerticalMs, blur12HorizontalMs, blur12VerticalMs;
    unsigned sourceTiles, outputTiles;
    BOOL fullFrame, noOp;
} PlasmaFrameProfile;

#define PLASMA_PROFILE_HISTORY 64

typedef struct {
    PlasmaFrameProfile frame;
    CFTimeInterval timestamp;
    PLATODisplayMode mode;
} PlasmaProfileSample;

static PlasmaProfileSample plasmaProfileHistory[PLASMA_PROFILE_HISTORY] = {0};
static size_t plasmaProfileNext = 0, plasmaProfileCount = 0;
static NSString *plasmaProfilePath = nil;

static inline double plasmaElapsedMs(CFTimeInterval start, CFTimeInterval end) { return (end - start) * 1000.0; }

static void plasmaProfileFrame(const PlasmaFrameProfile *frame, PLATODisplayMode mode) {
    plasmaProfileHistory[plasmaProfileNext].frame = *frame;
    plasmaProfileHistory[plasmaProfileNext].timestamp = CACurrentMediaTime();
    plasmaProfileHistory[plasmaProfileNext].mode = mode;
    plasmaProfileNext = (plasmaProfileNext + 1) % PLASMA_PROFILE_HISTORY;
    if (plasmaProfileCount < PLASMA_PROFILE_HISTORY) plasmaProfileCount++;
}

static NSString *plasmaProfileModeName(PLATODisplayMode mode) {
    return mode == PLATODisplayModeCrisp ? @"crisp" : (mode == PLATODisplayModeCrispColor ? @"crisp-color" : (mode == PLATODisplayModeSplit ? @"split" : @"real"));
}

typedef struct {
    double buildMs, blur4Ms, blur12Ms, composeMs, totalMs, sourceTiles, outputTiles;
    double blur4HorizontalMs, blur4VerticalMs, blur12HorizontalMs, blur12VerticalMs;
    uint64_t frames, activeFrames, partialFrames, fullFrames, noOpFrames;
    CFTimeInterval started;
} PlasmaLiveProfile;

static BOOL plasmaLiveProfileEnabled = NO;
static PlasmaLiveProfile plasmaLiveProfile = {0};

static void plasmaLiveProfileReset(CFTimeInterval now) {
    memset(&plasmaLiveProfile, 0, sizeof(plasmaLiveProfile));
    plasmaLiveProfile.started = now;
}

static void plasmaLiveProfileFrame(const PlasmaFrameProfile *frame, PLATODisplayMode mode) {
    if (!plasmaLiveProfileEnabled) return;
    CFTimeInterval now = CACurrentMediaTime();
    if (plasmaLiveProfile.started == 0.0) plasmaLiveProfile.started = now;
    plasmaLiveProfile.frames++;
    if (frame->noOp) {
        plasmaLiveProfile.noOpFrames++;
    } else {
        plasmaLiveProfile.activeFrames++;
        plasmaLiveProfile.buildMs += frame->buildMs; plasmaLiveProfile.blur4Ms += frame->blur4Ms;
        plasmaLiveProfile.blur12Ms += frame->blur12Ms; plasmaLiveProfile.composeMs += frame->composeMs;
        plasmaLiveProfile.totalMs += frame->totalMs; plasmaLiveProfile.sourceTiles += frame->sourceTiles;
        plasmaLiveProfile.outputTiles += frame->outputTiles;
        plasmaLiveProfile.blur4HorizontalMs += frame->blur4HorizontalMs; plasmaLiveProfile.blur4VerticalMs += frame->blur4VerticalMs;
        plasmaLiveProfile.blur12HorizontalMs += frame->blur12HorizontalMs; plasmaLiveProfile.blur12VerticalMs += frame->blur12VerticalMs;
        if (mode != PLATODisplayModeCrisp) { if (frame->fullFrame) plasmaLiveProfile.fullFrames++; else plasmaLiveProfile.partialFrames++; }
    }
    if (now - plasmaLiveProfile.started < 0.5) return;
    if (plasmaLiveProfile.activeFrames > 0) {
        double count = (double)plasmaLiveProfile.activeFrames;
        NSLog(@"[PLASMA LIVE] interval=%.3fs mode=%@ frames=%llu active=%llu partial=%llu full=%llu noop=%llu active-source-tiles=%.1f active-output-tiles=%.1f active-coverage=%.1f%% active-ms build=%.3f blur4=%.3f(h=%.3f v=%.3f) blur12=%.3f(h=%.3f v=%.3f) compose=%.3f total=%.3f",
              now - plasmaLiveProfile.started, plasmaProfileModeName(mode),
              (unsigned long long)plasmaLiveProfile.frames, (unsigned long long)plasmaLiveProfile.activeFrames,
              (unsigned long long)plasmaLiveProfile.partialFrames, (unsigned long long)plasmaLiveProfile.fullFrames,
              (unsigned long long)plasmaLiveProfile.noOpFrames,
              plasmaLiveProfile.sourceTiles / count, plasmaLiveProfile.outputTiles / count,
              100.0 * plasmaLiveProfile.outputTiles / (count * PLASMA_TILE_COUNT),
              plasmaLiveProfile.buildMs / count, plasmaLiveProfile.blur4Ms / count,
              plasmaLiveProfile.blur4HorizontalMs / count, plasmaLiveProfile.blur4VerticalMs / count,
              plasmaLiveProfile.blur12Ms / count, plasmaLiveProfile.blur12HorizontalMs / count,
              plasmaLiveProfile.blur12VerticalMs / count, plasmaLiveProfile.composeMs / count, plasmaLiveProfile.totalMs / count);
    }
    plasmaLiveProfileReset(now);
}

static NSString *plasmaProfileLine(NSString *tag, PLATODisplayMode currentMode) {
    CFTimeInterval cutoff = CACurrentMediaTime() - 1.0;
    PlasmaFrameProfile total = {0};
    uint64_t frames = 0;
    PLATODisplayMode mode = currentMode;
    for (size_t offset = 0; offset < plasmaProfileCount; offset++) {
        size_t index = (plasmaProfileNext + PLASMA_PROFILE_HISTORY - 1 - offset) % PLASMA_PROFILE_HISTORY;
        PlasmaProfileSample *sample = &plasmaProfileHistory[index];
        if (sample->timestamp < cutoff) break;
        total.logicalMs += sample->frame.logicalMs; total.expandMs += sample->frame.expandMs;
        total.buildMs += sample->frame.buildMs; total.blur4Ms += sample->frame.blur4Ms;
        total.blur12Ms += sample->frame.blur12Ms; total.composeMs += sample->frame.composeMs;
        total.totalMs += sample->frame.totalMs; mode = sample->mode; frames++;
    }
    NSString *line;
    if (frames == 0) {
        line = [NSString stringWithFormat:@"[PLASMA PROFILE] tag=\"%@\" window=1.000s mode=%@ frames=0 no-rendered-frames", tag, plasmaProfileModeName(mode)];
    } else {
        double count = (double)frames;
        line = [NSString stringWithFormat:@"[PLASMA PROFILE] tag=\"%@\" window=1.000s mode=%@ frames=%llu avg-ms logical=%.3f expand=%.3f build=%.3f blur4=%.3f blur12=%.3f compose=%.3f total=%.3f",
                tag, plasmaProfileModeName(mode), (unsigned long long)frames,
                total.logicalMs / count, total.expandMs / count, total.buildMs / count,
                total.blur4Ms / count, total.blur12Ms / count, total.composeMs / count, total.totalMs / count];
    }
    memset(plasmaProfileHistory, 0, sizeof(plasmaProfileHistory));
    plasmaProfileNext = 0;
    plasmaProfileCount = 0;
    return line;
}

static inline float plasmaClamp(float value, float low, float high) {
    return fminf(fmaxf(value, low), high);
}

static inline uint32_t plasmaRGBA(float red, float green, float blue) {
    uint32_t r = (uint32_t)(plasmaClamp(red, 0.0f, 1.0f) * 255.0f + 0.5f);
    uint32_t g = (uint32_t)(plasmaClamp(green, 0.0f, 1.0f) * 255.0f + 0.5f);
    uint32_t b = (uint32_t)(plasmaClamp(blue, 0.0f, 1.0f) * 255.0f + 0.5f);
    return b | (g << 8) | (r << 16) | (0xFFu << 24);
}

static void plasmaFreeState(PLATOPlasmaState *ps) {
    if (!ps) return;
    free(ps->gain); free(ps->energy); free(ps->source); free(ps->tmp);
    free(ps->local); free(ps->wide); free(ps->crisp); free(ps->logical);
    ps->gain = ps->energy = ps->source = ps->tmp = ps->local = ps->wide = NULL;
    ps->crisp = ps->logical = NULL;
    ps->lastTime = 0.0;
    ps->animateUntil = 0.0;
    ps->tilesInitialized = NO;
}

static BOOL plasmaAllocState(PLATOPlasmaState *ps) {
    if (!ps) return NO;
    if (ps->gain) return YES;
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
        return NO;
    }
    for (size_t i = 0; i < logicalPixels; i++) {
        float randomValue = (float)arc4random_uniform(1000001) / 1000000.0f;
        ps->gain[i] = 0.980f + 0.040f * randomValue;
    }
    return YES;
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

static void plasmaBoxBlur(const float *source, float *temporary, float *destination, int radius, double *horizontalMs, double *verticalMs) {
    CFTimeInterval horizontalStart = CACurrentMediaTime();
    plasmaBlurRows(source, temporary, PLASMA_WIDTH, PLASMA_HEIGHT, radius);
    CFTimeInterval horizontalEnd = CACurrentMediaTime();
    plasmaBlurCols(temporary, destination, PLASMA_WIDTH, PLASMA_HEIGHT, radius);
    CFTimeInterval verticalEnd = CACurrentMediaTime();
    if (horizontalMs) *horizontalMs += plasmaElapsedMs(horizontalStart, horizontalEnd);
    if (verticalMs) *verticalMs += plasmaElapsedMs(horizontalEnd, verticalEnd);
}

static void plasmaBoxBlurTile(const float *source, float *temporary, float *destination, int radius, int tileX, int tileY, double *horizontalMs, double *verticalMs) {
    const int width = PLASMA_WIDTH, height = PLASMA_HEIGHT, span = radius * 2 + 1;
    const float invSpan = 1.0f / (float)span;
    CFTimeInterval horizontalStart = CACurrentMediaTime();
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
    CFTimeInterval horizontalEnd = CACurrentMediaTime();

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
    CFTimeInterval verticalEnd = CACurrentMediaTime();

    if (horizontalMs) *horizontalMs += plasmaElapsedMs(horizontalStart, horizontalEnd);
    if (verticalMs) *verticalMs += plasmaElapsedMs(horizontalEnd, verticalEnd);
}

static void plasmaComposeTile(PLATOView *self, uint32_t *output, int tileX, int tileY) {
    PLATOPlasmaState *p = self->plasma;
    int x0 = tileX * PLASMA_TILE_SIZE, x1 = x0 + PLASMA_TILE_SIZE;
    int y0 = tileY * PLASMA_TILE_SIZE, y1 = y0 + PLASMA_TILE_SIZE;

    const float half_dim = 1024.0f;
    const float inv_half_dim = 1.0f / 1024.0f;
    const float k_cyl_x = 0.018f;
    const float k_bar_x = 0.018f;
    const float k_bar_y = 0.012f;
    int distMode = (p->displayMode == PLATODisplayModeRealPlasma) ? p->plasmaDistortion : 0;

    for (int y = y0; y < y1; y++) {
        float v_norm = ((float)y - half_dim) * inv_half_dim;
        float v2 = v_norm * v_norm;
        size_t rowBase = (size_t)y * PLASMA_WIDTH;

        for (int x = x0; x < x1; x++) {
            size_t i = rowBase + (size_t)x;
            int src_x = x, src_y = y;

            if (distMode == 2) {
                // Cylindrical: curvatura moderata su X, asse Y piatto
                float u_norm = ((float)x - half_dim) * inv_half_dim;
                float xs = half_dim + (u_norm * (1.0f + v2 * k_cyl_x)) * half_dim;
                if (xs < 0.0f || xs >= 2047.0f) {
                    output[i] = 0xFF000000u;
                    continue;
                }
                src_x = (int)xs;
                src_y = y;
            } else if (distMode == 1) {
                // Barrel: curvatura sferica proporzionata su X e Y
                float u_norm = ((float)x - half_dim) * inv_half_dim;
                float u2 = u_norm * u_norm;
                float xs = half_dim + (u_norm * (1.0f + v2 * k_bar_x)) * half_dim;
                float ys = half_dim + (v_norm * (1.0f + u2 * k_bar_y)) * half_dim;
                if (xs < 0.0f || xs >= 2047.0f || ys < 0.0f || ys >= 2047.0f) {
                    output[i] = 0xFF000000u;
                    continue;
                }
                src_x = (int)xs;
                src_y = (int)ys;
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

static unsigned plasmaBuildCellSurface(PLATOView *self, float deltaTime) {
    plato_terminal_t *term = self->terminal;
    PLATOPlasmaState *p = self->plasma;
    static const float cellProfile[PLASMA_SCALE][PLASMA_SCALE] = {
        { 0.65f, 0.82f, 0.82f, 0.65f },
        { 0.82f, 1.00f, 1.00f, 0.82f },
        { 0.82f, 1.00f, 1.00f, 0.82f },
        { 0.65f, 0.82f, 0.82f, 0.65f }
    };
    const float decay = expf(-deltaTime / p->decayTau);
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
        p->tilesInitialized = YES;
    }
    unsigned count = 0;
    for (int i = 0; i < PLASMA_TILE_COUNT; i++) if (p->dirtyOutput[i]) count++;
    return count;
}


// --- REAL COLOR CRT ENGINE (Stadi A, B, C, D) ---

#define CRT_BG_PIXEL 0xFF000000u // Nero puro allineato allo sfondo unlit e pillarbox

static unsigned crtBuildCellSurface(PLATOView *self, float deltaTime) {
    plato_terminal_t *term = self->terminal;
    PLATOPlasmaState *p = self->plasma;
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
        p->tilesInitialized = YES;
    }

    unsigned count = 0;
    for (int i = 0; i < PLASMA_TILE_COUNT; i++) if (p->dirtyOutput[i]) count++;
    return count;
}

// --- REAL COLOR CRT ENGINE (Profili Selezionabili: Medium / High / Ultra) ---

typedef struct {
    float w_3tap[4][3];
    float scanline[4];
    float bloom_amt;
    float gain;
} CRTBeamProfileParams;

static const CRTBeamProfileParams kCRTProfiles[3] = {
    // 0: Medium (Standard - Default, Autentico 13" CRT, Preferred Look)
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
    // 1: High (Soft Glow)
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
    // 2: Ultra (Vintage Arcade)
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

static void crtComposeTile(PLATOView *self, uint32_t *output, int tileX, int tileY) {
    PLATOPlasmaState *p = self->plasma;

    int x0 = tileX * PLASMA_TILE_SIZE, x1 = x0 + PLASMA_TILE_SIZE;
    int y0 = tileY * PLASMA_TILE_SIZE, y1 = y0 + PLASMA_TILE_SIZE;

    int profileIdx = p->crtBeamLevel;
    if (profileIdx < 0 || profileIdx > 2) profileIdx = 0;
    const CRTBeamProfileParams *prof = &kCRTProfiles[profileIdx];

    static const float mask_tbl[4][3] = {
        { 1.08f, 0.94f, 0.94f }, // R boost (sx=0)
        { 0.94f, 1.08f, 0.94f }, // G boost (sx=1)
        { 0.94f, 0.94f, 1.08f }, // B boost (sx=2)
        { 0.88f, 0.88f, 0.88f }  // Wire/Matrix (sx=3)
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
                if (xs < 0.0f || xs >= 2047.0f) {
                    output[i] = 0xFF000000u;
                    continue;
                }
                src_x = (int)xs;
                src_y = y;
            } else if (distMode == 1) {
                float u_norm = ((float)x - half_dim) * inv_half_dim;
                float u2 = u_norm * u_norm;
                float xs = half_dim + (u_norm * (1.0f + v2 * k_bar_x)) * half_dim;
                float ys = half_dim + (v_norm * (1.0f + u2 * k_bar_y)) * half_dim;
                if (xs < 0.0f || xs >= 2047.0f || ys < 0.0f || ys >= 2047.0f) {
                    output[i] = 0xFF000000u;
                    continue;
                }
                src_x = (int)xs;
                src_y = (int)ys;
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

static void crtRender(PLATOView *self, uint32_t *output, PlasmaFrameProfile *profile) {
    PLATOPlasmaState *p = self->plasma;
    CFTimeInterval totalStart = CACurrentMediaTime(), now = totalStart;
    float deltaTime = p->lastTime > 0.0 ? (float)(now - p->lastTime) : 1.0f / 60.0f;
    p->lastTime = now;
    deltaTime = plasmaClamp(deltaTime, 0.0f, 0.1f);

    CFTimeInterval phaseStart = CACurrentMediaTime();
    unsigned dirtyTiles = crtBuildCellSurface(self, deltaTime);
    CFTimeInterval phaseEnd = CACurrentMediaTime();
    profile->buildMs = plasmaElapsedMs(phaseStart, phaseEnd);
    profile->outputTiles = dirtyTiles;

    if (dirtyTiles == 0) {
        profile->noOp = YES;
        profile->totalMs = plasmaElapsedMs(totalStart, phaseEnd);
        return;
    }

    BOOL fullFrame = (p->crtDistortion != 0) || (dirtyTiles > PLASMA_PARTIAL_LIMIT);
    profile->fullFrame = fullFrame;
    phaseStart = phaseEnd;

    // Bloom/Halation doppio raggio (local r=4 + wide r=12)
    if (fullFrame) {
        plasmaBoxBlur(p->source, p->tmp, p->local, 4, &profile->blur4HorizontalMs, &profile->blur4VerticalMs);
        plasmaBoxBlur(p->source, p->tmp, p->wide, 12, &profile->blur12HorizontalMs, &profile->blur12VerticalMs);
    } else {
        for (int y = 0; y < PLASMA_TILE_ROWS; y++) {
            for (int x = 0; x < PLASMA_TILE_COLS; x++) {
                if (p->dirtyOutput[y * PLASMA_TILE_COLS + x]) {
                    plasmaBoxBlurTile(p->source, p->tmp, p->local, 4, x, y, &profile->blur4HorizontalMs, &profile->blur4VerticalMs);
                    plasmaBoxBlurTile(p->source, p->tmp, p->wide, 12, x, y, &profile->blur12HorizontalMs, &profile->blur12VerticalMs);
                }
            }
        }
    }
    phaseEnd = CACurrentMediaTime();
    profile->blur4Ms = plasmaElapsedMs(phaseStart, phaseEnd);
    phaseStart = phaseEnd;

    // Compositing finale
    if (fullFrame) {
        for (int y = 0; y < PLASMA_TILE_ROWS; y++) {
            for (int x = 0; x < PLASMA_TILE_COLS; x++) {
                crtComposeTile(self, output, x, y);
            }
        }
    } else {
        for (int y = 0; y < PLASMA_TILE_ROWS; y++) {
            for (int x = 0; x < PLASMA_TILE_COLS; x++) {
                if (p->dirtyOutput[y * PLASMA_TILE_COLS + x]) {
                    crtComposeTile(self, output, x, y);
                }
            }
        }
    }
    phaseEnd = CACurrentMediaTime();
    profile->composeMs = plasmaElapsedMs(phaseStart, phaseEnd);
    profile->totalMs = plasmaElapsedMs(totalStart, phaseEnd);
}

static void plasmaRender(PLATOView *self, uint32_t *output, PlasmaFrameProfile *profile) {
    PLATOPlasmaState *p = self->plasma;
    CFTimeInterval totalStart = CACurrentMediaTime(), now = totalStart;
    float deltaTime = p->lastTime > 0.0 ? (float)(now - p->lastTime) : 1.0f / 60.0f;
    p->lastTime = now; deltaTime = plasmaClamp(deltaTime, 0.0f, 0.1f);
    CFTimeInterval phaseStart = CACurrentMediaTime();
    unsigned dirtyTiles = plasmaBuildCellSurface(self, deltaTime);
    CFTimeInterval phaseEnd = CACurrentMediaTime();
    profile->buildMs = plasmaElapsedMs(phaseStart, phaseEnd);
    for (int i = 0; i < PLASMA_TILE_COUNT; i++) if (p->dirtySource[i]) profile->sourceTiles++;
    profile->outputTiles = dirtyTiles;
    if (dirtyTiles == 0) { profile->noOp = YES; profile->totalMs = plasmaElapsedMs(totalStart, phaseEnd); return; }
    BOOL fullFrame = (p->displayMode == PLATODisplayModeRealPlasma && p->plasmaDistortion != 0) || (dirtyTiles > PLASMA_PARTIAL_LIMIT);
    profile->fullFrame = fullFrame;
    phaseStart = phaseEnd;
    if (fullFrame) plasmaBoxBlur(p->source, p->tmp, p->local, 4, &profile->blur4HorizontalMs, &profile->blur4VerticalMs);
    else for (int y = 0; y < PLASMA_TILE_ROWS; y++) for (int x = 0; x < PLASMA_TILE_COLS; x++) if (p->dirtyOutput[y * PLASMA_TILE_COLS + x]) plasmaBoxBlurTile(p->source, p->tmp, p->local, 4, x, y, &profile->blur4HorizontalMs, &profile->blur4VerticalMs);
    phaseEnd = CACurrentMediaTime(); profile->blur4Ms = plasmaElapsedMs(phaseStart, phaseEnd); phaseStart = phaseEnd;
    if (fullFrame) plasmaBoxBlur(p->source, p->tmp, p->wide, 12, &profile->blur12HorizontalMs, &profile->blur12VerticalMs);
    else for (int y = 0; y < PLASMA_TILE_ROWS; y++) for (int x = 0; x < PLASMA_TILE_COLS; x++) if (p->dirtyOutput[y * PLASMA_TILE_COLS + x]) plasmaBoxBlurTile(p->source, p->tmp, p->wide, 12, x, y, &profile->blur12HorizontalMs, &profile->blur12VerticalMs);
    phaseEnd = CACurrentMediaTime(); profile->blur12Ms = plasmaElapsedMs(phaseStart, phaseEnd); phaseStart = phaseEnd;
    if (fullFrame) for (int y = 0; y < PLASMA_TILE_ROWS; y++) for (int x = 0; x < PLASMA_TILE_COLS; x++) plasmaComposeTile(self, output, x, y);
    else for (int y = 0; y < PLASMA_TILE_ROWS; y++) for (int x = 0; x < PLASMA_TILE_COLS; x++) if (p->dirtyOutput[y * PLASMA_TILE_COLS + x]) plasmaComposeTile(self, output, x, y);
    phaseEnd = CACurrentMediaTime(); profile->composeMs = plasmaElapsedMs(phaseStart, phaseEnd); profile->totalMs = plasmaElapsedMs(totalStart, phaseEnd);
}

static double getMachProcessCPUUsage(void) {
    kern_return_t kr;
    thread_array_t thread_list;
    mach_msg_type_number_t thread_count;
    thread_info_data_t thinfo;
    mach_msg_type_number_t thread_info_count;
    thread_basic_info_t basic_info_th;

    kr = task_threads(mach_task_self(), &thread_list, &thread_count);
    if (kr != KERN_SUCCESS) return 0.0;

    double tot_cpu = 0.0;
    for (mach_msg_type_number_t j = 0; j < thread_count; j++) {
        thread_info_count = THREAD_INFO_MAX;
        kr = thread_info(thread_list[j], THREAD_BASIC_INFO, (thread_info_t)thinfo, &thread_info_count);
        if (kr == KERN_SUCCESS) {
            basic_info_th = (thread_basic_info_t)thinfo;
            if (!(basic_info_th->flags & TH_FLAGS_IDLE)) {
                tot_cpu += (double)basic_info_th->cpu_usage / (double)TH_USAGE_SCALE * 100.0;
            }
        }
    }
    vm_deallocate(mach_task_self(), (vm_offset_t)thread_list, thread_count * sizeof(thread_t));
    return tot_cpu;
}

static void* network_worker(void *arg) {
    PLATOView *view = (__bridge PLATOView *)arg;
    uint8_t temp_buf[4096];

    while (view->running) {
        if (view->transport && view->transport->connected) {
            int n = plato_transport_recv(view->transport, temp_buf, sizeof(temp_buf));
            if (n > 0) {
                plato_ringbuf_write(&view->ringbuf, temp_buf, (size_t)n);
            } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                plato_transport_disconnect(view->transport);
                usleep(30000);
            }
        } else {
            usleep(20000);
        }
    }
    return NULL;
}

@interface PLATOView () <CALayerDelegate>
@end

@implementation PLATOView

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        rgbaBuffer = (uint32_t *)calloc(PLASMA_PIXELS, sizeof(uint32_t));
        plasma = (PLATOPlasmaState *)calloc(1, sizeof(PLATOPlasmaState));
        plasma->decayDuration = 0.10;
        plasma->decayTau = 0.010f;
        plasma->crtDecayDuration = 0.020;
        plasma->crtDecayTau = (float)(0.020 / 6.9077);
        plasma->displayMode = PLATODisplayModeRealPlasma;
        plasma->crtBeamLevel = 1; // Default High (Soft Glow)
        plasma->crtDistortion = 2; // Default Cylindrical
        plasma->plasmaDistortion = 2; // Default Cylindrical
        plasmaAllocState(plasma);

        terminal = (plato_terminal_t *)calloc(1, sizeof(plato_terminal_t));
        plato_terminal_init(terminal);

        colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        plato_ringbuf_init(&ringbuf);
        transport = plato_transport_create();
        terminal->transport = transport;

        running = true;
        keyboardReferenceVisible = NO;
        fpsCounterVisible = NO;
        fpsSampleStart = CACurrentMediaTime();
        flowSampleStart = fpsSampleStart;

        self.wantsLayer = YES;
        self.layer.backgroundColor = CGColorGetConstantColor(kCGColorBlack);

        plasmaLayer = [CALayer layer];
        plasmaLayer.actions = @{
            @"contents": [NSNull null],
            @"frame": [NSNull null],
            @"bounds": [NSNull null],
            @"position": [NSNull null]
        };
        plasmaLayer.backgroundColor = CGColorGetConstantColor(kCGColorBlack);
        plasmaLayer.minificationFilter = kCAFilterLinear;
        plasmaLayer.magnificationFilter = kCAFilterLinear;
        [self.layer addSublayer:plasmaLayer];

        overlayLayer = [CALayer layer];
        overlayLayer.delegate = self;
        overlayLayer.actions = @{
            @"contents": [NSNull null],
            @"frame": [NSNull null],
            @"bounds": [NSNull null],
            @"position": [NSNull null]
        };
        [self.layer addSublayer:overlayLayer];

        pasteQueue = dispatch_queue_create("com.fabiomontarsolo.platolives.pasteQueue", DISPATCH_QUEUE_SERIAL);
        pasteCancelled = NO;
        feedBufferLen = 0;
        feedBufferPos = 0;
        paceUntil = 0.0;

        pthread_create(&networkThread, NULL, network_worker, (__bridge void *)self);

        renderTimer = [NSTimer scheduledTimerWithTimeInterval:1.0/60.0
                                                       target:self
                                                     selector:@selector(onFrameTick:)
                                                     userInfo:nil
                                                      repeats:YES];
        fpsTimer = [NSTimer scheduledTimerWithTimeInterval:0.5 target:self selector:@selector(onFPSTick:) userInfo:nil repeats:YES];
    }
    return self;
}

- (void)dealloc {
    running = false;
    [renderTimer invalidate];
    [fpsTimer invalidate];
    if (transport) {
        plato_transport_disconnect(transport);
        pthread_join(networkThread, NULL);
        plato_transport_destroy(transport);
        transport = NULL;
    }
    plato_ringbuf_destroy(&ringbuf);
    if (rgbaBuffer) { free(rgbaBuffer); rgbaBuffer = NULL; }
    if (plasma) { plasmaFreeState(plasma); free(plasma); plasma = NULL; }
    if (terminal) { free(terminal); terminal = NULL; }
    if (colorSpace) { CGColorSpaceRelease(colorSpace); colorSpace = NULL; }
}

- (void)layout {
    [super layout];
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    plasmaLayer.frame = platoDisplayRect(self.bounds);
    overlayLayer.frame = self.bounds;
    [CATransaction commit];
    if (fpsCounterVisible || keyboardReferenceVisible) {
        [overlayLayer setNeedsDisplay];
    }
}

- (void)updateLayerFilters {
    if (!plasmaLayer || !plasma) return;
    NSString *filter = (plasma->displayMode == PLATODisplayModeCrisp || plasma->displayMode == PLATODisplayModeCrispColor) ? kCAFilterNearest : kCAFilterLinear;
    plasmaLayer.minificationFilter = filter;
    plasmaLayer.magnificationFilter = filter;
}

- (void)disconnect {
    if (transport) {
        plato_transport_disconnect(transport);
    }
    [self clearScreen];
}

- (void)clearScreen {
    if (terminal) {
        plato_fb_clear(&terminal->fb);
        terminal->x = 0;
        terminal->y = 496;
        terminal->margin_x = 0;
    }
    if (rgbaBuffer) {
        for (size_t i = 0; i < PLASMA_PIXELS; i++) {
            rgbaBuffer[i] = PLASMA_BG_PIXEL;
        }
    }
    if (plasma) {
        if (plasma->energy) memset(plasma->energy, 0, (size_t)PLATO_WIDTH * PLATO_HEIGHT * sizeof(float));
        if (plasma->source) memset(plasma->source, 0, PLASMA_PIXELS * sizeof(float));
        if (plasma->local) memset(plasma->local, 0, PLASMA_PIXELS * sizeof(float));
        if (plasma->wide) memset(plasma->wide, 0, PLASMA_PIXELS * sizeof(float));
        plasma->animateUntil = 0.0;
        plasma->tilesInitialized = NO;
    }

    if (plasmaLayer) {
        CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, rgbaBuffer, PLASMA_BYTES, NULL);
        CGImageRef img = CGImageCreate(PLASMA_WIDTH, PLASMA_HEIGHT, 8, 32, PLASMA_WIDTH * 4,
                                       colorSpace, kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst,
                                       provider, NULL, false, kCGRenderingIntentDefault);
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        plasmaLayer.frame = platoDisplayRect(self.bounds);
        plasmaLayer.contents = (__bridge id)img;
        [CATransaction commit];
        CGImageRelease(img);
        CGDataProviderRelease(provider);
    }
}

- (void)resetTerminal {
    [self disconnect];
    feedBufferLen = 0;
    feedBufferPos = 0;
    paceUntil = 0.0;
    if (terminal) {
        plato_beep_callback_t beepCb = terminal->beep_callback;
        void *beepCtx = terminal->beep_context;
        plato_metadata_callback_t metaCb = terminal->metadata_callback;
        void *metaCtx = terminal->metadata_context;
        plato_terminal_init(terminal);
        terminal->transport = self->transport;
        terminal->beep_callback = beepCb;
        terminal->beep_context = beepCtx;
        terminal->metadata_callback = metaCb;
        terminal->metadata_context = metaCtx;
    }
    pthread_mutex_lock(&ringbuf.mutex);
    ringbuf.head = 0;
    ringbuf.tail = 0;
    pthread_mutex_unlock(&ringbuf.mutex);
    [self clearScreen];
}

- (void)displayConnectionFailedMessage:(NSString *)errorMsg host:(NSString *)host port:(int)port {
    [self clearScreen];
    if (terminal) {
        NSString *line1 = @"** CONNECTION FAILED **";
        NSString *line2 = [NSString stringWithFormat:@"%@:%d", host, port];
        NSString *line3 = [errorMsg uppercaseString];

        int y1 = 280;
        int x1 = (PLATO_WIDTH - (int)[line1 length] * 8) / 2;
        for (NSUInteger i = 0; i < [line1 length]; i++) {
            plato_draw_char(&terminal->fb, &terminal->font, PLATO_CHARSET_M0,
                            [line1 characterAtIndex:i], x1 + (int)i * 8, y1,
                            PLATO_SCREEN_WRITE, 0);
        }

        int y2 = 256;
        int x2 = (PLATO_WIDTH - (int)[line2 length] * 8) / 2;
        for (NSUInteger i = 0; i < [line2 length]; i++) {
            plato_draw_char(&terminal->fb, &terminal->font, PLATO_CHARSET_M0,
                            [line2 characterAtIndex:i], x2 + (int)i * 8, y2,
                            PLATO_SCREEN_WRITE, 0);
        }

        int y3 = 232;
        int x3 = (PLATO_WIDTH - (int)[line3 length] * 8) / 2;
        for (NSUInteger i = 0; i < [line3 length]; i++) {
            plato_draw_char(&terminal->fb, &terminal->font, PLATO_CHARSET_M0,
                            [line3 characterAtIndex:i], x3 + (int)i * 8, y3,
                            PLATO_SCREEN_WRITE, 0);
        }

        if (plasma) {
            plasma->tilesInitialized = NO;
            plasma->animateUntil = CACurrentMediaTime() + plasma->decayDuration;
        }
        [self refreshDisplay];
    }

    if (self.window) {
        [self.window setTitle:[NSString stringWithFormat:@"PlatoLives - Connection Failed (%@:%d)", host, port]];
    }
}

- (void)connectToHost:(NSString *)host port:(int)port {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        bool ok = plato_transport_connect(self->transport, [host UTF8String], port);
        if (ok) {
            NSLog(@"[PLATO] Connesso con successo a %@:%d", host, port);
        } else {
            const char *err = (self->transport && self->transport->last_error[0]) ? self->transport->last_error : "Connection refused";
            NSString *errStr = [NSString stringWithUTF8String:err];
            NSLog(@"[PLATO] Errore di connessione a %@:%d (%@)", host, port, errStr);
            dispatch_async(dispatch_get_main_queue(), ^{
                [self displayConnectionFailedMessage:errStr host:host port:port];
            });
        }
    });
}

- (void)processIncomingData {
    if (!terminal || !plasma || !running) return;

    CFTimeInterval now = CACurrentMediaTime();
    if (now < self->paceUntil) return;

    while (running) {
        if (feedBufferPos >= feedBufferLen) {
            feedBufferPos = 0;
            feedBufferLen = plato_ringbuf_read(&ringbuf, feedBuffer, sizeof(feedBuffer));
            if (feedBufferLen == 0) break;
            flowFeedCount++;
            flowReadBytes += feedBufferLen;
        }

        terminal->delay_requested = false;
        size_t remaining = feedBufferLen - feedBufferPos;
        size_t consumed = plato_terminal_feed(terminal, &feedBuffer[feedBufferPos], remaining);
        feedBufferPos += consumed;

        if (terminal->delay_requested) {
            terminal->delay_requested = false;

            /* Se c'è stato del disegno, forziamo il rendering del fotogramma a video */
            if (terminal->fb.dirty) {
                terminal->fb.dirty = false;
                if (plasma->displayMode != PLATODisplayModeCrisp && plasma->displayMode != PLATODisplayModeCrispColor) {
                    NSTimeInterval dur = (plasma->displayMode == PLATODisplayModeRealColorCRT) ? plasma->crtDecayDuration : plasma->decayDuration;
                    plasma->animateUntil = now + dur;
                }
                [self refreshDisplay];
            }

            /* Pausa di pacing di 8 ms (conforme alla formula storica di pterm) */
            self->paceUntil = now + 0.008;

            __weak PLATOView *weakSelf = self;
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(8 * NSEC_PER_MSEC)), dispatch_get_main_queue(), ^{
                [weakSelf processIncomingData];
            });
            return;
        }
    }

    if (terminal->fb.dirty) {
        terminal->fb.dirty = false;
        if (plasma->displayMode != PLATODisplayModeCrisp && plasma->displayMode != PLATODisplayModeCrispColor) {
            NSTimeInterval dur = (plasma->displayMode == PLATODisplayModeRealColorCRT) ? plasma->crtDecayDuration : plasma->decayDuration;
            plasma->animateUntil = now + dur;
        }
        [self refreshDisplay];
    }
}

- (void)onFrameTick:(NSTimer *)timer {
    if (!terminal || !plasma) return;
    flowTickCount++;
    size_t ringBefore = plato_ringbuf_available(&ringbuf);
    if (ringBefore > flowRingMaximum) flowRingMaximum = ringBefore;

    /* 1. Ingestione logica asincrona disaccoppiata (si ferma solo lei se incontra NUL) */
    [self processIncomingData];

    flowRingEnd = plato_ringbuf_available(&ringbuf);
    if (flowRingEnd > flowRingMaximum) flowRingMaximum = flowRingEnd;

    /* 2. Il motore Real Plasma a 60 FPS NON SI FERMA MAI: calcola il decadimento gas continuamente */
    if (plasma->displayMode != PLATODisplayModeCrisp && plasma->displayMode != PLATODisplayModeCrispColor && CACurrentMediaTime() < plasma->animateUntil) {
        [self refreshDisplay];
    }
}

- (void)onFPSTick:(NSTimer *)timer {
    CFTimeInterval now = CACurrentMediaTime();
    double elapsed = now - fpsSampleStart;
    if (elapsed > 0.0) {
        fpsTotal = fpsTotalFrames / elapsed;
        fpsPartial = fpsPartialFrames / elapsed;
        fpsFull = fpsFullFrames / elapsed;
    }
    fpsTotalFrames = fpsPartialFrames = fpsFullFrames = 0;
    fpsSampleStart = now;

    cpuUsagePercent = getMachProcessCPUUsage();

    double flowElapsed = now - flowSampleStart;
    if (plasmaLiveProfileEnabled && flowElapsed > 0.0) {
        NSLog(@"[PLATO FLOW] interval=%.3fs ticks=%llu feeds=%llu read-bytes=%llu bytes-per-sec=%.1f ring-max=%zu ring-end=%zu",
              flowElapsed, (unsigned long long)flowTickCount, (unsigned long long)flowFeedCount,
              (unsigned long long)flowReadBytes, flowReadBytes / flowElapsed, flowRingMaximum, flowRingEnd);
    }
    flowTickCount = flowFeedCount = flowReadBytes = 0;
    flowRingMaximum = flowRingEnd = 0;
    flowSampleStart = now;
    if (fpsCounterVisible) [overlayLayer setNeedsDisplay];
}

- (BOOL)acceptsFirstResponder { return YES; }

- (void)setTerminal:(plato_terminal_t *)term {
    if (term != terminal) {
        if (terminal) free(terminal);
        terminal = term;
    }
    if (terminal) terminal->transport = self->transport;
    [self refreshDisplay];
}

- (void)refreshDisplay {
    if (!terminal || !plasma || !plasmaAllocState(plasma)) return;
    PlasmaFrameProfile profile = {0};
    CFTimeInterval totalStart = CACurrentMediaTime();
    if (plasma->displayMode == PLATODisplayModeCrisp || plasma->displayMode == PLATODisplayModeCrispColor) {
        CFTimeInterval phaseStart = CACurrentMediaTime();
        plato_terminal_render_rgba(terminal, plasma->logical);
        CFTimeInterval phaseEnd = CACurrentMediaTime();
        profile.logicalMs = plasmaElapsedMs(phaseStart, phaseEnd);
        phaseStart = phaseEnd;
        plasmaExpandCrisp(plasma->logical, rgbaBuffer);
        phaseEnd = CACurrentMediaTime();
        profile.expandMs = plasmaElapsedMs(phaseStart, phaseEnd);
        profile.totalMs = plasmaElapsedMs(totalStart, phaseEnd);
    } else if (plasma->displayMode == PLATODisplayModeRealColorCRT) {
        crtRender(self, rgbaBuffer, &profile);
    } else {
        plasmaRender(self, rgbaBuffer, &profile);
        if (plasma->displayMode == PLATODisplayModeSplit) {
            CFTimeInterval phaseStart = CACurrentMediaTime();
            plato_terminal_render_rgba(terminal, plasma->logical);
            CFTimeInterval phaseEnd = CACurrentMediaTime();
            profile.logicalMs = plasmaElapsedMs(phaseStart, phaseEnd);
            phaseStart = phaseEnd;
            plasmaExpandCrisp(plasma->logical, plasma->crisp);
            phaseEnd = CACurrentMediaTime();
            profile.expandMs = plasmaElapsedMs(phaseStart, phaseEnd);
            size_t halfBytes = (PLASMA_WIDTH / 2) * sizeof(uint32_t);
            for (int y = 0; y < PLASMA_HEIGHT; y++) {
                memcpy(rgbaBuffer + (size_t)y * PLASMA_WIDTH + PLASMA_WIDTH / 2,
                       plasma->crisp + (size_t)y * PLASMA_WIDTH + PLASMA_WIDTH / 2, halfBytes);
            }
            profile.totalMs = plasmaElapsedMs(totalStart, CACurrentMediaTime());
        }
    }
    plasmaProfileFrame(&profile, plasma->displayMode);
    plasmaLiveProfileFrame(&profile, plasma->displayMode);
    if (plasma->displayMode == PLATODisplayModeCrisp || plasma->displayMode == PLATODisplayModeCrispColor || !profile.noOp) {
        fpsTotalFrames++;
        if (plasma->displayMode != PLATODisplayModeCrisp && plasma->displayMode != PLATODisplayModeCrispColor) {
            if (profile.fullFrame) fpsFullFrames++; else fpsPartialFrames++;
        }

        CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, rgbaBuffer, PLASMA_BYTES, NULL);
        CGImageRef img = CGImageCreate(PLASMA_WIDTH, PLASMA_HEIGHT, 8, 32, PLASMA_WIDTH * 4,
                                       colorSpace, kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst,
                                       provider, NULL, false, kCGRenderingIntentDefault);
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        plasmaLayer.frame = platoDisplayRect(self.bounds);
        plasmaLayer.contents = (__bridge id)img;
        [CATransaction commit];
        CGImageRelease(img);
        CGDataProviderRelease(provider);
    }
}

- (void)setDisplayCrisp {
    if (!plasma) return;
    plasma->displayMode = PLATODisplayModeCrisp;
    plasma->animateUntil = 0.0;
    if (terminal) plato_terminal_set_color_mode(terminal, false);
    [self updateLayerFilters];
    [self refreshDisplay];
}

- (void)setDisplayCrispColor {
    if (!plasma) return;
    plasma->displayMode = PLATODisplayModeCrispColor;
    plasma->animateUntil = 0.0;
    if (terminal) plato_terminal_set_color_mode(terminal, true);
    [self updateLayerFilters];
    [self refreshDisplay];
}
- (void)setDisplayRealColorCRT {
    if (!plasma) return;
    plasma->displayMode = PLATODisplayModeRealColorCRT;
    plasma->lastTime = 0.0;
    plasma->tilesInitialized = NO;
    plasma->animateUntil = CACurrentMediaTime() + plasma->crtDecayDuration;
    if (terminal) plato_terminal_set_color_mode(terminal, true);
    [self updateLayerFilters];
    [self refreshDisplay];
}

- (void)setCRTBeamLevel:(NSInteger)level {
    if (!plasma) return;
    if (level < 0) level = 0;
    if (level > 2) level = 2;
    plasma->crtBeamLevel = (int)level;
    plasma->tilesInitialized = NO;
    plasma->animateUntil = CACurrentMediaTime() + 0.10;
    [self refreshDisplay];
}

- (NSInteger)crtBeamLevel {
    return plasma ? plasma->crtBeamLevel : 0;
}

- (void)setCRTDistortion:(NSInteger)distortion {
    if (!plasma) return;
    if (distortion < 0) distortion = 0;
    if (distortion > 2) distortion = 2;
    plasma->crtDistortion = (int)distortion;
    plasma->tilesInitialized = NO;
    plasma->animateUntil = CACurrentMediaTime() + 0.10;
    [self refreshDisplay];
}

- (NSInteger)crtDistortion {
    return plasma ? plasma->crtDistortion : 2;
}

- (void)setPlasmaDistortion:(NSInteger)distortion {
    if (!plasma) return;
    if (distortion < 0) distortion = 0;
    if (distortion > 2) distortion = 2;
    plasma->plasmaDistortion = (int)distortion;
    plasma->tilesInitialized = NO;
    plasma->animateUntil = CACurrentMediaTime() + plasma->decayDuration;
    [self refreshDisplay];
}

- (NSInteger)plasmaDistortion {
    return plasma ? plasma->plasmaDistortion : 2;
}


- (void)setDisplayRealPlasma {
    if (!plasma) return;
    plasma->displayMode = PLATODisplayModeRealPlasma;
    plasma->lastTime = 0.0;
    plasma->tilesInitialized = NO;
    plasma->animateUntil = CACurrentMediaTime() + plasma->decayDuration;
    if (terminal) plato_terminal_set_color_mode(terminal, false);
    [self updateLayerFilters];
    [self refreshDisplay];
}

- (void)setDisplaySplit {
    if (!plasma) return;
    plasma->displayMode = PLATODisplayModeSplit;
    plasma->lastTime = 0.0;
    plasma->tilesInitialized = NO;
    plasma->animateUntil = CACurrentMediaTime() + plasma->decayDuration;
    if (terminal) plato_terminal_set_color_mode(terminal, false);
    [self updateLayerFilters];
    [self refreshDisplay];
}

- (void)setPlasmaProfilePath:(NSString *)path {
    plasmaProfilePath = [path copy];
    if (plasmaProfilePath) [[NSData data] writeToFile:plasmaProfilePath atomically:YES];
}

- (void)setDiagnosticLogEnabled:(BOOL)enabled {
    if (transport) plato_transport_set_logging(transport, enabled);
}

- (void)writePlasmaProfileWithTag:(NSString *)tag {
    PLATODisplayMode mode = plasma ? plasma->displayMode : PLATODisplayModeRealPlasma;
    NSString *line = plasmaProfileLine(tag, mode);
    NSLog(@"%@", line);
    if (!plasmaProfilePath) return;
    NSString *record = [line stringByAppendingString:@"\n"];
    NSFileHandle *handle = [NSFileHandle fileHandleForWritingAtPath:plasmaProfilePath];
    if (!handle) return;
    [handle seekToEndOfFile];
    [handle writeData:[record dataUsingEncoding:NSUTF8StringEncoding]];
    [handle closeFile];
}

- (void)setRendererPerformanceLogEnabled:(BOOL)enabled {
    plasmaLiveProfileEnabled = enabled;
    plasmaLiveProfileReset(enabled ? CACurrentMediaTime() : 0.0);
    flowTickCount = flowFeedCount = flowReadBytes = 0;
    flowRingMaximum = flowRingEnd = 0;
    flowSampleStart = CACurrentMediaTime();
}

- (void)setFPSCounterVisible:(BOOL)visible {
    fpsCounterVisible = visible;
    fpsTotalFrames = fpsPartialFrames = fpsFullFrames = 0;
    fpsTotal = fpsPartial = fpsFull = 0.0;
    fpsSampleStart = CACurrentMediaTime();
    [overlayLayer setNeedsDisplay];
}

- (void)setPlasmaDecayDuration:(NSTimeInterval)duration {
    if (!plasma) return;
    if (duration < 0.02) duration = 0.02;
    plasma->decayDuration = duration;
    plasma->decayTau = (float)(duration / 10.0);
    plasma->animateUntil = CACurrentMediaTime() + plasma->decayDuration;
    [self refreshDisplay];
}

- (NSInteger)currentDisplayMode {
    return plasma ? (NSInteger)plasma->displayMode : 1;
}

- (NSTimeInterval)plasmaDecayDuration {
    return plasma ? plasma->decayDuration : 0.10;
}

- (void)setCRTDecayDuration:(NSTimeInterval)duration {
    if (!plasma) return;
    if (duration < 0.02) duration = 0.02;
    plasma->crtDecayDuration = duration;
    plasma->crtDecayTau = (float)(duration / 10.0);
    plasma->tilesInitialized = NO;
    plasma->animateUntil = CACurrentMediaTime() + plasma->crtDecayDuration;
    [self refreshDisplay];
}

- (NSTimeInterval)crtDecayDuration {
    return plasma ? plasma->crtDecayDuration : 0.020;
}

- (void)setKeyboardReferenceVisible:(BOOL)visible {
    keyboardReferenceVisible = visible;
    [overlayLayer setNeedsDisplay];
}

- (BOOL)sendTestText:(NSString *)text error:(NSString **)error {
    if (!transport || !transport->connected) {
        if (error) *error = @"transport not connected";
        return NO;
    }
    NSData *data = [text dataUsingEncoding:NSUTF8StringEncoding];
    if (!data || [data length] == 0) return YES;
    plato_transport_send(transport, [data bytes], [data length]);
    return YES;
}

- (BOOL)sendTestKey:(NSString *)name error:(NSString **)error {
    if (!terminal || !transport || !transport->connected) {
        if (error) *error = @"terminal transport not connected";
        return NO;
    }
    NSString *key = [[name uppercaseString] stringByReplacingOccurrencesOfString:@"_" withString:@"-"];
    BOOL shifted = [key hasPrefix:@"SHIFT-"];
    if (shifted) key = [key substringFromIndex:6];
    int code;
    if ([key isEqualToString:@"NEXT"]) code = PLATO_KEY_NEXT;
    else if ([key isEqualToString:@"BACK"]) code = PLATO_KEY_BACK;
    else if ([key isEqualToString:@"STOP"]) code = PLATO_KEY_STOP;
    else if ([key isEqualToString:@"ERASE"]) code = PLATO_KEY_ERASE;
    else if ([key isEqualToString:@"HELP"]) code = PLATO_KEY_HELP;
    else if ([key isEqualToString:@"LAB"]) code = PLATO_KEY_LAB;
    else if ([key isEqualToString:@"DATA"]) code = PLATO_KEY_DATA;
    else if ([key isEqualToString:@"EDIT"]) code = PLATO_KEY_EDIT;
    else if ([key isEqualToString:@"MICRO"]) code = PLATO_KEY_MICRO;
    else if ([key isEqualToString:@"FONT"]) code = PLATO_KEY_FONT;
    else if ([key isEqualToString:@"SUPER"]) code = PLATO_KEY_SUPER;
    else if ([key isEqualToString:@"SUB"]) code = PLATO_KEY_SUB;
    else if ([key isEqualToString:@"ACCESS"]) code = PLATO_KEY_ACCESS;
    else if ([key isEqualToString:@"TERM"]) code = PLATO_KEY_TERM;
    else if ([key isEqualToString:@"ANS"]) code = PLATO_KEY_ANS;
    else if ([key isEqualToString:@"SQUARE"]) code = PLATO_KEY_SQUARE;
    else {
        if (error) *error = [NSString stringWithFormat:@"unknown PLATO key: %@", name];
        return NO;
    }
    plato_protocol_send_key(terminal, plato_keyboard_keycode(code, shifted));
    return YES;
}

- (void)pasteText:(NSString *)text {
    if (!text || [text length] == 0 || !transport || !transport->connected) return;

    NSString *normalized = [text stringByReplacingOccurrencesOfString:@"\r\n" withString:@"\n"];
    normalized = [normalized stringByReplacingOccurrencesOfString:@"\r" withString:@"\n"];

    pasteCancelled = NO;
    dispatch_async(pasteQueue, ^{
        NSUInteger len = [normalized length];
        for (NSUInteger i = 0; i < len; i++) {
            if (self->pasteCancelled || !self->running || !self->transport || !self->transport->connected) {
                break;
            }
            unichar ch = [normalized characterAtIndex:i];
            if (ch == '\n') {
                dispatch_async(dispatch_get_main_queue(), ^{
                    if (self->terminal) {
                        plato_protocol_send_key(self->terminal, plato_keyboard_keycode(PLATO_KEY_NEXT, false));
                    }
                });
                usleep(500000);
            } else if (ch >= 32 && ch <= 126) {
                uint8_t byte = (uint8_t)ch;
                plato_transport_send(self->transport, &byte, 1);
                usleep(250000);
            }
        }
    });
}

- (void)cancelPaste {
    pasteCancelled = YES;
}

- (void)copyScreenToPasteboard {
    if (!rgbaBuffer) return;
    [self refreshDisplay];
    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, rgbaBuffer, PLASMA_BYTES, NULL);
    CGImageRef image = CGImageCreate(PLASMA_WIDTH, PLASMA_HEIGHT, 8, 32, PLASMA_WIDTH * 4, colorSpace,
                                     kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst,
                                     provider, NULL, false, kCGRenderingIntentDefault);
    if (!image) {
        CGDataProviderRelease(provider);
        return;
    }
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithCGImage:image];
    NSData *png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    CGImageRelease(image);
    CGDataProviderRelease(provider);

    if (png) {
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        [pb setData:png forType:NSPasteboardTypePNG];
    }
}

- (BOOL)saveRenderedScreenshot:(NSString *)path error:(NSString **)error {
    if (!terminal || !rgbaBuffer) {
        if (error) *error = @"renderer not initialized";
        return NO;
    }
    [self refreshDisplay];
    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, rgbaBuffer, PLASMA_BYTES, NULL);
    CGImageRef image = CGImageCreate(PLASMA_WIDTH, PLASMA_HEIGHT, 8, 32, PLASMA_WIDTH * 4, colorSpace,
                                     kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst,
                                     provider, NULL, false, kCGRenderingIntentDefault);
    if (!image) {
        CGDataProviderRelease(provider);
        if (error) *error = @"cannot create screenshot image";
        return NO;
    }
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithCGImage:image];
    NSData *png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    CGImageRelease(image); CGDataProviderRelease(provider);
    NSError *writeError = nil;
    BOOL ok = [png writeToFile:path options:NSDataWritingAtomic error:&writeError];
    if (!ok && error) *error = [writeError localizedDescription];
    return ok;
}

- (void)drawLayer:(CALayer *)layer inContext:(CGContextRef)ctx {
    if (layer != overlayLayer) return;
    if (!fpsCounterVisible && !keyboardReferenceVisible) return;

    NSGraphicsContext *nsCtx = [NSGraphicsContext graphicsContextWithCGContext:ctx flipped:NO];
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:nsCtx];

    CGRect destRect = platoDisplayRect(self.bounds);
    if (fpsCounterVisible) {
        PLATODisplayMode curMode = plasma ? plasma->displayMode : PLATODisplayModeRealPlasma;
        NSString *userInfo = @"";
        if (terminal && terminal->user_name[0] && terminal->user_group[0]) {
            userInfo = [NSString stringWithFormat:@"USER    %s/%s (%s)\n", terminal->user_name, terminal->user_group, terminal->user_station];
        } else if (terminal && terminal->user_station[0]) {
            userInfo = [NSString stringWithFormat:@"SLOT    %s\n", terminal->user_station];
        }

        NSString *fpsText = curMode == PLATODisplayModeCrisp
            ? [NSString stringWithFormat:@"%@FPS     %5.1f\nCPU     %5.1f%%", userInfo, fpsTotal, cpuUsagePercent]
            : [NSString stringWithFormat:@"%@FPS     %5.1f\nPARTIAL %5.1f\nFULL    %5.1f\nCPU     %5.1f%%", userInfo, fpsTotal, fpsPartial, fpsFull, cpuUsagePercent];
        NSDictionary *fpsAttributes = @{
            NSFontAttributeName: [NSFont monospacedSystemFontOfSize:13.0 weight:NSFontWeightSemibold],
            NSForegroundColorAttributeName: [NSColor colorWithCalibratedRed:1.0 green:0.62 blue:0.20 alpha:0.98]
        };
        NSSize textSize = [fpsText sizeWithAttributes:fpsAttributes];
        NSRect fpsPanel = NSMakeRect(self.bounds.size.width - textSize.width - 24.0,
                                     self.bounds.size.height - textSize.height - 20.0,
                                     textSize.width + 12.0, textSize.height + 8.0);
        [[NSColor colorWithCalibratedWhite:0.0 alpha:0.68] setFill];
        [[NSBezierPath bezierPathWithRoundedRect:fpsPanel xRadius:5.0 yRadius:5.0] fill];
        [fpsText drawAtPoint:NSMakePoint(fpsPanel.origin.x + 6.0, fpsPanel.origin.y + 4.0) withAttributes:fpsAttributes];
    }
    CGFloat freeRight = self.bounds.size.width - CGRectGetMaxX(destRect);
    if (keyboardReferenceVisible && freeRight > 40.0) {
        NSRect panel = NSMakeRect(CGRectGetMaxX(destRect) + 12.0, 12.0, freeRight - 24.0, self.bounds.size.height - 24.0);
        CGFloat fontSize = 14.0;
        NSDictionary *attributes = nil;
        NSString *text = PLATOKeyboardReferenceText();
        for (; fontSize >= 7.0; fontSize -= 0.5) {
            attributes = @{
                NSFontAttributeName: [NSFont monospacedSystemFontOfSize:fontSize weight:NSFontWeightRegular],
                NSForegroundColorAttributeName: [NSColor colorWithCalibratedRed:1.0 green:0.48 blue:0.12 alpha:0.92]
            };
            NSRect needed = [text boundingRectWithSize:panel.size options:NSStringDrawingUsesLineFragmentOrigin attributes:attributes];
            if (needed.size.width <= panel.size.width && needed.size.height <= panel.size.height) break;
        }
        if (fontSize >= 7.0) [text drawInRect:panel withAttributes:attributes];
    }

    [NSGraphicsContext restoreGraphicsState];
}

- (void)mouseDown:(NSEvent *)event {
    if (!terminal || !transport || !transport->connected) return;
    NSPoint loc = [self convertPoint:[event locationInWindow] fromView:nil];
    CGRect displayRect = platoDisplayRect(self.bounds);
    CGFloat scale = displayRect.size.width / PLATO_WIDTH;
    CGFloat drawW = displayRect.size.width;
    CGFloat drawH = displayRect.size.height;
    CGFloat drawX = displayRect.origin.x;
    CGFloat drawY = displayRect.origin.y;
    if (loc.x >= drawX && loc.x < drawX + drawW &&
        loc.y >= drawY && loc.y < drawY + drawH) {
        int plato_x = (int)((loc.x - drawX) / scale);
        int plato_y = (int)((loc.y - drawY) / scale);

        int activeDist = 0;
        if (plasma) {
            if (plasma->displayMode == PLATODisplayModeRealColorCRT) {
                activeDist = plasma->crtDistortion;
            } else if (plasma->displayMode == PLATODisplayModeRealPlasma) {
                activeDist = plasma->plasmaDistortion;
            }
        }
        if (activeDist != 0) {
            float u = ((float)plato_x - 256.0f) / 256.0f;
            float v = ((float)plato_y - 256.0f) / 256.0f;
            float u_src = u, v_src = v;
            if (activeDist == 2) {
                u_src = u * (1.0f + (v * v) * 0.018f);
            } else if (activeDist == 1) {
                u_src = u * (1.0f + (v * v) * 0.018f);
                v_src = v * (1.0f + (u * u) * 0.012f);
            }
            plato_x = (int)(256.0f + u_src * 256.0f);
            plato_y = (int)(256.0f + v_src * 256.0f);
        }

        if (plato_x < 0) plato_x = 0;
        if (plato_x >= PLATO_WIDTH) plato_x = PLATO_WIDTH - 1;
        if (plato_y < 0) plato_y = 0;
        if (plato_y >= PLATO_HEIGHT) plato_y = PLATO_HEIGHT - 1;

        plato_protocol_send_touch(terminal, (int16_t)plato_x, (int16_t)plato_y);
    }
}

- (void)keyDown:(NSEvent *)event {
    NSEventModifierFlags flags = [event modifierFlags];
    bool shift = (flags & NSEventModifierFlagShift) != 0;
    bool ctrl  = (flags & NSEventModifierFlagControl) != 0;
    bool alt   = (flags & NSEventModifierFlagOption) != 0;
    bool cmd   = (flags & NSEventModifierFlagCommand) != 0;

    NSString *chars = [event characters];
    NSString *unmod = [[event charactersIgnoringModifiers] lowercaseString];
    if ([chars length] == 0) return;
    unichar c = [chars characterAtIndex:0];
    unichar uc = ([unmod length] > 0) ? [unmod characterAtIndex:0] : 0;

    uint8_t sendBuf[8];
    size_t len = 0;
    uint16_t platoKey = UINT16_MAX;

    // 1. Tasti funzione F1..F12
    if (c >= NSF1FunctionKey && c <= NSF12FunctionKey) {
        switch (c) {
            case NSF1FunctionKey:
            case NSF6FunctionKey:  platoKey = plato_keyboard_keycode(PLATO_KEY_HELP, shift); break;
            case NSF2FunctionKey:
            case NSF7FunctionKey:  platoKey = plato_keyboard_keycode(PLATO_KEY_LAB, shift); break;
            case NSF3FunctionKey:
            case NSF9FunctionKey:  platoKey = plato_keyboard_keycode(PLATO_KEY_DATA, shift); break;
            case NSF4FunctionKey:
            case NSF10FunctionKey: platoKey = plato_keyboard_keycode(PLATO_KEY_STOP, shift); break;
            case NSF5FunctionKey:  platoKey = plato_keyboard_keycode(PLATO_KEY_EDIT, shift); break;
            case NSF8FunctionKey:  platoKey = plato_keyboard_keycode(PLATO_KEY_BACK, shift); break;
            case NSF11FunctionKey:
                [NSApp sendAction:@selector(toggleMainFullScreen:) to:[NSApp delegate] from:self];
                return;
            case NSF12FunctionKey:
                [NSApp sendAction:@selector(toggleFPSCounter:) to:[NSApp delegate] from:self];
                return;
        }
    }
    // 2. Tasti di controllo e opzione (Ctrl / Option)
    else if ((ctrl || alt) && !cmd) {
        if (alt && !ctrl && c == NSLeftArrowFunctionKey) {
            // Option + Freccia Sinistra: Freccia di assegnamento (<=)
            platoKey = shift ? 0x2D : 0x0D;
        } else if (ctrl && (uc >= '0' && uc <= '9')) {
            // Ctrl+[0..9] e Ctrl+Shift+[0..9] (PLATO shifted number row: > [ ] $ % _ ' * ( <)
            int digit = uc - '0';
            platoKey = (uint16_t)(digit | 0x20);
        } else if (ctrl && shift && (uc == '!' || uc == '@' || uc == '#' || uc == '$' ||
                                     uc == '%' || uc == '^' || uc == '&' || uc == '*' ||
                                     uc == '(' || uc == ')')) {
            int digit = 0;
            switch (uc) {
                case '!': digit = 1; break;
                case '@': digit = 2; break;
                case '#': digit = 3; break;
                case '$': digit = 4; break;
                case '%': digit = 5; break;
                case '^': digit = 6; break;
                case '&': digit = 7; break;
                case '*': digit = 8; break;
                case '(': digit = 9; break;
                case ')': digit = 0; break;
            }
            platoKey = (uint16_t)(digit | 0x20);
        } else {
            switch (uc) {
                case 'a': platoKey = plato_keyboard_keycode(PLATO_KEY_ANS, shift); break;
                case 'b': platoKey = plato_keyboard_keycode(PLATO_KEY_BACK, shift); break;
                case 'c': platoKey = plato_keyboard_keycode(PLATO_KEY_COPY, shift); break;
                case 'd': platoKey = plato_keyboard_keycode(PLATO_KEY_DATA, shift); break;
                case 'e': platoKey = plato_keyboard_keycode(PLATO_KEY_EDIT, shift); break;
                case 'f': platoKey = plato_keyboard_keycode(PLATO_KEY_FONT, shift); break;
                case 'g': platoKey = 0x0B; break; // Simbolo Divisione (÷)
                case 'h': platoKey = plato_keyboard_keycode(PLATO_KEY_HELP, shift); break;
                case 'l': platoKey = plato_keyboard_keycode(PLATO_KEY_LAB, shift); break;
                case 'm': platoKey = plato_keyboard_keycode(PLATO_KEY_MICRO, shift); break;
                case 'n': platoKey = plato_keyboard_keycode(PLATO_KEY_NEXT, shift); break;
                case 'p': platoKey = plato_keyboard_keycode(PLATO_KEY_SUPER, shift); break;
                case 'q': platoKey = plato_keyboard_keycode(PLATO_KEY_SQUARE, shift); break;
                case 'r': platoKey = plato_keyboard_keycode(PLATO_KEY_ERASE, shift); break;
                case 's': platoKey = plato_keyboard_keycode(PLATO_KEY_STOP, shift); break;
                case 't': platoKey = plato_keyboard_keycode(PLATO_KEY_TERM, shift); break;
                case 'x': platoKey = 0x0A; break; // Simbolo Moltiplicazione (×)
                case 'y': platoKey = plato_keyboard_keycode(PLATO_KEY_SUB, shift); break;
                case '/': platoKey = plato_keyboard_keycode(PLATO_KEY_ANS, shift); break;
            }
        }
    }
    // 3. Frecce e tasti di scorrimento
    else if (c == NSLeftArrowFunctionKey) {
        platoKey = plato_keyboard_keycode(PLATO_KEY_ERASE, shift);
    }
    else if (c == NSRightArrowFunctionKey || c == '\t' || c == 9) {
        sendBuf[0] = 0x09; // TAB fisico o Freccia Destra
        len = 1;
    }
    else if (c == NSUpArrowFunctionKey || c == NSPageUpFunctionKey) {
        platoKey = plato_keyboard_keycode(PLATO_KEY_SUPER, shift);
    }
    else if (c == NSDownArrowFunctionKey || c == NSPageDownFunctionKey) {
        platoKey = plato_keyboard_keycode(PLATO_KEY_SUB, shift);
    }
    // 4. Return / Invio
    else if (c == 13 || c == 3 || c == 10) {
        platoKey = plato_keyboard_keycode(PLATO_KEY_NEXT, shift);
    }
    // 5. Backspace / Delete
    else if (c == 127 || c == 8 || c == NSDeleteFunctionKey) {
        platoKey = plato_keyboard_keycode(PLATO_KEY_ERASE, shift);
    }
    // 6. Escape -> BACK
    else if (c == 27) {
        platoKey = plato_keyboard_keycode(PLATO_KEY_BACK, shift);
    }
    // 7. ASCII stampabili
    else if (c >= 32 && c <= 126) {
        sendBuf[0] = (uint8_t)c;
        len = 1;
    }

    if (platoKey != UINT16_MAX && terminal) {
        plato_protocol_send_key(terminal, platoKey);
    } else if (len > 0 && transport && transport->connected) {
        plato_transport_send(transport, sendBuf, len);
    }
}

- (void)keyUp:(NSEvent *)event {
    // Gestione rilasci se necessari in futuro
}

@end
