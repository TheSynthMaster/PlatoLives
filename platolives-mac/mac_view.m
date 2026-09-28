#import "mac_view.h"
#import "mac_window.h"
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
           @"NEW TAB       Cmd+T\n"
           @"NEXT TAB      Cmd+]\n"
           @"PREV TAB      Cmd+[\n"
           @"GO TO TAB     Cmd+1..Cmd+8\n"
           @"LAST TAB      Cmd+9\n"
           @"CLOSE WINDOW  Cmd+W\n"
           @"MINIMIZE      Cmd+M\n"
           @"LIVE BUFFER   Cmd+Shift+B\n"
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

// --- MOTORE OTTICO CONDIVISO (libplato) ---
// Tutto il calcolo dei fosfori, bloom e fisica CRT/Plasma e' centralizzato in src/plato_optical.c

static NSString *plasmaProfilePath = nil;
static BOOL plasmaLiveProfileEnabled = NO;
static inline void plasmaLiveProfileReset(CFTimeInterval now) { (void)now; }

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
@synthesize scriptRunner = scriptRunner;

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        rgbaBuffer = (uint32_t *)calloc(PLASMA_PIXELS, sizeof(uint32_t));
        optical = plato_optical_create();
        memset(&opticalProfile, 0, sizeof(opticalProfile));
        opticalProfile.display_mode = 0; /* 0: Real Plasma in libplato */
        opticalProfile.persistence_ms = 100;
        opticalProfile.plasma_distortion = 2;
        opticalProfile.crt_beam_level = 1;
        opticalProfile.crt_persistence_ms = 20;
        opticalProfile.crt_distortion = 2;
        isAnimating = NO;

        terminal = (plato_terminal_t *)calloc(1, sizeof(plato_terminal_t));
        plato_terminal_init(terminal);
        plato_keyboard_state_init(&keyboardState);

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
    if (optical) { plato_optical_destroy(optical); optical = NULL; }
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
    if (!plasmaLayer) return;
    NSString *filter = (opticalProfile.display_mode == 1 || opticalProfile.display_mode == 3) ? kCAFilterNearest : kCAFilterLinear;
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
    if (optical) {
        plato_optical_invalidate(optical);
    }

    if (plasmaLayer) {
        CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, rgbaBuffer, PLASMA_BYTES, NULL);
        CGImageRef img = CGImageCreate(PLASMA_WIDTH, PLASMA_HEIGHT, 8, 32, PLASMA_WIDTH * 4,
                                       colorSpace, kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst,
                                       provider, NULL, false, kCGRenderingIntentDefault);
        if (!graphicsDisabled) {
            [CATransaction begin];
            [CATransaction setDisableActions:YES];
            plasmaLayer.frame = platoDisplayRect(self.bounds);
            plasmaLayer.contents = (__bridge id)img;
            [CATransaction commit];
        }
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
        plato_keyboard_state_init(&keyboardState);
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

        if (optical) {
            plato_optical_invalidate(optical);
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
            dispatch_async(dispatch_get_main_queue(), ^{
                if (self.onConnectedHandler) {
                    self.onConnectedHandler();
                }
            });
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
    if (!terminal || !optical || !running) return;

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
        [self refreshDisplay];
    }
}

