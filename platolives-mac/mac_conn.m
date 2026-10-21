#import "mac_conn.h"
#import "mac_window.h"
#import <QuartzCore/QuartzCore.h>

@interface PLATOConnectingWindowController ()

@property (nonatomic, copy) NSString *profileName;
@property (nonatomic, copy) NSString *host;
@property (nonatomic, assign) int port;
@property (nonatomic, assign) plato_transport_t *transport;
@property (nonatomic, copy) mac_conn_done_block_t completionBlock;

@property (nonatomic, strong) NSTextField *titleLabel;
@property (nonatomic, strong) NSTextField *profileLabel;
@property (nonatomic, strong) NSTextField *targetLabel;
@property (nonatomic, strong) NSTextField *statusLabel;
@property (nonatomic, strong) NSButton *actionButton;
@property (nonatomic, strong) NSButton *retryButton;
@property (nonatomic, strong) NSTimer *animTimer;
@property (nonatomic, assign) int animStep;
@property (nonatomic, assign) BOOL isFailed;

@end

@implementation PLATOConnectingWindowController

+ (instancetype)sharedController {
    static PLATOConnectingWindowController *instance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        instance = [[PLATOConnectingWindowController alloc] init];
    });
    return instance;
}

- (instancetype)init {
    NSRect frame = NSMakeRect(0, 0, 480, 230);
    NSWindow *win = [[NSWindow alloc] initWithContentRect:frame
                                                styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];
    [win setTitle:@"PlatoLives - Connecting"];
    win.appearance = [NSAppearance appearanceNamed:NSAppearanceNameVibrantDark];

    self = [super initWithWindow:win];
    if (self) {
        [self setupUI];
    }
    return self;
}

- (void)setupUI {
    NSView *cv = self.window.contentView;
    cv.wantsLayer = YES;

    // Titolo retro
    _titleLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 175, 440, 26)];
    _titleLabel.bezeled = NO;
    _titleLabel.drawsBackground = NO;
    _titleLabel.editable = NO;
    _titleLabel.alignment = NSTextAlignmentCenter;
    _titleLabel.font = [NSFont boldSystemFontOfSize:17];
    [cv addSubview:_titleLabel];

    // Info Profilo
    _profileLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(36, 138, 410, 20)];
    _profileLabel.bezeled = NO;
    _profileLabel.drawsBackground = NO;
    _profileLabel.editable = NO;
    _profileLabel.font = [NSFont systemFontOfSize:14 weight:NSFontWeightMedium];
    [cv addSubview:_profileLabel];

    // Target Server
    _targetLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(36, 112, 410, 20)];
    _targetLabel.bezeled = NO;
    _targetLabel.drawsBackground = NO;
    _targetLabel.editable = NO;
    _targetLabel.font = [NSFont systemFontOfSize:14 weight:NSFontWeightRegular];
    [cv addSubview:_targetLabel];

    // Stato / Animazione / Errore
    _statusLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(36, 86, 410, 20)];
    _statusLabel.bezeled = NO;
    _statusLabel.drawsBackground = NO;
    _statusLabel.editable = NO;
    _statusLabel.font = [NSFont systemFontOfSize:13 weight:NSFontWeightRegular];
    [cv addSubview:_statusLabel];

    // Pulsante Retry (Enter)
    _retryButton = [[NSButton alloc] initWithFrame:NSMakeRect(85, 25, 145, 34)];
    _retryButton.bezelStyle = NSBezelStyleRounded;
    _retryButton.title = @"Retry (Enter)";
    _retryButton.target = self;
    _retryButton.action = @selector(onRetryClicked:);
    _retryButton.keyEquivalent = @"\r"; // Enter
    _retryButton.hidden = YES;
    [cv addSubview:_retryButton];

    // Pulsante Action (Cancel / Close)
    _actionButton = [[NSButton alloc] initWithFrame:NSMakeRect((480 - 150) / 2, 25, 150, 34)];
    _actionButton.bezelStyle = NSBezelStyleRounded;
    _actionButton.title = @"Cancel (ESC)";
    _actionButton.target = self;
    _actionButton.action = @selector(onActionClicked:);
    _actionButton.keyEquivalent = [NSString stringWithFormat:@"%C", (unichar)0x1B]; // ESC
    [cv addSubview:_actionButton];
}

