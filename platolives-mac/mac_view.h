#import <Cocoa/Cocoa.h>
#include <libproc.h>
#include <sys/resource.h>
#include <mach/mach.h>
#include "plato/plato_terminal.h"
#include "plato/plato_transport.h"
#include "plato/plato_ringbuf.h"
#include "plato/plato_graphics.h"

FOUNDATION_EXPORT NSString *PLATOKeyboardReferenceText(void);

typedef struct PLATOPlasmaState PLATOPlasmaState;

@interface PLATOView : NSView {
@public
    plato_terminal_t *terminal;
    BOOL graphicsDisabled;
    plato_transport_t *transport;
    plato_ringbuf_t ringbuf;
    uint32_t *rgbaBuffer;
    CGColorSpaceRef colorSpace;
    pthread_t networkThread;
    bool running;
    NSTimer *renderTimer;
    NSTimer *fpsTimer;
    BOOL keyboardReferenceVisible;
    BOOL fpsCounterVisible;
    CFTimeInterval fpsSampleStart;
    uint64_t fpsTotalFrames, fpsPartialFrames, fpsFullFrames;
    double fpsTotal, fpsPartial, fpsFull;
    uint64_t flowTickCount, flowFeedCount, flowReadBytes;
    size_t flowRingMaximum, flowRingEnd;
    CFTimeInterval flowSampleStart;
    CALayer *plasmaLayer;
    CALayer *overlayLayer;
    dispatch_queue_t pasteQueue;
    volatile BOOL pasteCancelled;
    double cpuUsagePercent;
    PLATOPlasmaState *plasma;
    uint8_t feedBuffer[4096];
    size_t feedBufferLen;
    size_t feedBufferPos;
    CFTimeInterval paceUntil;
}

- (void)setTerminal:(plato_terminal_t *)term;
- (void)connectToHost:(NSString *)host port:(int)port;
- (void)disconnect;
- (void)resetTerminal;
- (void)clearScreen;
- (void)displayConnectionFailedMessage:(NSString *)errorMsg host:(NSString *)host port:(int)port;
- (void)refreshDisplay;
- (BOOL)sendTestText:(NSString *)text error:(NSString **)error;
- (BOOL)sendTestKey:(NSString *)name error:(NSString **)error;
- (BOOL)saveRenderedScreenshot:(NSString *)path error:(NSString **)error;
- (void)setDisplayNone;
- (void)setDisplayCrisp;
- (void)setDisplayRealPlasma;
- (void)setDisplaySplit;
- (void)setDisplayCrispColor;
- (void)setDisplayRealColorCRT;
- (void)setCRTBeamLevel:(NSInteger)level;
- (NSInteger)crtBeamLevel;
- (void)setCRTDistortion:(NSInteger)distortion;
- (NSInteger)crtDistortion;
- (void)setPlasmaDistortion:(NSInteger)distortion;
- (NSInteger)plasmaDistortion;
- (void)setKeyboardReferenceVisible:(BOOL)visible;
- (void)setDiagnosticLogEnabled:(BOOL)enabled;
- (void)setPlasmaProfilePath:(NSString *)path;
- (void)writePlasmaProfileWithTag:(NSString *)tag;
- (void)setRendererPerformanceLogEnabled:(BOOL)enabled;
- (void)setFPSCounterVisible:(BOOL)visible;
- (void)setPlasmaDecayDuration:(NSTimeInterval)duration;
- (NSTimeInterval)plasmaDecayDuration;
- (void)setCRTDecayDuration:(NSTimeInterval)duration;
- (NSTimeInterval)crtDecayDuration;
- (NSInteger)currentDisplayMode;
- (void)pasteText:(NSString *)text;
- (void)cancelPaste;
- (void)copyScreenToPasteboard;
- (NSString *)extractTextFromCol:(int)col0 row:(int)row0 toCol:(int)col1 row:(int)row1 compact:(BOOL)compact;
- (NSString *)extractAllTextCompact:(BOOL)compact;
- (void)copyTextToPasteboardCompact:(BOOL)compact;
- (BOOL)saveTextToFile:(NSString *)path fromCol:(int)col0 row:(int)row0 toCol:(int)col1 row:(int)row1 compact:(BOOL)compact error:(NSString **)error;
- (BOOL)saveAllTextToFile:(NSString *)path compact:(BOOL)compact error:(NSString **)error;
@end