- (void)onFrameTick:(NSTimer *)timer {
    if (!terminal || !optical) return;
    plato_keyboard_poll(&keyboardState, terminal, CACurrentMediaTime());
    flowTickCount++;
    size_t ringBefore = plato_ringbuf_available(&ringbuf);
    if (ringBefore > flowRingMaximum) flowRingMaximum = ringBefore;

    /* 1. Ingestione dati di rete */
    [self processIncomingData];

    flowRingEnd = plato_ringbuf_available(&ringbuf);
    if (flowRingEnd > flowRingMaximum) flowRingMaximum = flowRingEnd;

    /* 2. Se e' attiva animazione, script in corso O appena terminato, ridisegna */
    BOOL isScriptRunning = (self.scriptRunner && plato_script_is_running(self.scriptRunner));
    static BOOL s_prevScriptRunning = NO;
    BOOL shouldRefresh = isAnimating || isScriptRunning || s_prevScriptRunning;
    s_prevScriptRunning = isScriptRunning;
    if (shouldRefresh) {
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
    if (!terminal || !optical || !rgbaBuffer) return;
    double now = CACurrentMediaTime();
    isAnimating = plato_optical_render(optical, terminal, &opticalProfile, now, rgbaBuffer);

    /* Indicatore visivo Script in Esecuzione: salvataggio e ripristino fedele dello sfondo */
    static uint32_t s_saved_bg[33 * 33];
    static BOOL s_has_saved_bg = NO;

    const int cx = PLASMA_WIDTH - 48;
    const int cy = 48;
    const int r = 16;
    const int box_dim = 2 * r + 1; /* 33 */

    /* Ripristina SEMPRE lo sfondo originale pulito se era stato salvato */
    if (s_has_saved_bg) {
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                int px = cx + dx;
                int py = cy + dy;
                if (px >= 0 && px < PLASMA_WIDTH && py >= 0 && py < PLASMA_HEIGHT) {
                    rgbaBuffer[py * PLASMA_WIDTH + px] = s_saved_bg[(dy + r) * box_dim + (dx + r)];
                }
            }
        }
        s_has_saved_bg = NO;
    }

    BOOL isScriptRunning = (self.scriptRunner && plato_script_is_running(self.scriptRunner));
    if (isScriptRunning) {
        isAnimating = YES;
        /* Frequenza lampeggio: ciclo di 400ms (200ms ON / 200ms OFF) a 2.5 Hz */
        bool blink_on = ((uint64_t)(now * 1000.0) % 400) < 200;
        if (blink_on) {
            /* Salva i pixel di sfondo correnti prima di applicare il colore */
            for (int dy = -r; dy <= r; dy++) {
                for (int dx = -r; dx <= r; dx++) {
                    int px = cx + dx;
                    int py = cy + dy;
                    if (px >= 0 && px < PLASMA_WIDTH && py >= 0 && py < PLASMA_HEIGHT) {
                        s_saved_bg[(dy + r) * box_dim + (dx + r)] = rgbaBuffer[py * PLASMA_WIDTH + px];
                    }
                }
            }
            s_has_saved_bg = YES;

            /* Disegna il pallino arancione */
            uint32_t col = 0xFFFF6E00u; /* Orange Plasma */
            for (int dy = -r; dy <= r; dy++) {
                for (int dx = -r; dx <= r; dx++) {
                    if (dx * dx + dy * dy <= r * r) {
                        int px = cx + dx;
                        int py = cy + dy;
                        if (px >= 0 && px < PLASMA_WIDTH && py >= 0 && py < PLASMA_HEIGHT) {
                            rgbaBuffer[py * PLASMA_WIDTH + px] = col;
                        }
                    }
                }
            }
        }
    }

    fpsTotalFrames++;
    if (opticalProfile.display_mode != 1 && opticalProfile.display_mode != 3) {
        if (plato_optical_is_full_frame(optical)) fpsFullFrames++; else fpsPartialFrames++;
    }

    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, rgbaBuffer, PLASMA_BYTES, NULL);
    CGImageRef img = CGImageCreate(PLASMA_WIDTH, PLASMA_HEIGHT, 8, 32, PLASMA_WIDTH * 4,
                                   colorSpace, kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst,
                                   provider, NULL, false, kCGRenderingIntentDefault);
    if (!graphicsDisabled) {
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        plasmaLayer.frame = platoDisplayRect(self.bounds);
        plasmaLayer.contents = (__bridge id)img;
        [CATransaction commit];
    }
    CGImageRelease(img);
    CGDataProviderRelease(provider);
}