- (void)applyTheme {
    PLATOThemeColors *tc = [PLATOThemeColors currentThemeColors];
    [self.window setBackgroundColor:tc.bgDark];

    _titleLabel.textColor = tc.bright;
    _profileLabel.textColor = tc.normal;
    _targetLabel.textColor = tc.muted;
    _statusLabel.textColor = tc.bright;
}

- (void)showForParentWindow:(NSWindow *)parentWindow
                profileName:(NSString *)name
                       host:(NSString *)host
                       port:(int)port
                  transport:(plato_transport_t *)t
                 completion:(mac_conn_done_block_t)completion {
    _profileName = [name copy];
    _host = [host copy];
    _port = port;
    _transport = t;
    _completionBlock = [completion copy];
    _isFailed = NO;
    _animStep = 0;

    [self applyTheme];

    if (parentWindow) {
        NSRect pr = [parentWindow frame];
        NSRect wr = [self.window frame];
        NSPoint p = NSMakePoint(pr.origin.x + (pr.size.width - wr.size.width) / 2.0,
                                pr.origin.y + (pr.size.height - wr.size.height) / 2.0);
        [self.window setFrameOrigin:p];
    } else {
        [self.window center];
    }

    [self startConnecting];
    [self.window makeKeyAndOrderFront:nil];
}

- (void)startConnecting {
    _isFailed = NO;
    _retryButton.hidden = YES;
    _actionButton.frame = NSMakeRect((480 - 150) / 2, 25, 150, 34);
    _actionButton.title = @"Cancel (ESC)";

    _titleLabel.stringValue = @"[ CONNECTING TO PLATO ]";
    _profileLabel.stringValue = [NSString stringWithFormat:@"Profile: %@", _profileName];
    _targetLabel.stringValue = [NSString stringWithFormat:@"Target:  %@:%d", _host, _port];
    _statusLabel.stringValue = @"[ . . . ]";

    PLATOThemeColors *tc = [PLATOThemeColors currentThemeColors];
    _statusLabel.textColor = tc.bright;

    [_animTimer invalidate];
    _animTimer = [NSTimer scheduledTimerWithTimeInterval:0.22
                                                  target:self
                                                selector:@selector(onAnimTimer:)
                                                userInfo:nil
                                                 repeats:YES];

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        bool ok = plato_transport_connect(self->_transport, [self->_host UTF8String], self->_port);
        dispatch_async(dispatch_get_main_queue(), ^{
            [self handleConnectResult:ok];
        });
    });
}

- (void)onAnimTimer:(NSTimer *)timer {
    (void)timer;
    if (_isFailed) return;
    _animStep = (_animStep + 1) % 4;
    NSArray *dots = @[@"[ . . . ]", @"[ * . . ]", @"[ * * . ]", @"[ * * * ]"];
    _statusLabel.stringValue = dots[_animStep];
}

- (void)handleConnectResult:(BOOL)success {
    [_animTimer invalidate];
    _animTimer = nil;

    if (success) {
        [self closeConnectingWindow];
        if (_completionBlock) _completionBlock(YES);
    } else {
        _isFailed = YES;
        PLATOThemeColors *tc = [PLATOThemeColors currentThemeColors];

        _titleLabel.stringValue = @"[ CONNECTION FAILED ]";
        const char *err = (_transport && _transport->last_error[0]) ? _transport->last_error : "Connection refused";
        _statusLabel.stringValue = [NSString stringWithFormat:@"Error:   %s", err];
        _statusLabel.textColor = tc.normal;

        _retryButton.hidden = NO;
        _retryButton.frame = NSMakeRect(85, 25, 145, 34);

        _actionButton.frame = NSMakeRect(250, 25, 145, 34);
        _actionButton.title = @"Close (ESC)";
    }
}

- (void)onRetryClicked:(id)sender {
    (void)sender;
    [self startConnecting];
}

- (void)onActionClicked:(id)sender {
    (void)sender;
    if (!_isFailed && _transport) {
        plato_transport_cancel_connect(_transport);
    }
    mac_conn_done_block_t cb = _completionBlock;
    [self closeConnectingWindow];
    if (cb) cb(NO);
}

- (void)closeConnectingWindow {
    [_animTimer invalidate];
    _animTimer = nil;
    [self.window orderOut:nil];
}

@end