- (void)setDisplayNone {
    graphicsDisabled = YES;
    if (fpsTimer) {
        [fpsTimer invalidate];
        fpsTimer = nil;
    }
    if (plasmaLayer) [plasmaLayer setHidden:YES];
    if (overlayLayer) [overlayLayer setHidden:YES];
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.window) {
            [self.window orderOut:nil];
        }
    });
}

- (void)setDisplayCrisp {
    opticalProfile.display_mode = 1; /* Crisp Mono in libplato */
    if (terminal) plato_terminal_set_color_mode(terminal, false);
    if (optical) plato_optical_invalidate(optical);
    [self updateLayerFilters];
    [self refreshDisplay];
}

- (void)setDisplayCrispColor {
    opticalProfile.display_mode = 3; /* Crisp Color in libplato */
    if (terminal) plato_terminal_set_color_mode(terminal, true);
    if (optical) plato_optical_invalidate(optical);
    [self updateLayerFilters];
    [self refreshDisplay];
}

- (void)setDisplayRealColorCRT {
    opticalProfile.display_mode = 4; /* Real Color CRT in libplato */
    if (terminal) plato_terminal_set_color_mode(terminal, true);
    if (optical) plato_optical_invalidate(optical);
    [self updateLayerFilters];
    [self refreshDisplay];
}

- (void)setCRTBeamLevel:(NSInteger)level {
    if (level < 0) level = 0;
    if (level > 2) level = 2;
    opticalProfile.crt_beam_level = (int)level;
    if (optical) plato_optical_invalidate(optical);
    [self refreshDisplay];
}

- (NSInteger)crtBeamLevel {
    return opticalProfile.crt_beam_level;
}

- (void)setCRTDistortion:(NSInteger)distortion {
    if (distortion < 0) distortion = 0;
    if (distortion > 2) distortion = 2;
    opticalProfile.crt_distortion = (int)distortion;
    if (optical) plato_optical_invalidate(optical);
    [self refreshDisplay];
}

- (NSInteger)crtDistortion {
    return opticalProfile.crt_distortion;
}

- (void)setPlasmaDistortion:(NSInteger)distortion {
    if (distortion < 0) distortion = 0;
    if (distortion > 2) distortion = 2;
    opticalProfile.plasma_distortion = (int)distortion;
    if (optical) plato_optical_invalidate(optical);
    [self refreshDisplay];
}

- (NSInteger)plasmaDistortion {
    return opticalProfile.plasma_distortion;
}

- (void)setDisplayRealPlasma {
    opticalProfile.display_mode = 0; /* Real Plasma in libplato */
    if (terminal) plato_terminal_set_color_mode(terminal, false);
    if (optical) plato_optical_invalidate(optical);
    [self updateLayerFilters];
    [self refreshDisplay];
}

- (void)setDisplaySplit {
    opticalProfile.display_mode = 2; /* Split Mono in libplato */
    if (terminal) plato_terminal_set_color_mode(terminal, false);
    if (optical) plato_optical_invalidate(optical);
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
    (void)tag;
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
    if (duration < 0.02) duration = 0.02;
    opticalProfile.persistence_ms = (int)(duration * 1000.0 + 0.5);
    if (optical) plato_optical_invalidate(optical);
    [self refreshDisplay];
}

- (NSInteger)currentDisplayMode {
    if (opticalProfile.display_mode == 0) return 1; /* Real Plasma (tag 1002 Cocoa) */
    if (opticalProfile.display_mode == 1) return 0; /* Crisp Mono (tag 1001 Cocoa) */
    return opticalProfile.display_mode;             /* 2, 3, 4 coincidono */
}

- (NSTimeInterval)plasmaDecayDuration {
    return (NSTimeInterval)opticalProfile.persistence_ms / 1000.0;
}

- (void)setCRTDecayDuration:(NSTimeInterval)duration {
    if (duration < 0.02) duration = 0.02;
    opticalProfile.crt_persistence_ms = (int)(duration * 1000.0 + 0.5);
    if (optical) plato_optical_invalidate(optical);
    [self refreshDisplay];
}

- (NSTimeInterval)crtDecayDuration {
    return (NSTimeInterval)opticalProfile.crt_persistence_ms / 1000.0;
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

- (NSString *)extractTextFromCol:(int)col0 row:(int)row0 toCol:(int)col1 row:(int)row1 compact:(BOOL)compact {
    if (!terminal) return @"";
    char buf[PLATO_ROWS * (PLATO_COLS + 2) + 1];
    size_t len = plato_terminal_get_text_area(terminal, col0, row0, col1, row1, compact ? true : false, buf, sizeof(buf));
    if (len == 0) return @"";
    return [NSString stringWithUTF8String:buf];
}

- (NSString *)extractAllTextCompact:(BOOL)compact {
    return [self extractTextFromCol:0 row:0 toCol:PLATO_COLS - 1 row:PLATO_ROWS - 1 compact:compact];
}

- (void)copyTextToPasteboardCompact:(BOOL)compact {
    NSString *text = [self extractAllTextCompact:compact];
    if (text && [text length] > 0) {
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        [pb setString:text forType:NSPasteboardTypeString];
    }
}

- (BOOL)saveTextToFile:(NSString *)path fromCol:(int)col0 row:(int)row0 toCol:(int)col1 row:(int)row1 compact:(BOOL)compact error:(NSString **)error {
    NSString *text = [self extractTextFromCol:col0 row:row0 toCol:col1 row:row1 compact:compact];
    NSError *err = nil;
    BOOL ok = [text writeToFile:[path stringByExpandingTildeInPath] atomically:YES encoding:NSUTF8StringEncoding error:&err];
    if (!ok && error) {
        *error = [err localizedDescription];
    }
    return ok;
}

- (BOOL)saveAllTextToFile:(NSString *)path compact:(BOOL)compact error:(NSString **)error {
    return [self saveTextToFile:path fromCol:0 row:0 toCol:PLATO_COLS - 1 row:PLATO_ROWS - 1 compact:compact error:error];
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
        BOOL isCrisp = (opticalProfile.display_mode == 1 || opticalProfile.display_mode == 3);
        NSString *userInfo = @"";
        if (terminal && terminal->user_name[0] && terminal->user_group[0]) {
            userInfo = [NSString stringWithFormat:@"USER    %s/%s (%s)\n", terminal->user_name, terminal->user_group, terminal->user_station];
        } else if (terminal && terminal->user_station[0]) {
            userInfo = [NSString stringWithFormat:@"SLOT    %s\n", terminal->user_station];
        }

        NSString *fpsText = isCrisp
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
        if (opticalProfile.display_mode == 4) {
            activeDist = opticalProfile.crt_distortion;
        } else if (opticalProfile.display_mode == 0) {
            activeDist = opticalProfile.plasma_distortion;
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
    plato_key_event_t ev = {0};
    ev.shift = (flags & NSEventModifierFlagShift) != 0;
    ev.ctrl  = (flags & NSEventModifierFlagControl) != 0;
    ev.alt   = (flags & NSEventModifierFlagOption) != 0;
    ev.gui   = (flags & NSEventModifierFlagCommand) != 0;

    NSString *chars = [event characters];
    NSString *unmod = [[event charactersIgnoringModifiers] lowercaseString];
    if ([chars length] == 0) return;
    unichar c = [chars characterAtIndex:0];
    unichar uc = ([unmod length] > 0) ? [unmod characterAtIndex:0] : 0;

    /* 1. Durante l'esecuzione dello script, blocca qualsiasi input tranne Ctrl+Shift+X */
    if (self.scriptRunner && plato_script_is_running(self.scriptRunner)) {
        if (ev.ctrl && ev.shift && !ev.alt && (uc == 'x')) {
            plato_script_stop(self.scriptRunner);
        }
        return;
    }

    /* 2. Controllo e dispatching Hotkey Script Personalizzati */
    PLATOAppDelegate *appDelegate = (PLATOAppDelegate *)[NSApp delegate];
    if ([appDelegate respondsToSelector:@selector(scriptList)] && appDelegate.scriptList && appDelegate.scriptList->count > 0) {
        uint32_t current_mods = 0;
        if (ev.ctrl)  current_mods |= PLATO_HOTKEY_MOD_CTRL;
        if (ev.alt)   current_mods |= PLATO_HOTKEY_MOD_ALT;
        if (ev.shift) current_mods |= PLATO_HOTKEY_MOD_SHIFT;

        unichar upc = [[chars uppercaseString] characterAtIndex:0];
        uint32_t match_key = (c >= NSF1FunctionKey && c <= NSF12FunctionKey) ? (uint32_t)c : (uint32_t)upc;

        for (size_t si = 0; si < appDelegate.scriptList->count; si++) {
            const plato_script_t *sc = &appDelegate.scriptList->scripts[si];
            if (!sc->enabled || sc->hotkey_key == 0) continue;
            if (sc->hotkey_modifiers == current_mods && sc->hotkey_key == match_key) {
                PLATOTerminalWindowController *tc = (PLATOTerminalWindowController *)[self.window windowController];
                if (tc && [tc respondsToSelector:@selector(runScriptStruct:)]) {
                    [tc runScriptStruct:sc];
                }
                return; /* Tasto consumato per avviare lo script: nessun leak al terminale */
            }
        }
    }

    ev.codepoint = c;
    ev.unmod_codepoint = uc;

    if (c >= NSF1FunctionKey && c <= NSF12FunctionKey) {
        ev.vkey = (plato_virtual_key_t)(PLATO_VKEY_F1 + (c - NSF1FunctionKey));
    } else if (c == NSLeftArrowFunctionKey) {
        ev.vkey = PLATO_VKEY_LEFT;
    } else if (c == NSRightArrowFunctionKey) {
        ev.vkey = PLATO_VKEY_RIGHT;
    } else if (c == NSUpArrowFunctionKey) {
        ev.vkey = PLATO_VKEY_UP;
    } else if (c == NSDownArrowFunctionKey) {
        ev.vkey = PLATO_VKEY_DOWN;
    } else if (c == NSPageUpFunctionKey) {
        ev.vkey = PLATO_VKEY_PAGEUP;
    } else if (c == NSPageDownFunctionKey) {
        ev.vkey = PLATO_VKEY_PAGEDOWN;
    } else if (c == 13 || c == 3 || c == 10) {
        ev.vkey = PLATO_VKEY_RETURN;
    } else if (c == 127 || c == 8 || c == NSDeleteFunctionKey) {
        ev.vkey = PLATO_VKEY_BACKSPACE;
    } else if (c == 27) {
        ev.vkey = PLATO_VKEY_ESCAPE;
    } else if (c == '\t' || c == 9) {
        ev.vkey = PLATO_VKEY_TAB;
    }

    if (ev.vkey == PLATO_VKEY_F11) {
        [NSApp sendAction:@selector(toggleMainFullScreen:) to:[NSApp delegate] from:self];
        return;
    }
    if (ev.vkey == PLATO_VKEY_F12) {
        [NSApp sendAction:@selector(toggleFPSCounter:) to:[NSApp delegate] from:self];
        return;
    }

    plato_keyboard_dispatch(&keyboardState, terminal, &ev, CACurrentMediaTime());
}

- (void)keyUp:(NSEvent *)event {
    // Gestione rilasci se necessari in futuro
}

- (void)copy:(id)sender {
    [self copyTextToPasteboardCompact:NO];
}

- (void)paste:(id)sender {
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    NSString *text = [pb stringForType:NSPasteboardTypeString];
    if (text && [text length] > 0) {
        [self pasteText:text];
    }
}

@end