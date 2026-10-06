#include "plato/plato_script.h"
#import "mac_window.h"
#import "test_runner.h"
#import "mac_scripts.h"

static void plato_mac_metadata_callback(void *context, const char *name, const char *group, const char *system, const char *station) {
    (void)name; (void)group; (void)system; (void)station;
    PLATOTerminalWindowController *controller = (__bridge PLATOTerminalWindowController *)context;
    if (!controller) return;
    dispatch_async(dispatch_get_main_queue(), ^{
        [controller updateTitleWithMetadata];
    });
}

static void plato_mac_beep(void *context) {
    (void)context;
    NSBeep();
    if (![NSApp isActive]) [NSApp requestUserAttention:NSInformationalRequest];
}

@interface PLATOTerminalWindowController () {
@public
    plato_script_runner_t *_scriptRunner;
}
@end

@implementation PLATOTerminalWindowController

- (instancetype)initWithProfile:(NSDictionary *)profile {
    return [self initWithProfile:profile tabbedWithWindow:nil];
}

- (instancetype)initWithProfile:(NSDictionary *)profile tabbedWithWindow:(NSWindow *)hostWindow {
    static NSPoint cascadePoint = { 40.0, 100.0 };
    NSRect frame = NSMakeRect(cascadePoint.x, cascadePoint.y, 960, 960);
    NSWindow *win = [[NSWindow alloc] initWithContentRect:frame
                                                styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                           NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable)
                                                  backing:NSBackingStoreBuffered defer:NO];
    [win setColorSpace:[NSColorSpace sRGBColorSpace]];

    self = [super initWithWindow:win];
    if (self) {
        [win setDelegate:self];
        _view = [[PLATOView alloc] initWithFrame:frame];
        if (_view->terminal) {
            _view->terminal->beep_callback = plato_mac_beep;
            _view->terminal->metadata_callback = plato_mac_metadata_callback;
            _view->terminal->metadata_context = (__bridge void *)self;
        }
        [win setContentView:_view];

        cascadePoint.x += 28.0;
        cascadePoint.y += 28.0;
        if (cascadePoint.x > 300.0) cascadePoint.x = 40.0;
        if (cascadePoint.y > 300.0) cascadePoint.y = 100.0;

        if (hostWindow) {
            [hostWindow addTabbedWindow:win ordered:NSWindowAbove];
            [self applyProfile:profile syncFullScreen:NO];
        } else {
            [self applyProfile:profile syncFullScreen:YES];
        }
    }
    return self;
}

- (void)updateTitleWithMetadata {
    if (!_view || !_view->terminal) return;
    plato_terminal_t *t = _view->terminal;
    NSString *name = _currentProfile[@"name"] ? _currentProfile[@"name"] : @"PLATO";
    NSString *host = _currentProfile[@"host"] ? _currentProfile[@"host"] : @"cyberserv.org";
    int port = [_currentProfile[@"port"] intValue] ? [_currentProfile[@"port"] intValue] : 8005;

    if (t->user_name[0] && t->user_group[0]) {
        NSString *user = [NSString stringWithUTF8String:t->user_name];
        NSString *grp  = [NSString stringWithUTF8String:t->user_group];
        NSString *stn  = (t->user_station[0]) ? [NSString stringWithFormat:@" (%s)", t->user_station] : @"";
        [self.window setTitle:[NSString stringWithFormat:@"PlatoLives: %@/%@%@ - %@ (%@:%d)", user, grp, stn, name, host, port]];
    } else if (t->user_station[0]) {
        NSString *stn = [NSString stringWithUTF8String:t->user_station];
        [self.window setTitle:[NSString stringWithFormat:@"PlatoLives: slot %@ - %@ (%@:%d)", stn, name, host, port]];
    }
}

- (void)applyProfile:(NSDictionary *)p {
    [self applyProfile:p syncFullScreen:YES];
}

- (void)applyProfile:(NSDictionary *)p syncFullScreen:(BOOL)syncFullScreen {
    if (!p) return;
    _currentProfile = [p copy];

    [self.view resetTerminal];

    NSInteger mode = [p[@"displayMode"] integerValue];
    if (mode == 0) [self.view setDisplayCrisp];
    else if (mode == 2) [self.view setDisplaySplit];
    else if (mode == 3) [self.view setDisplayCrispColor];
    else if (mode == 4) [self.view setDisplayRealColorCRT];
    else [self.view setDisplayRealPlasma];

    NSInteger ms = [p[@"persistenceMs"] integerValue];
    if (ms <= 0) ms = 100;
    [self.view setPlasmaDecayDuration:(NSTimeInterval)ms / 1000.0];

    NSInteger pDist = p[@"plasmaDistortion"] ? [p[@"plasmaDistortion"] integerValue] : 2;
    [self.view setPlasmaDistortion:pDist];

    NSInteger crtMs = [p[@"crtPersistenceMs"] integerValue];
    if (crtMs <= 0) crtMs = 20;
    [self.view setCRTDecayDuration:(NSTimeInterval)crtMs / 1000.0];

    NSInteger dist = p[@"crtDistortion"] ? [p[@"crtDistortion"] integerValue] : 2;
    [self.view setCRTDistortion:dist];

    NSString *host = p[@"host"] ? p[@"host"] : @"cyberserv.org";
    int pVal = [p[@"port"] intValue];
    int port = (pVal > 0 ? pVal : 8005);
    __weak PLATOTerminalWindowController *weakSelf = self;
    self.view.onConnectedHandler = ^{
        [weakSelf startStartupScriptIfNeeded];
    };
    [self.view connectToHost:host port:port];

    NSString *name = p[@"name"] ? p[@"name"] : @"PLATO";
    [self.window setTitle:[NSString stringWithFormat:@"PlatoLives - %@ (%@:%d)", name, host, port]];
    [self.window makeKeyAndOrderFront:nil];

    if (!syncFullScreen) return;
    BOOL wantFull = [p[@"fullScreen"] boolValue];
    BOOL isFull = ((self.window.styleMask & NSWindowStyleMaskFullScreen) != 0);
    if (wantFull != isFull) {
        dispatch_async(dispatch_get_main_queue(), ^{ [self.window toggleFullScreen:nil]; });
    }
}

- (void)runScriptText:(NSString *)scriptText {
    if (!scriptText || [scriptText length] == 0 || !self.view || !self.view->terminal) return;
    if (!_scriptRunner) {
        _scriptRunner = plato_script_create(self.view->terminal);
        self.view.scriptRunner = _scriptRunner;
    }
    self.view.scriptRunner = _scriptRunner;
    plato_script_start(_scriptRunner, [scriptText UTF8String]);
}

- (void)runScriptStruct:(const plato_script_t *)script {
    if (!script || !self.view || !self.view->terminal) return;
    if (!_scriptRunner) {
        _scriptRunner = plato_script_create(self.view->terminal);
        self.view.scriptRunner = _scriptRunner;
    }
    self.view.scriptRunner = _scriptRunner;
    plato_script_start_ex(_scriptRunner, script);
}

- (void)startStartupScriptIfNeeded {
    if (!self.currentProfile) return;
    BOOL enabled = [self.currentProfile[@"startupScriptEnabled"] boolValue];
    NSString *script = self.currentProfile[@"startupScript"];
    if (enabled && script && [script length] > 0) {
        [self runScriptText:script];
    }
}

- (void)windowWillClose:(NSNotification *)notification {
    if (_scriptRunner) {
        self.view.scriptRunner = NULL;
        plato_script_destroy(_scriptRunner);
        _scriptRunner = NULL;
    }
    [self.view disconnect];
    if ([self.appDelegate respondsToSelector:@selector(terminalWindowControllerWillClose:)]) {
        [self.appDelegate terminalWindowControllerWillClose:self];
    }
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
    PLATOAppDelegate *del = (PLATOAppDelegate *)[NSApp delegate];
    if ([del respondsToSelector:@selector(activeWindowDidChange:)]) {
        [del activeWindowDidChange:self];
    }
}

@end

@interface PLATOKeymapWindow : NSWindow
@end

@implementation PLATOKeymapWindow
- (void)toggleFullScreen:(id)sender {
    PLATOAppDelegate *delegate = (PLATOAppDelegate *)[NSApp delegate];
    if (delegate.window && delegate.window != self) [delegate.window toggleFullScreen:sender];
}
@end

@interface PLATOKeymapWindowController : NSWindowController <NSWindowDelegate>
@property (nonatomic, copy) void (^closeHandler)(void);
@end


@interface PLATOScriptRefWindow : NSWindow
@end

@implementation PLATOScriptRefWindow
- (BOOL)canBecomeKeyWindow { return YES; }
- (BOOL)canBecomeMainWindow { return YES; }
@end

@interface PLATOScriptRefWindowController : NSWindowController <NSWindowDelegate>
@end

@implementation PLATOScriptRefWindowController
- (instancetype)init {
    NSRect frame = NSMakeRect(0, 0, 840, 780);
    NSWindow *window = [[PLATOScriptRefWindow alloc] initWithContentRect:frame
                                                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                             NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable)
                                                    backing:NSBackingStoreBuffered defer:NO];
    self = [super initWithWindow:window];
    if (self) {
        [window setTitle:@"PLATO Scripting Engine Reference"];
        [window setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]];
        [window setBackgroundColor:[NSColor colorWithCalibratedRed:16.0/255.0 green:12.0/255.0 blue:10.0/255.0 alpha:1.0]];
        [window setDelegate:self];
        [window center];

        NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:[[window contentView] bounds]];
        [scroll setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [scroll setHasVerticalScroller:YES];
        [scroll setHasHorizontalScroller:NO];
        [scroll setBorderType:NSNoBorder];
        [scroll setDrawsBackground:YES];
        [scroll setBackgroundColor:[NSColor colorWithCalibratedRed:16.0/255.0 green:12.0/255.0 blue:10.0/255.0 alpha:1.0]];

        NSTextView *textView = [[NSTextView alloc] initWithFrame:[[scroll contentView] bounds]];
        [textView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [textView setEditable:NO];
        [textView setSelectable:YES];
        [textView setDrawsBackground:YES];
        [textView setBackgroundColor:[NSColor colorWithCalibratedRed:16.0/255.0 green:12.0/255.0 blue:10.0/255.0 alpha:1.0]];
        [textView setTextContainerInset:NSMakeSize(20.0, 20.0)];

        const char *manual_c = plato_script_get_manual_text();
        NSString *manualText = [NSString stringWithUTF8String:manual_c ? manual_c : ""];
        NSFont *font = [NSFont fontWithName:@"Menlo" size:12.5];
        if (!font) {
            font = [NSFont monospacedSystemFontOfSize:12.5 weight:NSFontWeightRegular];
        }
        NSDictionary *attrs = @{
            NSFontAttributeName: font,
            NSForegroundColorAttributeName: [NSColor colorWithCalibratedRed:255.0/255.0 green:110.0/255.0 blue:0.0/255.0 alpha:1.0]
        };
        [[textView textStorage] setAttributedString:[[NSAttributedString alloc] initWithString:manualText attributes:attrs]];
        [scroll setDocumentView:textView];
        [[window contentView] addSubview:scroll];
    }
    return self;
}
@end

@implementation PLATOKeymapWindowController
- (instancetype)init {
    NSRect frame = NSMakeRect(0, 0, 620, 880);
    NSWindow *window = [[PLATOKeymapWindow alloc] initWithContentRect:frame
                                                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                             NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable)
                                                    backing:NSBackingStoreBuffered defer:NO];
    self = [super initWithWindow:window];
    if (self) {
        [window setTitle:@"PLATO Keyboard Reference"];
        [window setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]];
        [window setBackgroundColor:[NSColor colorWithCalibratedRed:16.0/255.0 green:12.0/255.0 blue:10.0/255.0 alpha:1.0]];
        [window setDelegate:self];
        [window center];
        NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:[[window contentView] bounds]];
        [scroll setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [scroll setHasVerticalScroller:NO];
        [scroll setHasHorizontalScroller:NO];
        [scroll setBorderType:NSNoBorder];
        [scroll setDrawsBackground:YES];
        [scroll setBackgroundColor:[NSColor colorWithCalibratedRed:16.0/255.0 green:12.0/255.0 blue:10.0/255.0 alpha:1.0]];

        NSTextView *textView = [[NSTextView alloc] initWithFrame:[[window contentView] bounds]];
        [textView setEditable:NO];
        [textView setSelectable:YES];
        [textView setDrawsBackground:YES];
        [textView setBackgroundColor:[NSColor colorWithCalibratedRed:16.0/255.0 green:12.0/255.0 blue:10.0/255.0 alpha:1.0]];
        [textView setTextColor:[NSColor colorWithCalibratedRed:255.0/255.0 green:110.0/255.0 blue:0.0/255.0 alpha:1.0]];
        [textView setFont:[NSFont monospacedSystemFontOfSize:14.0 weight:NSFontWeightRegular]];
        [textView setTextContainerInset:NSMakeSize(18.0, 18.0)];
        [textView setContinuousSpellCheckingEnabled:NO];
        [textView setGrammarCheckingEnabled:NO];
        [textView setAutomaticSpellingCorrectionEnabled:NO];
        [textView setAutomaticTextReplacementEnabled:NO];
        [textView setAutomaticQuoteSubstitutionEnabled:NO];
        [[textView textStorage] setAttributedString:[[NSAttributedString alloc] initWithString:PLATOKeyboardReferenceText()
                                                                                   attributes:@{
            NSFontAttributeName: [NSFont monospacedSystemFontOfSize:14.0 weight:NSFontWeightRegular],
            NSForegroundColorAttributeName: [NSColor colorWithCalibratedRed:255.0/255.0 green:110.0/255.0 blue:0.0/255.0 alpha:1.0]
        }]];
        [scroll setDocumentView:textView];
        [window setContentView:scroll];
    }
    return self;
}
- (void)windowWillClose:(NSNotification *)notification { if (self.closeHandler) self.closeHandler(); }
@end

#define PLATO_PROFILES_KEY @"PlatoLivesProfiles"

@interface PLATOProfileManager : NSObject
+ (NSMutableArray<NSMutableDictionary *> *)loadProfiles;
+ (void)saveProfiles:(NSArray<NSDictionary *> *)profiles;
+ (NSMutableDictionary *)defaultProfile;
+ (void)setDefaultProfileId:(NSString *)profileId;
@end

@implementation PLATOProfileManager
+ (NSMutableArray<NSMutableDictionary *> *)loadProfiles {
    NSArray *saved = [[NSUserDefaults standardUserDefaults] objectForKey:PLATO_PROFILES_KEY];
    if (!saved || [saved count] == 0) {
        NSMutableDictionary *cyber1 = [@{
            @"id": [[NSUUID UUID] UUIDString],
            @"name": @"Cyber1",
            @"host": @"cyberserv.org",
            @"port": @8005,
            @"displayMode": @1,
            @"persistenceMs": @100,
            @"plasmaDistortion": @2,
            @"crtBeamLevel": @1,
            @"crtPersistenceMs": @20,
            @"crtDistortion": @2,
            @"fullScreen": @YES,
            @"isDefault": @YES
        } mutableCopy];
        [self saveProfiles:@[cyber1]];
        return [NSMutableArray arrayWithObject:cyber1];
    }
    NSMutableArray<NSMutableDictionary *> *result = [NSMutableArray array];
    for (NSDictionary *dict in saved) {
        [result addObject:[dict mutableCopy]];
    }
    return result;
}

+ (void)saveProfiles:(NSArray<NSDictionary *> *)profiles {
    [[NSUserDefaults standardUserDefaults] setObject:profiles forKey:PLATO_PROFILES_KEY];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

+ (NSMutableDictionary *)defaultProfile {
    NSMutableArray<NSMutableDictionary *> *profiles = [self loadProfiles];
    for (NSMutableDictionary *p in profiles) {
        if ([p[@"isDefault"] boolValue]) return p;
    }
    return [profiles firstObject];
}

+ (void)setDefaultProfileId:(NSString *)profileId {
    NSMutableArray<NSMutableDictionary *> *profiles = [self loadProfiles];
    for (NSMutableDictionary *p in profiles) {
        p[@"isDefault"] = @([p[@"id"] isEqualToString:profileId]);
    }
    [self saveProfiles:profiles];
}
@end

@class PLATOProfilesWindowController;

@protocol PLATOProfilesDelegate <NSObject>
- (void)profilesController:(PLATOProfilesWindowController *)controller didSelectConnectProfile:(NSDictionary *)profile;
- (void)profilesControllerDidUpdateProfiles:(PLATOProfilesWindowController *)controller;
@end

@interface PLATOProfilesWindowController : NSWindowController <NSTableViewDataSource, NSTableViewDelegate>
@property (nonatomic, weak) id<PLATOProfilesDelegate> delegate;
@property (nonatomic, strong) NSMutableArray<NSMutableDictionary *> *profiles;
@property (nonatomic, strong) NSTableView *tableView;
@property (nonatomic, strong) NSTextField *nameField;
@property (nonatomic, strong) NSTextField *hostField;
@property (nonatomic, strong) NSTextField *portField;
@property (nonatomic, strong) NSPopUpButton *modePopup;

// Controlli Real Plasma
@property (nonatomic, strong) NSTextField *decayLbl;
@property (nonatomic, strong) NSPopUpButton *decayPopup;
@property (nonatomic, strong) NSTextField *plasmaDistortionLbl;
@property (nonatomic, strong) NSPopUpButton *plasmaDistortionPopup;

// Controlli Real Color CRT
@property (nonatomic, strong) NSTextField *crtBeamLbl;
@property (nonatomic, strong) NSPopUpButton *crtBeamPopup;
@property (nonatomic, strong) NSTextField *crtDecayLbl;
@property (nonatomic, strong) NSPopUpButton *crtDecayPopup;
@property (nonatomic, strong) NSTextField *crtDistortionLbl;
@property (nonatomic, strong) NSPopUpButton *crtDistortionPopup;

@property (nonatomic, strong) NSButton *fullscreenCheckbox;
@property (nonatomic, strong) NSButton *defaultCheckbox;
@property (nonatomic, strong) NSButton *scriptCheckbox;
@property (nonatomic, strong) NSTextView *scriptTextView;
@property (nonatomic) NSInteger currentEditingIndex;
@end

@implementation PLATOProfilesWindowController

- (instancetype)init {
    NSRect frame = NSMakeRect(0, 0, 1080, 960);
    NSWindow *win = [[NSWindow alloc] initWithContentRect:frame
                                                styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                                                  backing:NSBackingStoreBuffered defer:NO];
    [win setTitle:@"Connection Profiles"];
    [win setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]];
    [win setBackgroundColor:[NSColor colorWithCalibratedRed:16.0/255.0 green:12.0/255.0 blue:10.0/255.0 alpha:1.0]];
    [win center];

    self = [super initWithWindow:win];
    if (self) {
        _profiles = [PLATOProfileManager loadProfiles];
        [self setupUI];
    }
    return self;
}

- (void)setupUI {
    NSView *content = [self.window contentView];

    NSColor *plasmaOrange = [NSColor colorWithCalibratedRed:255.0/255.0 green:110.0/255.0 blue:0.0/255.0 alpha:1.0];
    NSColor *boxBg = [NSColor colorWithCalibratedRed:24.0/255.0 green:18.0/255.0 blue:14.0/255.0 alpha:1.0];

    NSScrollView *tableScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 55, 200, 880)];
    [tableScroll setHasVerticalScroller:YES];
    [tableScroll setBorderType:NSBezelBorder];
    [tableScroll setDrawsBackground:YES];
    [tableScroll setBackgroundColor:boxBg];

    _tableView = [[NSTableView alloc] initWithFrame:[tableScroll bounds]];
    NSTableColumn *col = [[NSTableColumn alloc] initWithIdentifier:@"name"];
    [col setTitle:@"Profiles"];
    [col setWidth:180];
    [_tableView addTableColumn:col];
    [_tableView setHeaderView:nil];
    [_tableView setDataSource:self];
    [_tableView setDelegate:self];
    [_tableView setBackgroundColor:boxBg];
    [tableScroll setDocumentView:_tableView];
    [content addSubview:tableScroll];

    NSSegmentedControl *seg = [NSSegmentedControl segmentedControlWithLabels:@[@"+", @"-"]
                                                               trackingMode:NSSegmentSwitchTrackingMomentary
                                                                     target:self
                                                                     action:@selector(onAddRemove:)];
    [seg setFrame:NSMakeRect(20, 20, 60, 24)];
    [content addSubview:seg];

    CGFloat rx = 240, rw = 815;
    CGFloat fx = rx + 120, fw = 695;

    NSTextField *titleLbl = [NSTextField labelWithString:@"Profile Settings"];
    [titleLbl setFont:[NSFont boldSystemFontOfSize:14]];
    [titleLbl setTextColor:plasmaOrange];
    [titleLbl setFrame:NSMakeRect(rx, 915, rw, 22)];
    [content addSubview:titleLbl];

    NSTextField *nameLbl = [NSTextField labelWithString:@"Profile Name:"];
    [nameLbl setFrame:NSMakeRect(rx, 884, 105, 18)];
    [nameLbl setTextColor:plasmaOrange];
    [content addSubview:nameLbl];
    _nameField = [[NSTextField alloc] initWithFrame:NSMakeRect(fx, 882, fw, 22)];
    [content addSubview:_nameField];

    NSTextField *hostLbl = [NSTextField labelWithString:@"Server Host:"];
    [hostLbl setFrame:NSMakeRect(rx, 851, 105, 18)];
    [hostLbl setTextColor:plasmaOrange];
    [content addSubview:hostLbl];
    _hostField = [[NSTextField alloc] initWithFrame:NSMakeRect(fx, 849, fw, 22)];
    [content addSubview:_hostField];

    NSTextField *portLbl = [NSTextField labelWithString:@"Port:"];
    [portLbl setFrame:NSMakeRect(rx, 818, 105, 18)];
    [portLbl setTextColor:plasmaOrange];
    [content addSubview:portLbl];
    _portField = [[NSTextField alloc] initWithFrame:NSMakeRect(fx, 816, 100, 22)];
    [content addSubview:_portField];

    NSTextField *modeLbl = [NSTextField labelWithString:@"Display Mode:"];
    [modeLbl setFrame:NSMakeRect(rx, 780, 105, 18)];
    [modeLbl setTextColor:plasmaOrange];
    [content addSubview:modeLbl];
    _modePopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(fx, 777, fw, 25) pullsDown:NO];
    [_modePopup addItemsWithTitles:@[@"Real Plasma", @"Crisp Monochrome", @"Split Monochrome", @"Crisp Color", @"Real Color CRT"]];
    [_modePopup setTarget:self];
    [_modePopup setAction:@selector(onModeChanged:)];
    [content addSubview:_modePopup];

    // --- Controlli Real Plasma ---
    _decayLbl = [NSTextField labelWithString:@"Persistence:"];
    [_decayLbl setFrame:NSMakeRect(rx, 742, 105, 18)];
    [_decayLbl setTextColor:plasmaOrange];
    [content addSubview:_decayLbl];
    _decayPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(fx, 739, fw, 25) pullsDown:NO];
    [_decayPopup addItemsWithTitles:@[@"100 ms (Authentic Plasma)", @"200 ms (Warm Glow)", @"500 ms (Medium)", @"1000 ms (Long/Radar)", @"2000 ms (Ultra Persistence)", @"5000 ms (5s Extreme)"]];
    [content addSubview:_decayPopup];

    _plasmaDistortionLbl = [NSTextField labelWithString:@"Distortion:"];
    [_plasmaDistortionLbl setFrame:NSMakeRect(rx, 704, 105, 18)];
    [_plasmaDistortionLbl setTextColor:plasmaOrange];
    [content addSubview:_plasmaDistortionLbl];
    _plasmaDistortionPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(fx, 701, fw, 25) pullsDown:NO];
    [_plasmaDistortionPopup addItemsWithTitles:@[@"None (Flat)", @"Barrel (Curved Glass)", @"Cylindrical"]];
    [content addSubview:_plasmaDistortionPopup];

    // --- Controlli Real Color CRT ---
    _crtBeamLbl = [NSTextField labelWithString:@"CRT Beam:"];
    [_crtBeamLbl setFrame:NSMakeRect(rx, 742, 105, 18)];
    [_crtBeamLbl setTextColor:plasmaOrange];
    [content addSubview:_crtBeamLbl];
    _crtBeamPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(fx, 739, fw, 25) pullsDown:NO];
    [_crtBeamPopup addItemsWithTitles:@[@"Standard (Authentic 13\")", @"High (Soft Glow)", @"Ultra (Vintage Arcade)"]];
    [content addSubview:_crtBeamPopup];

    _crtDecayLbl = [NSTextField labelWithString:@"CRT Persistence:"];
    [_crtDecayLbl setFrame:NSMakeRect(rx, 704, 105, 18)];
    [_crtDecayLbl setTextColor:plasmaOrange];
    [content addSubview:_crtDecayLbl];
    _crtDecayPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(fx, 701, fw, 25) pullsDown:NO];
    [_crtDecayPopup addItemsWithTitles:@[@"20 ms (Default CRT)", @"200 ms (Warm Glow)", @"500 ms (Medium)", @"1000 ms (Long/Radar)", @"2000 ms (Ultra Persistence)", @"5000 ms (5s Extreme)"]];
    [content addSubview:_crtDecayPopup];

    _crtDistortionLbl = [NSTextField labelWithString:@"CRT Distortion:"];
    [_crtDistortionLbl setFrame:NSMakeRect(rx, 666, 105, 18)];
    [_crtDistortionLbl setTextColor:plasmaOrange];
    [content addSubview:_crtDistortionLbl];
    _crtDistortionPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(fx, 663, fw, 25) pullsDown:NO];
    [_crtDistortionPopup addItemsWithTitles:@[@"None (Flat)", @"Barrel (Curved Glass)", @"Cylindrical"]];
    [content addSubview:_crtDistortionPopup];

    _fullscreenCheckbox = [NSButton checkboxWithTitle:@"Launch in Full Screen" target:nil action:nil];
    [_fullscreenCheckbox setFrame:NSMakeRect(fx, 632, 240, 18)];
    [content addSubview:_fullscreenCheckbox];

    _defaultCheckbox = [NSButton checkboxWithTitle:@"Default profile at startup" target:nil action:nil];
    [_defaultCheckbox setFrame:NSMakeRect(fx, 608, 240, 18)];
    [content addSubview:_defaultCheckbox];

    _scriptCheckbox = [NSButton checkboxWithTitle:@"Run Startup Script on connect" target:nil action:nil];
    [_scriptCheckbox setFrame:NSMakeRect(fx, 580, 260, 18)];
    [content addSubview:_scriptCheckbox];

    NSScrollView *scriptScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(fx, 65, fw, 505)];
    [scriptScroll setHasVerticalScroller:YES];
    [scriptScroll setBorderType:NSBezelBorder];
    _scriptTextView = [[NSTextView alloc] initWithFrame:[scriptScroll bounds]];
    [_scriptTextView setFont:[NSFont monospacedSystemFontOfSize:11 weight:NSFontWeightRegular]];
    [_scriptTextView setBackgroundColor:boxBg];
    [_scriptTextView setTextColor:[NSColor colorWithCalibratedRed:255.0/255.0 green:180.0/255.0 blue:60.0/255.0 alpha:1.0]];
    [_scriptTextView setInsertionPointColor:plasmaOrange];
    [_scriptTextView setEditable:YES];
    [_scriptTextView setSelectable:YES];
    [_scriptTextView setRichText:NO];
    [_scriptTextView setAllowsUndo:YES];
    [_scriptTextView setAutomaticQuoteSubstitutionEnabled:NO];
    [_scriptTextView setAutomaticDashSubstitutionEnabled:NO];
    [_scriptTextView setAutomaticSpellingCorrectionEnabled:NO];
    [scriptScroll setDocumentView:_scriptTextView];
    [content addSubview:scriptScroll];

    NSButton *saveBtn = [[NSButton alloc] initWithFrame:NSMakeRect(rx + rw - 260, 18, 115, 30)];
    [saveBtn setBezelStyle:NSBezelStyleRounded];
    [saveBtn setTitle:@"Save"];
    [saveBtn setTarget:self];
    [saveBtn setAction:@selector(onSave:)];
    [content addSubview:saveBtn];

    NSButton *connBtn = [[NSButton alloc] initWithFrame:NSMakeRect(rx + rw - 135, 18, 135, 30)];
    [connBtn setBezelStyle:NSBezelStyleRounded];
    [connBtn setTitle:@"Connect Now"];
    [connBtn setKeyEquivalent:[NSString stringWithFormat:@"%c", 13]];
    [connBtn setTarget:self];
    [connBtn setAction:@selector(onConnectNow:)];
    [content addSubview:connBtn];

    [_tableView reloadData];
    if ([_profiles count] > 0) {
        self.currentEditingIndex = 0;
        [_tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
        [self loadProfileToForm:_profiles[0]];
    } else {
        self.currentEditingIndex = -1;
    }
}

- (void)updateVisibilityForModeIndex:(NSInteger)mIdx {
    // mIdx: 0: Real Plasma, 1: Crisp Mono, 2: Split Mono, 3: Crisp Color, 4: Real Color CRT
    BOOL isPlasmaFull = (mIdx == 0); // Real Plasma a tutto schermo
    BOOL isPlasmaAny  = (mIdx == 0 || mIdx == 2);
    BOOL isCRT        = (mIdx == 4);

    [_decayLbl setHidden:!isPlasmaAny];
    [_decayPopup setHidden:!isPlasmaAny];
    [_plasmaDistortionLbl setHidden:!isPlasmaFull];
    [_plasmaDistortionPopup setHidden:!isPlasmaFull];

    [_crtBeamLbl setHidden:!isCRT];
    [_crtBeamPopup setHidden:!isCRT];
    [_crtDecayLbl setHidden:!isCRT];
    [_crtDecayPopup setHidden:!isCRT];
    [_crtDistortionLbl setHidden:!isCRT];
    [_crtDistortionPopup setHidden:!isCRT];
}

- (void)onModeChanged:(id)sender {
    [self updateVisibilityForModeIndex:[_modePopup indexOfSelectedItem]];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    return [self.profiles count];
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row {
    NSTextField *label = [NSTextField labelWithString:@""];
    [label setFont:[NSFont systemFontOfSize:12]];
    [label setTextColor:[NSColor colorWithCalibratedRed:255.0/255.0 green:160.0/255.0 blue:40.0/255.0 alpha:1.0]];
    NSDictionary *p = self.profiles[row];
    NSString *title = p[@"name"] ? p[@"name"] : @"Untitled";
    if ([p[@"isDefault"] boolValue]) {
        title = [title stringByAppendingString:@" ★"];
    }
    [label setStringValue:title];
    return label;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification {
    NSInteger sel = [self.tableView selectedRow];
    if (sel >= 0 && sel < (NSInteger)[self.profiles count]) {
        self.currentEditingIndex = sel;
        [self loadProfileToForm:self.profiles[sel]];
    }
}

- (void)loadProfileToForm:(NSDictionary *)p {
    [self.nameField setStringValue:(p[@"name"] ? p[@"name"] : @"")];
    [self.hostField setStringValue:(p[@"host"] ? p[@"host"] : @"")];
    [self.portField setStringValue:[NSString stringWithFormat:@"%@", (p[@"port"] ? p[@"port"] : @8005)]];

    NSInteger mode = [p[@"displayMode"] integerValue];
    NSInteger pIdx = 0;
    if (mode == 0) pIdx = 1;      // Crisp Mono
    else if (mode == 2) pIdx = 2; // Split Mono
    else if (mode == 3) pIdx = 3; // Crisp Color
    else if (mode == 4) pIdx = 4; // Real Color CRT
    else pIdx = 0;                // Real Plasma
    [self.modePopup selectItemAtIndex:pIdx];

    // Plasma persistence
    NSInteger ms = [p[@"persistenceMs"] integerValue];
    NSInteger decayIdx = 0;
    if (ms == 200) decayIdx = 1;
    else if (ms == 500) decayIdx = 2;
    else if (ms == 1000) decayIdx = 3;
    else if (ms == 2000) decayIdx = 4;
    else if (ms == 5000) decayIdx = 5;
    [self.decayPopup selectItemAtIndex:decayIdx];

    NSInteger pDist = p[@"plasmaDistortion"] ? [p[@"plasmaDistortion"] integerValue] : 2; // Default Cylindrical
    [self.plasmaDistortionPopup selectItemAtIndex:pDist];

    // CRT settings
    NSInteger beamLevel = p[@"crtBeamLevel"] ? [p[@"crtBeamLevel"] integerValue] : 1; // Default High
    [self.crtBeamPopup selectItemAtIndex:beamLevel];

    NSInteger crtMs = [p[@"crtPersistenceMs"] integerValue];
    NSInteger crtDecayIdx = 0;
    if (crtMs == 200) crtDecayIdx = 1;
    else if (crtMs == 500) crtDecayIdx = 2;
    else if (crtMs == 1000) crtDecayIdx = 3;
    else if (crtMs == 2000) crtDecayIdx = 4;
    else if (crtMs == 5000) crtDecayIdx = 5;
    [self.crtDecayPopup selectItemAtIndex:crtDecayIdx];

    NSInteger dist = p[@"crtDistortion"] ? [p[@"crtDistortion"] integerValue] : 2;
    [self.crtDistortionPopup selectItemAtIndex:dist];

    [self updateVisibilityForModeIndex:pIdx];

    [self.fullscreenCheckbox setState:[p[@"fullScreen"] boolValue] ? NSControlStateValueOn : NSControlStateValueOff];
    [self.defaultCheckbox setState:[p[@"isDefault"] boolValue] ? NSControlStateValueOn : NSControlStateValueOff];
    BOOL scriptOn = [p[@"startupScriptEnabled"] boolValue];
    [self.scriptCheckbox setState:scriptOn ? NSControlStateValueOn : NSControlStateValueOff];
    NSString *scriptText = p[@"startupScript"];
    if (!scriptText || [scriptText length] == 0) {
        scriptText = [NSString stringWithUTF8String:PLATO_DEFAULT_AUTOLOGIN_SCRIPT];
    }
    [self.scriptTextView setString:scriptText];
}

- (void)saveCurrentFormToProfile {
    NSInteger sel = self.currentEditingIndex;
    if (sel < 0 || sel >= (NSInteger)[self.profiles count]) return;
    NSMutableDictionary *p = self.profiles[sel];
    p[@"name"] = [self.nameField stringValue];
    p[@"host"] = [self.hostField stringValue];
    p[@"port"] = @([self.portField intValue]);

    NSInteger mIdx = [self.modePopup indexOfSelectedItem];
    NSInteger modeVal = 1;
    if (mIdx == 1) modeVal = 0;      // Crisp Mono
    else if (mIdx == 2) modeVal = 2; // Split Mono
    else if (mIdx == 3) modeVal = 3; // Crisp Color
    else if (mIdx == 4) modeVal = 4; // Real Color CRT
    else modeVal = 1;                // Real Plasma
    p[@"displayMode"] = @(modeVal);

    NSInteger dIdx = [self.decayPopup indexOfSelectedItem];
    NSInteger ms = 100;
    if (dIdx == 1) ms = 200;
    else if (dIdx == 2) ms = 500;
    else if (dIdx == 3) ms = 1000;
    else if (dIdx == 4) ms = 2000;
    else if (dIdx == 5) ms = 5000;
    p[@"persistenceMs"] = @(ms);

    p[@"plasmaDistortion"] = @([self.plasmaDistortionPopup indexOfSelectedItem]);

    p[@"crtBeamLevel"] = @([self.crtBeamPopup indexOfSelectedItem]);

    NSInteger cdIdx = [self.crtDecayPopup indexOfSelectedItem];
    NSInteger crtMs = 20;
    if (cdIdx == 1) crtMs = 200;
    else if (cdIdx == 2) crtMs = 500;
    else if (cdIdx == 3) crtMs = 1000;
    else if (cdIdx == 4) crtMs = 2000;
    else if (cdIdx == 5) crtMs = 5000;
    p[@"crtPersistenceMs"] = @(crtMs);

    p[@"crtDistortion"] = @([self.crtDistortionPopup indexOfSelectedItem]);

    p[@"fullScreen"] = @([self.fullscreenCheckbox state] == NSControlStateValueOn);
    p[@"startupScriptEnabled"] = @([self.scriptCheckbox state] == NSControlStateValueOn);
    p[@"startupScript"] = [self.scriptTextView string];
    BOOL isDef = ([self.defaultCheckbox state] == NSControlStateValueOn);
    p[@"isDefault"] = @(isDef);

    if (isDef) {
        for (NSMutableDictionary *other in self.profiles) {
            if (other != p) other[@"isDefault"] = @NO;
        }
    }
}

- (void)onSave:(id)sender {
    [self saveCurrentFormToProfile];
    [PLATOProfileManager saveProfiles:self.profiles];
    [self.tableView reloadData];
    if ([self.delegate respondsToSelector:@selector(profilesControllerDidUpdateProfiles:)]) {
        [self.delegate profilesControllerDidUpdateProfiles:self];
    }
}

- (void)onConnectNow:(id)sender {
    [self onSave:sender];
    NSInteger sel = self.currentEditingIndex;
    if (sel >= 0 && sel < (NSInteger)[self.profiles count]) {
        NSDictionary *p = [self.profiles[sel] copy];
        [self close];
        if ([self.delegate respondsToSelector:@selector(profilesController:didSelectConnectProfile:)]) {
            [self.delegate profilesController:self didSelectConnectProfile:p];
        }
    }
}

- (void)onAddRemove:(NSSegmentedControl *)sender {
    if ([sender selectedSegment] == 0) {
        NSMutableDictionary *newP = [@{
            @"id": [[NSUUID UUID] UUIDString],
            @"name": @"New Profile",
            @"host": @"cyberserv.org",
            @"port": @8005,
            @"displayMode": @1,
            @"persistenceMs": @100,
            @"plasmaDistortion": @2,
            @"crtBeamLevel": @1,
            @"crtPersistenceMs": @20,
            @"crtDistortion": @2,
            @"fullScreen": @YES,
            @"startupScriptEnabled": @NO,
            @"startupScript": [NSString stringWithUTF8String:PLATO_DEFAULT_AUTOLOGIN_SCRIPT],
            @"isDefault": @NO
        } mutableCopy];
        [self.profiles addObject:newP];
        [self.tableView reloadData];
        NSInteger idx = [self.profiles count] - 1;
        [self.tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:idx] byExtendingSelection:NO];
        [self loadProfileToForm:newP];
    } else {
        if ([self.profiles count] <= 1) return;
        NSInteger sel = [self.tableView selectedRow];
        if (sel >= 0) {
            [self.profiles removeObjectAtIndex:sel];
            [PLATOProfileManager saveProfiles:self.profiles];
            [self.tableView reloadData];
            if ([self.delegate respondsToSelector:@selector(profilesControllerDidUpdateProfiles:)]) {
                [self.delegate profilesControllerDidUpdateProfiles:self];
            }
            NSInteger nextSel = sel > 0 ? sel - 1 : 0;
            [self.tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:nextSel] byExtendingSelection:NO];
            [self loadProfileToForm:self.profiles[nextSel]];
        }
    }
}
@end


@interface PLATOTextBufferWindowController : NSWindowController <NSWindowDelegate, NSTextViewDelegate>
@property (nonatomic, weak) PLATOAppDelegate *appDelegate;
@property (nonatomic, strong) NSTextView *textView;
@property (nonatomic, strong) NSButton *compactCheckbox;
@property (nonatomic, strong) NSTextField *statusLabel;
@property (nonatomic, strong) NSTimer *refreshTimer;
@property (nonatomic) BOOL isFrozen;
@property (nonatomic, copy) NSString *lastText;

- (void)updateContentFromTerminal;
- (void)onCopy:(id)sender;
- (void)showAndFocus;
@end

@implementation PLATOTextBufferWindowController

- (instancetype)init {
    NSRect frame = NSMakeRect(0, 0, 560, 550);
    NSWindow *win = [[NSWindow alloc] initWithContentRect:frame
                                                styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                           NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable)
                                                  backing:NSBackingStoreBuffered defer:NO];
    [win setTitle:@"PLATO Live Text Buffer [● LIVE]"];
    [win setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]];
    [win center];

    self = [super initWithWindow:win];
    if (self) {
        [win setDelegate:self];
        _isFrozen = NO;
        _lastText = @"";

        NSView *content = [win contentView];
        NSRect contentBounds = [content bounds];

        NSRect barRect = NSMakeRect(0, 0, contentBounds.size.width, 44);
        NSView *bottomBar = [[NSView alloc] initWithFrame:barRect];
        [bottomBar setAutoresizingMask:NSViewWidthSizable | NSViewMaxYMargin];
        [bottomBar setWantsLayer:YES];
        bottomBar.layer.backgroundColor = [NSColor colorWithCalibratedRed:0.04 green:0.015 blue:0.005 alpha:1.0].CGColor;
        bottomBar.layer.borderWidth = 1.0;
        bottomBar.layer.borderColor = [NSColor colorWithCalibratedRed:0.25 green:0.10 blue:0.03 alpha:1.0].CGColor;
        [content addSubview:bottomBar];

        NSDictionary *btnTextAttrs = @{
            NSForegroundColorAttributeName: [NSColor colorWithCalibratedRed:1.0 green:0.85 blue:0.4 alpha:1.0],
            NSFontAttributeName: [NSFont systemFontOfSize:12.0 weight:NSFontWeightMedium]
        };
        NSAttributedString *(^makeBtnTitle)(NSString *) = ^(NSString *title) {
            return [[NSAttributedString alloc] initWithString:title attributes:btnTextAttrs];
        };

        NSButton *selAllBtn = [[NSButton alloc] initWithFrame:NSMakeRect(10, 10, 80, 24)];
        [selAllBtn setBezelStyle:NSBezelStyleRounded];
        [selAllBtn setBezelColor:[NSColor colorWithCalibratedRed:0.18 green:0.08 blue:0.03 alpha:1.0]];
        [selAllBtn setAttributedTitle:makeBtnTitle(@"Select All")];
        [selAllBtn setTarget:self];
        [selAllBtn setAction:@selector(onSelectAll:)];
        [bottomBar addSubview:selAllBtn];

        NSButton *selNoneBtn = [[NSButton alloc] initWithFrame:NSMakeRect(96, 10, 114, 24)];
        [selNoneBtn setBezelStyle:NSBezelStyleRounded];
        [selNoneBtn setBezelColor:[NSColor colorWithCalibratedRed:0.18 green:0.08 blue:0.03 alpha:1.0]];
        [selNoneBtn setAttributedTitle:makeBtnTitle(@"Deselect / Live")];
        [selNoneBtn setTarget:self];
        [selNoneBtn setAction:@selector(onSelectNone:)];
        [bottomBar addSubview:selNoneBtn];

        _compactCheckbox = [NSButton checkboxWithTitle:@"Compact" target:nil action:nil];
        [_compactCheckbox setFrame:NSMakeRect(216, 11, 80, 22)];
        [_compactCheckbox setAttributedTitle:makeBtnTitle(@"Compact")];
        [bottomBar addSubview:_compactCheckbox];

        NSButton *copyBtn = [[NSButton alloc] initWithFrame:NSMakeRect(302, 10, 140, 24)];
        [copyBtn setBezelStyle:NSBezelStyleRounded];
        [copyBtn setBezelColor:[NSColor colorWithCalibratedRed:0.32 green:0.14 blue:0.04 alpha:1.0]];
        [copyBtn setAttributedTitle:makeBtnTitle(@"Copy to Clipboard")];
        [copyBtn setTarget:self];
        [copyBtn setAction:@selector(onCopy:)];
        [bottomBar addSubview:copyBtn];

        _statusLabel = [NSTextField labelWithString:@"[● LIVE]"];
        [_statusLabel setFont:[NSFont boldSystemFontOfSize:11.5]];
        [_statusLabel setTextColor:[NSColor colorWithCalibratedRed:0.2 green:0.9 blue:0.3 alpha:1.0]];
        [_statusLabel setAlignment:NSTextAlignmentRight];
        [_statusLabel setFrame:NSMakeRect(barRect.size.width - 108, 12, 98, 20)];
        [_statusLabel setAutoresizingMask:NSViewMinXMargin];
        [bottomBar addSubview:_statusLabel];

        NSRect scrollRect = NSMakeRect(0, 44, contentBounds.size.width, contentBounds.size.height - 44);
        NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:scrollRect];
        [scroll setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [scroll setHasVerticalScroller:YES];
        [scroll setHasHorizontalScroller:NO];
        [scroll setBorderType:NSNoBorder];

        _textView = [[NSTextView alloc] initWithFrame:[scroll bounds]];
        [_textView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [_textView setHorizontallyResizable:NO];
        [_textView setVerticallyResizable:YES];
        [[_textView textContainer] setWidthTracksTextView:YES];
        [_textView setEditable:NO];
        [_textView setSelectable:YES];
        [_textView setDrawsBackground:YES];
        [_textView setBackgroundColor:[NSColor colorWithCalibratedRed:0.035 green:0.010 blue:0.003 alpha:1.0]];
        [_textView setTextColor:[NSColor colorWithCalibratedRed:1.0 green:0.55 blue:0.12 alpha:0.96]];
        [_textView setFont:[NSFont monospacedSystemFontOfSize:12.5 weight:NSFontWeightRegular]];
        [_textView setTextContainerInset:NSMakeSize(10.0, 10.0)];
        [_textView setDelegate:self];
        [_textView setContinuousSpellCheckingEnabled:NO];
        [_textView setGrammarCheckingEnabled:NO];
        [scroll setDocumentView:_textView];
        [content addSubview:scroll];

        __weak __typeof__(self) weakSelf = self;
        _refreshTimer = [NSTimer scheduledTimerWithTimeInterval:0.10 repeats:YES block:^(NSTimer * _Nonnull timer) {
            (void)timer;
            [weakSelf updateContentFromTerminal];
        }];
    }
    return self;
}

- (void)updateStatusLabel {
    if (self.isFrozen) {
        [self.statusLabel setStringValue:@"[⏸ FROZEN]"];
        [self.statusLabel setTextColor:[NSColor colorWithCalibratedRed:1.0 green:0.75 blue:0.1 alpha:1.0]];
        [self.window setTitle:@"PLATO Live Text Buffer [⏸ FROZEN]"];
    } else {
        [self.statusLabel setStringValue:@"[● LIVE]"];
        [self.statusLabel setTextColor:[NSColor colorWithCalibratedRed:0.2 green:0.9 blue:0.3 alpha:1.0]];
        [self.window setTitle:@"PLATO Live Text Buffer [● LIVE]"];
    }
}

- (void)textViewDidChangeSelection:(NSNotification *)notification {
    (void)notification;
    NSRange range = [self.textView selectedRange];
    if (range.length > 0) {
        if (!self.isFrozen) {
            self.isFrozen = YES;
            [self updateStatusLabel];
        }
    }
}

- (void)onSelectAll:(id)sender {
    (void)sender;
    self.isFrozen = YES;
    [self updateStatusLabel];
    [self.textView selectAll:nil];
}

- (void)onSelectNone:(id)sender {
    (void)sender;
    [self.textView setSelectedRange:NSMakeRange(0, 0)];
    self.isFrozen = NO;
    [self updateStatusLabel];
    [self updateContentFromTerminal];
}

- (void)onCopy:(id)sender {
    (void)sender;
    NSString *rawText = nil;
    NSRange range = [self.textView selectedRange];
    if (range.length > 0) {
        rawText = [[self.textView string] substringWithRange:range];
    } else {
        rawText = [self.textView string];
    }

    if (!rawText || [rawText length] == 0) return;

    NSString *toCopy = rawText;
    if ([self.compactCheckbox state] == NSControlStateValueOn) {
        NSArray<NSString *> *lines = [rawText componentsSeparatedByCharactersInSet:[NSCharacterSet newlineCharacterSet]];
        NSMutableArray<NSString *> *compactedLines = [NSMutableArray array];
        for (NSString *line in lines) {
            NSString *trimmed = [line stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
            if ([trimmed length] == 0) continue;
            NSError *err = nil;
            NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:@"[ \\t]+" options:0 error:&err];
            NSString *collapsed = [regex stringByReplacingMatchesInString:trimmed options:0 range:NSMakeRange(0, [trimmed length]) withTemplate:@" "];
            [compactedLines addObject:collapsed];
        }
        toCopy = [compactedLines componentsJoinedByString:@"\n"];
    }

    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    [pb clearContents];
    [pb setString:toCopy forType:NSPasteboardTypeString];
}

- (void)updateContentFromTerminal {
    if (self.isFrozen || ![self.window isVisible]) return;
    if (!self.appDelegate) return;
    PLATOView *v = self.appDelegate.view;
    if (!v) return;

    NSString *text = [v extractAllTextCompact:NO];
    if (!text) text = @"";
    if (![text isEqualToString:self.lastText]) {
        self.lastText = text;
        [self.textView setString:text];
    }
}

- (void)showAndFocus {
    [self updateContentFromTerminal];
    [self showWindow:nil];
    [self.window makeKeyAndOrderFront:nil];
}

- (void)windowWillClose:(NSNotification *)notification {
    (void)notification;
    self.isFrozen = NO;
    [self updateStatusLabel];
}

- (void)dealloc {
    [_refreshTimer invalidate];
}

@end

@interface PLATOAppDelegate () <PLATOScriptsDelegate,PLATOProfilesDelegate>
@property (nonatomic, strong) NSMutableArray<PLATOTerminalWindowController *> *terminalControllers;
@property (nonatomic, strong) PLATOKeymapWindowController *keymapController;
@property (nonatomic, strong) PLATOScriptRefWindowController *scriptRefController;

@property (nonatomic, strong) PLATOTextBufferWindowController *textBufferController;
@property (nonatomic, strong) PLATOProfilesWindowController *profilesController;
@property (nonatomic, strong) NSMenu *connectionSubmenu;
@property (nonatomic, strong) NSMenu *profileNewWindowSubmenu;
@property (nonatomic, strong) NSMenu *viewMenu;
@property (nonatomic, strong) NSMenu *decayMenu;
@property (nonatomic, strong) NSMenu *plasmaDistortionMenu;
@property (nonatomic, strong) NSMenu *crtBeamMenu;
@property (nonatomic, strong) NSMenu *crtDecayMenu;
@property (nonatomic, strong) NSMenu *crtDistortionMenu;
@property (nonatomic, strong) NSMenuItem *keyboardReferenceMenuItem;
@property (nonatomic, strong) NSMenuItem *fpsCounterMenuItem;
@property (nonatomic) BOOL keyboardReferenceEnabled;
@property (nonatomic) BOOL fpsCounterEnabled;
@property (nonatomic, strong) NSStatusItem *statusItem;
@property (nonatomic, strong) NSMenu *statusMenu;
@end

@implementation PLATOAppDelegate

- (PLATOTerminalWindowController *)activeTerminalController {
    NSWindow *keyWin = [NSApp keyWindow];
    for (PLATOTerminalWindowController *tc in self.terminalControllers) {
        if (tc.window == keyWin) return tc;
    }
    return [self.terminalControllers firstObject];
}

- (NSWindow *)window {
    return [self activeTerminalController].window;
}

- (PLATOView *)view {
    return [self activeTerminalController].view;
}

- (BOOL)isMainWindowFullScreen {
    NSWindow *win = self.window;
    return win && ((win.styleMask & NSWindowStyleMaskFullScreen) != 0);
}

- (PLATOTerminalWindowController *)openNewWindowWithProfile:(NSDictionary *)profile {
    PLATOTerminalWindowController *tc = [[PLATOTerminalWindowController alloc] initWithProfile:profile];
    tc.appDelegate = self;
    [self.terminalControllers addObject:tc];
    [tc.window makeKeyAndOrderFront:nil];
    NSInteger savedBeam = [[NSUserDefaults standardUserDefaults] objectForKey:@"crtBeamLevel"]
        ? [[NSUserDefaults standardUserDefaults] integerForKey:@"crtBeamLevel"] : 1; // Default High
    [tc.view setCRTBeamLevel:savedBeam];
    [self updateCRTBeamMenu:savedBeam];

    NSInteger savedDist = [[NSUserDefaults standardUserDefaults] objectForKey:@"crtDistortion"]
        ? [[NSUserDefaults standardUserDefaults] integerForKey:@"crtDistortion"] : 2; // Default Cylindrical
    [tc.view setCRTDistortion:savedDist];
    [self updateCRTDistortionMenu:savedDist];

    NSInteger savedPlasmaDist = [[NSUserDefaults standardUserDefaults] objectForKey:@"plasmaDistortion"]
        ? [[NSUserDefaults standardUserDefaults] integerForKey:@"plasmaDistortion"] : 2; // Default Cylindrical
    [tc.view setPlasmaDistortion:savedPlasmaDist];
    [self updatePlasmaDistortionMenu:savedPlasmaDist];

    NSInteger savedCRTDecay = [[NSUserDefaults standardUserDefaults] objectForKey:@"crtPersistenceMs"]
        ? [[NSUserDefaults standardUserDefaults] integerForKey:@"crtPersistenceMs"] : 20;
    [tc.view setCRTDecayDuration:(NSTimeInterval)savedCRTDecay / 1000.0];
    [self updateCRTDecayMenuForDuration:(NSTimeInterval)savedCRTDecay / 1000.0];
    return tc;
}

- (PLATOTerminalWindowController *)openNewTabWithProfile:(NSDictionary *)profile {
    NSWindow *host = [self activeTerminalController].window;
    if (!host) return [self openNewWindowWithProfile:profile];
    PLATOTerminalWindowController *tc = [[PLATOTerminalWindowController alloc] initWithProfile:profile tabbedWithWindow:host];
    tc.appDelegate = self;
    [self.terminalControllers addObject:tc];
    [tc.window makeKeyAndOrderFront:nil];
    return tc;
}

- (void)terminalWindowControllerWillClose:(PLATOTerminalWindowController *)controller {
    [self.terminalControllers removeObject:controller];
    if ([self.terminalControllers count] == 0) {
        [NSApp terminate:nil];
    }
}

- (void)activeWindowDidChange:(PLATOTerminalWindowController *)controller {
    if (!controller || !controller.view) return;
    [self updateDisplayModeMenu:[controller.view currentDisplayMode]];
    [self updatePersistenceMenuForDuration:[controller.view plasmaDecayDuration]];
    [self updateCRTBeamMenu:[controller.view crtBeamLevel]];
    [self updateCRTDistortionMenu:[controller.view crtDistortion]];
    [self updatePlasmaDistortionMenu:[controller.view plasmaDistortion]];
    [self updateCRTDecayMenuForDuration:[controller.view crtDecayDuration]];
}

- (void)applyKeyboardReferencePresentation {
    if (!self.keyboardReferenceEnabled) {
        [self.view setKeyboardReferenceVisible:NO];
        [self.keymapController.window orderOut:nil];
    } else if ([self isMainWindowFullScreen]) {
        [self.keymapController.window orderOut:nil];
        [self.view setKeyboardReferenceVisible:YES];
    } else {
        [self.view setKeyboardReferenceVisible:NO];
        [self.keymapController showWindow:nil];
        [self.keymapController.window makeKeyAndOrderFront:nil];
    }
    [self.keyboardReferenceMenuItem setState:self.keyboardReferenceEnabled ? NSControlStateValueOn : NSControlStateValueOff];
}

- (void)setupMenuBar {
    NSMenu *mainMenu = [[NSMenu alloc] init];

    // =============================================================
    // 0. App Menu: PlatoLives
    // =============================================================
    NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
    NSMenu *appMenu = [[NSMenu alloc] initWithTitle:@"PlatoLives"];
    NSMenuItem *aboutItem = [[NSMenuItem alloc] initWithTitle:@"About PlatoLives" action:@selector(showAbout:) keyEquivalent:@""];
    [aboutItem setTarget:self];
    [appMenu addItem:aboutItem];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:@"Hide PlatoLives" action:@selector(hide:) keyEquivalent:@"h"];
    [appMenu addItemWithTitle:@"Quit PlatoLives" action:@selector(terminate:) keyEquivalent:@"q"];
    [appMenuItem setSubmenu:appMenu];

    // =============================================================
    // 1. Connection (Dinamico)
    // =============================================================
    NSMenuItem *connMenuItem = [[NSMenuItem alloc] init];
    self.connectionSubmenu = [[NSMenu alloc] initWithTitle:@"Connection"];
    [self.connectionSubmenu setDelegate:self];
    [connMenuItem setSubmenu:self.connectionSubmenu];

    // =============================================================
    // 2. Scripts
    // =============================================================
    NSMenuItem *scriptsMenuItem = [[NSMenuItem alloc] init];
    NSMenu *scriptsMenu = [[NSMenu alloc] initWithTitle:@"Scripts"];
    self.scriptsSubmenu = scriptsMenu;

    NSMenuItem *manageScriptsItem = [[NSMenuItem alloc] initWithTitle:@"Manage Scripts..." action:@selector(showScriptsWindow:) keyEquivalent:@""];
    [manageScriptsItem setTarget:self];
    [scriptsMenu addItem:manageScriptsItem];

    NSMenuItem *scriptRefItem = [[NSMenuItem alloc] initWithTitle:@"Scripting Reference..." action:@selector(showScriptingReference:) keyEquivalent:@""];
    [scriptRefItem setTarget:self];
    [scriptsMenu addItem:scriptRefItem];

    NSMenuItem *cancelScriptItem = [[NSMenuItem alloc] initWithTitle:@"Cancel Script Execution" action:@selector(cancelScriptExecution:) keyEquivalent:@"X"];
    [cancelScriptItem setTarget:self];
    [cancelScriptItem setKeyEquivalentModifierMask:NSEventModifierFlagControl | NSEventModifierFlagShift];
    [scriptsMenu addItem:cancelScriptItem];

    [scriptsMenu addItem:[NSMenuItem separatorItem]];
    [scriptsMenuItem setSubmenu:scriptsMenu];

    // =============================================================
    // 3. View
    // =============================================================
    NSMenuItem *viewMenuItem = [[NSMenuItem alloc] init];
    NSMenu *viewMenu = [[NSMenu alloc] initWithTitle:@"View"];
    self.viewMenu = viewMenu;

    // --- SEZIONE MONOCROMATICA ---
    NSMenuItem *crispItem = [[NSMenuItem alloc] initWithTitle:@"Crisp Monochrome" action:@selector(setDisplayCrisp:) keyEquivalent:@""];
    [crispItem setTarget:self]; [crispItem setTag:1001];
    [viewMenu addItem:crispItem];

    NSMenuItem *splitItem = [[NSMenuItem alloc] initWithTitle:@"Split Monochrome" action:@selector(setDisplaySplit:) keyEquivalent:@""];
    [splitItem setTarget:self]; [splitItem setTag:1003];
    [viewMenu addItem:splitItem];

    NSMenuItem *plasmaItem = [[NSMenuItem alloc] initWithTitle:@"Real Plasma" action:@selector(setDisplayRealPlasma:) keyEquivalent:@""];
    [plasmaItem setTarget:self]; [plasmaItem setTag:1002]; [plasmaItem setState:NSControlStateValueOn];
    [viewMenu addItem:plasmaItem];

    NSMenuItem *decayItem = [[NSMenuItem alloc] initWithTitle:@"Real Plasma Persistence" action:nil keyEquivalent:@""];
    self.decayMenu = [[NSMenu alloc] initWithTitle:@"Real Plasma Persistence"];
    NSArray *decayOptions = @[
        @{@"title": @"100 ms (Authentic Plasma)", @"tag": @2001},
        @{@"title": @"200 ms (Warm Glow)", @"tag": @2002},
        @{@"title": @"500 ms (Medium Persistence)", @"tag": @2003},
        @{@"title": @"1000 ms (Long Persistence / Radar)", @"tag": @2004},
        @{@"title": @"2000 ms (Ultra Persistence / Phosphor)", @"tag": @2005},
        @{@"title": @"5000 ms (5s Extreme / Storage Tube)", @"tag": @2006}
    ];
    for (NSDictionary *opt in decayOptions) {
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:opt[@"title"] action:@selector(selectPlasmaDecay:) keyEquivalent:@""];
        [item setTarget:self];
        [item setTag:[opt[@"tag"] integerValue]];
        [self.decayMenu addItem:item];
    }
    [decayItem setSubmenu:self.decayMenu];
    [viewMenu addItem:decayItem];

    NSMenuItem *plasmaDistortionItem = [[NSMenuItem alloc] initWithTitle:@"Real Plasma Distortion" action:nil keyEquivalent:@""];
    self.plasmaDistortionMenu = [[NSMenu alloc] initWithTitle:@"Real Plasma Distortion"];
    NSArray *plasmaDistOptions = @[
        @{@"title": @"None (Flat)", @"tag": @6001},
        @{@"title": @"Barrel (Curved Glass)", @"tag": @6002},
        @{@"title": @"Cylindrical", @"tag": @6003}
    ];
    for (NSDictionary *opt in plasmaDistOptions) {
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:opt[@"title"] action:@selector(selectPlasmaDistortion:) keyEquivalent:@""];
        [item setTarget:self];
        [item setTag:[opt[@"tag"] integerValue]];
        if ([opt[@"tag"] integerValue] == 6003) [item setState:NSControlStateValueOn];
        [self.plasmaDistortionMenu addItem:item];
    }
    [plasmaDistortionItem setSubmenu:self.plasmaDistortionMenu];
    [viewMenu addItem:plasmaDistortionItem];

    [viewMenu addItem:[NSMenuItem separatorItem]];

    // --- SEZIONE A COLORI ---
    NSMenuItem *colorItem = [[NSMenuItem alloc] initWithTitle:@"Crisp Color" action:@selector(setDisplayCrispColor:) keyEquivalent:@""];
    [colorItem setTarget:self]; [colorItem setTag:1004];
    [viewMenu addItem:colorItem];

    NSMenuItem *crtItem = [[NSMenuItem alloc] initWithTitle:@"Real Color CRT" action:@selector(setDisplayRealColorCRT:) keyEquivalent:@""];
    [crtItem setTarget:self]; [crtItem setTag:1005];
    [viewMenu addItem:crtItem];

    NSMenuItem *crtBeamItem = [[NSMenuItem alloc] initWithTitle:@"CRT Beam Profile" action:nil keyEquivalent:@""];
    self.crtBeamMenu = [[NSMenu alloc] initWithTitle:@"CRT Beam Profile"];
    NSMenuItem *medItem = [[NSMenuItem alloc] initWithTitle:@"Standard (Authentic 13\")" action:@selector(setCRTBeamMedium:) keyEquivalent:@""];
    [medItem setTarget:self]; [medItem setTag:3001];
    NSMenuItem *highItem = [[NSMenuItem alloc] initWithTitle:@"High (Soft Glow)" action:@selector(setCRTBeamHigh:) keyEquivalent:@""];
    [highItem setTarget:self]; [highItem setTag:3002]; [highItem setState:NSControlStateValueOn];
    NSMenuItem *ultraItem = [[NSMenuItem alloc] initWithTitle:@"Ultra (Vintage Arcade)" action:@selector(setCRTBeamUltra:) keyEquivalent:@""];
    [ultraItem setTarget:self]; [ultraItem setTag:3003];
    [self.crtBeamMenu addItem:medItem];
    [self.crtBeamMenu addItem:highItem];
    [self.crtBeamMenu addItem:ultraItem];
    [crtBeamItem setSubmenu:self.crtBeamMenu];
    [viewMenu addItem:crtBeamItem];

    NSMenuItem *crtDecayItem = [[NSMenuItem alloc] initWithTitle:@"CRT Persistence" action:nil keyEquivalent:@""];
    self.crtDecayMenu = [[NSMenu alloc] initWithTitle:@"CRT Persistence"];
    NSArray *crtDecayOptions = @[
        @{@"title": @"20 ms (P22 Fast / Default CRT)", @"tag": @4001},
        @{@"title": @"200 ms (Warm Glow)", @"tag": @4002},
        @{@"title": @"500 ms (Medium Persistence)", @"tag": @4003},
        @{@"title": @"1000 ms (Long Persistence / Radar)", @"tag": @4004},
        @{@"title": @"2000 ms (Ultra Persistence / Phosphor)", @"tag": @4005},
        @{@"title": @"5000 ms (5s Extreme / Storage Tube)", @"tag": @4006}
    ];
    for (NSDictionary *opt in crtDecayOptions) {
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:opt[@"title"] action:@selector(selectCRTDecay:) keyEquivalent:@""];
        [item setTarget:self];
        [item setTag:[opt[@"tag"] integerValue]];
        if ([opt[@"tag"] integerValue] == 4001) [item setState:NSControlStateValueOn];
        [self.crtDecayMenu addItem:item];
    }
    [crtDecayItem setSubmenu:self.crtDecayMenu];
    [viewMenu addItem:crtDecayItem];

    NSMenuItem *crtDistortionItem = [[NSMenuItem alloc] initWithTitle:@"CRT Distortion" action:nil keyEquivalent:@""];
    self.crtDistortionMenu = [[NSMenu alloc] initWithTitle:@"CRT Distortion"];
    NSArray *distortionOptions = @[
        @{@"title": @"None (Flat)", @"tag": @5001},
        @{@"title": @"Barrel (Curved Glass)", @"tag": @5002},
        @{@"title": @"Cylindrical", @"tag": @5003}
    ];
    for (NSDictionary *opt in distortionOptions) {
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:opt[@"title"] action:@selector(selectCRTDistortion:) keyEquivalent:@""];
        [item setTarget:self];
        [item setTag:[opt[@"tag"] integerValue]];
        if ([opt[@"tag"] integerValue] == 5003) [item setState:NSControlStateValueOn];
        [self.crtDistortionMenu addItem:item];
    }
    [crtDistortionItem setSubmenu:self.crtDistortionMenu];
    [viewMenu addItem:crtDistortionItem];

    [viewMenu addItem:[NSMenuItem separatorItem]];

    self.keyboardReferenceMenuItem = [[NSMenuItem alloc] initWithTitle:@"Show Keyboard Reference" action:@selector(toggleKeyboardReference:) keyEquivalent:@"k"];
    [self.keyboardReferenceMenuItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];
    [self.keyboardReferenceMenuItem setTarget:self];
    [self.keyboardReferenceMenuItem setState:NSControlStateValueOff];
    [viewMenu addItem:self.keyboardReferenceMenuItem];
    [viewMenu addItem:[NSMenuItem separatorItem]];

    unichar fullScreenKey = NSF11FunctionKey, fpsCounterKey = NSF12FunctionKey;
    NSMenuItem *fullScreenItem = [[NSMenuItem alloc] initWithTitle:@"Toggle Full Screen" action:@selector(toggleMainFullScreen:) keyEquivalent:[NSString stringWithCharacters:&fullScreenKey length:1]];
    [fullScreenItem setKeyEquivalentModifierMask:0]; [fullScreenItem setTarget:self]; [viewMenu addItem:fullScreenItem];

    self.fpsCounterMenuItem = [[NSMenuItem alloc] initWithTitle:@"Show Performance HUD (FPS/CPU)" action:@selector(toggleFPSCounter:) keyEquivalent:[NSString stringWithCharacters:&fpsCounterKey length:1]];
    [self.fpsCounterMenuItem setKeyEquivalentModifierMask:0]; [self.fpsCounterMenuItem setTarget:self]; [self.fpsCounterMenuItem setState:NSControlStateValueOff];
    [viewMenu addItem:self.fpsCounterMenuItem];
    [viewMenuItem setSubmenu:viewMenu];

    // =============================================================
    // 4. Edit
    // =============================================================
    NSMenuItem *editMenuItem = [[NSMenuItem alloc] init];
    NSMenu *editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];

    NSMenuItem *copyVerbatimItem = [[NSMenuItem alloc] initWithTitle:@"Copy All Text Verbatim" action:@selector(copyAllTextVerbatim:) keyEquivalent:@"c"];
    [copyVerbatimItem setTarget:self];
    [editMenu addItem:copyVerbatimItem];

    NSMenuItem *copyCompactItem = [[NSMenuItem alloc] initWithTitle:@"Copy All Text Compact" action:@selector(copyAllTextCompact:) keyEquivalent:@"C"];
    [copyCompactItem setTarget:self];
    [copyCompactItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagShift];
    [editMenu addItem:copyCompactItem];

    NSMenuItem *copySelectedItem = [[NSMenuItem alloc] initWithTitle:@"Copy Selected Text..." action:@selector(copySelectedText:) keyEquivalent:@"T"];
    [copySelectedItem setTarget:self];
    [copySelectedItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagShift];
    [editMenu addItem:copySelectedItem];

    NSMenuItem *copyScreenItem = [[NSMenuItem alloc] initWithTitle:@"Copy Screen Image" action:@selector(copyScreenImage:) keyEquivalent:@""];
    [copyScreenItem setTarget:self];
    [editMenu addItem:copyScreenItem];

    [editMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem *pasteItem = [[NSMenuItem alloc] initWithTitle:@"Paste Text" action:@selector(pasteText:) keyEquivalent:@"v"];
    [pasteItem setTarget:self];
    [editMenu addItem:pasteItem];

    NSMenuItem *cancelPasteItem = [[NSMenuItem alloc] initWithTitle:@"Cancel Paste" action:@selector(cancelPaste:) keyEquivalent:@"."];
    [cancelPasteItem setTarget:self];
    [editMenu addItem:cancelPasteItem];
    [editMenuItem setSubmenu:editMenu];

    // =============================================================
    // 5. Micro Tutor
    // =============================================================
    NSMenuItem *microTutorMenuItem = [[NSMenuItem alloc] init];
    NSMenu *microTutorMenu = [[NSMenu alloc] initWithTitle:@"Micro Tutor"];
    NSMenuItem *bootMteItem = [[NSMenuItem alloc] initWithTitle:@"Load and Boot from MTE..." action:@selector(bootFromMte:) keyEquivalent:@""];
    [bootMteItem setTarget:self]; [microTutorMenu addItem:bootMteItem];
    NSMenuItem *mtLogItem = [[NSMenuItem alloc] initWithTitle:@"Enable Z80 Diagnostic Log" action:@selector(toggleZ80Log:) keyEquivalent:@""];
    [mtLogItem setTarget:self]; [mtLogItem setState:NSControlStateValueOff]; [microTutorMenu addItem:mtLogItem];
    [microTutorMenuItem setSubmenu:microTutorMenu];

    // =============================================================
    // 6. Window (Standard macOS - Posizione 6)
    // =============================================================
    NSMenuItem *windowMenuItem = [[NSMenuItem alloc] init];
    NSMenu *windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
    [windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [windowMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
    [windowMenu addItem:[NSMenuItem separatorItem]];

    [windowMenu addItemWithTitle:@"Show Previous Tab" action:@selector(selectPreviousTab:) keyEquivalent:@"["];
    [windowMenu addItemWithTitle:@"Show Next Tab" action:@selector(selectNextTab:) keyEquivalent:@"]"];

    NSMenuItem *selectTabItem = [[NSMenuItem alloc] initWithTitle:@"Select Tab" action:nil keyEquivalent:@""];
    NSMenu *selectTabMenu = [[NSMenu alloc] initWithTitle:@"Select Tab"];
    for (NSInteger pos = 1; pos <= 9; pos++) {
        NSString *title = (pos == 9) ? @"Last Tab" : [NSString stringWithFormat:@"Tab %ld", (long)pos];
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:@selector(selectTabAtPosition:) keyEquivalent:[NSString stringWithFormat:@"%ld", (long)pos]];
        [item setTarget:self]; [item setTag:pos];
        [selectTabMenu addItem:item];
    }
    [selectTabItem setSubmenu:selectTabMenu];
    [windowMenu addItem:selectTabItem];

    [windowMenu addItem:[NSMenuItem separatorItem]];
    [windowMenu addItemWithTitle:@"Bring All to Front" action:@selector(arrangeInFront:) keyEquivalent:@""];
    [windowMenuItem setSubmenu:windowMenu];

    // =============================================================
    // 7. Tools (Intatto - Posizione 7)
    // =============================================================
    NSMenuItem *toolsMenuItem = [[NSMenuItem alloc] init];
    NSMenu *toolsMenu = [[NSMenu alloc] initWithTitle:@"Tools"];
    NSMenuItem *textBufferItem = [[NSMenuItem alloc] initWithTitle:@"Show Live Text Buffer" action:@selector(showTextBufferWindow:) keyEquivalent:@"B"];
    [textBufferItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagShift];
    [textBufferItem setTarget:self]; [toolsMenu addItem:textBufferItem];
    NSMenuItem *logItem = [[NSMenuItem alloc] initWithTitle:@"Enable Diagnostic Log" action:@selector(toggleTrafficLog:) keyEquivalent:@""];
    [logItem setTarget:self]; [toolsMenu addItem:logItem];
    NSMenuItem *rendererLogItem = [[NSMenuItem alloc] initWithTitle:@"Enable Renderer Performance Log" action:@selector(toggleRendererPerformanceLog:) keyEquivalent:@""];
    [rendererLogItem setTarget:self]; [rendererLogItem setState:NSControlStateValueOff]; [toolsMenu addItem:rendererLogItem];
    [toolsMenuItem setSubmenu:toolsMenu];

    // =============================================================
    // 8. Help (Posizione 8)
    // =============================================================
    NSMenuItem *helpMenuItem = [[NSMenuItem alloc] init];
    NSMenu *helpMenu = [[NSMenu alloc] initWithTitle:@"Help"];
    NSMenuItem *aboutHelpItem = [[NSMenuItem alloc] initWithTitle:@"About PlatoLives..." action:@selector(showAbout:) keyEquivalent:@""];
    [aboutHelpItem setTarget:self];
    [helpMenu addItem:aboutHelpItem];
    [helpMenuItem setSubmenu:helpMenu];

    // =============================================================
    // COMPOSIZIONE BARRA DEI MENU NEL RIGOROSO ORDINE SPECIFICATO
    // =============================================================
    [mainMenu addItem:appMenuItem];        // 0. PlatoLives
    [mainMenu addItem:connMenuItem];       // 1. Connection
    [mainMenu addItem:scriptsMenuItem];    // 2. Scripts
    [mainMenu addItem:viewMenuItem];       // 3. View
    [mainMenu addItem:editMenuItem];       // 4. Edit
    [mainMenu addItem:microTutorMenuItem]; // 5. Micro Tutor
    [mainMenu addItem:windowMenuItem];     // 6. Window
    [mainMenu addItem:toolsMenuItem];      // 7. Tools
    [mainMenu addItem:helpMenuItem];       // 8. Help

    [NSApp setWindowsMenu:windowMenu];
    [NSApp setHelpMenu:helpMenu];
    [NSApp setMainMenu:mainMenu];

    [self rebuildConnectionMenu];
    [self rebuildScriptsMenu];
}

- (void)menuNeedsUpdate:(NSMenu *)menu {
    if (menu == self.connectionSubmenu) {
        [self rebuildConnectionMenu];
    [self rebuildScriptsMenu];
    } else if (menu == self.statusMenu) {
        [self rebuildStatusMenu];
    }
}

- (void)rebuildConnectionMenu {
    if (!self.connectionSubmenu) return;
    [self.connectionSubmenu removeAllItems];

    NSArray *profiles = [PLATOProfileManager loadProfiles];

    // a) Same window default profile
    NSMenuItem *defConnItem = [[NSMenuItem alloc] initWithTitle:@"Same window default profile" action:@selector(connectDefaultProfile:) keyEquivalent:@""];
    [defConnItem setTarget:self];
    [self.connectionSubmenu addItem:defConnItem];

    // b) Same window with profile (sottomenu profili)
    NSMenuItem *sameWinProfItem = [[NSMenuItem alloc] initWithTitle:@"Same window with profile" action:nil keyEquivalent:@""];
    NSMenu *sameWinProfMenu = [[NSMenu alloc] initWithTitle:@"Same window with profile"];
    for (NSDictionary *p in profiles) {
        NSString *name = p[@"name"] ? p[@"name"] : @"Untitled";
        NSString *title = [NSString stringWithFormat:@"%@", name];
        if ([p[@"isDefault"] boolValue]) title = [title stringByAppendingString:@" ★"];
        NSMenuItem *pItem = [[NSMenuItem alloc] initWithTitle:title action:@selector(quickConnectProfile:) keyEquivalent:@""];
        [pItem setRepresentedObject:p];
        [pItem setTarget:self];
        [sameWinProfMenu addItem:pItem];
    }
    if ([profiles count] == 0) {
        [sameWinProfMenu addItemWithTitle:@"(No Profiles)" action:nil keyEquivalent:@""];
    }
    [sameWinProfItem setSubmenu:sameWinProfMenu];
    [self.connectionSubmenu addItem:sameWinProfItem];

    // c) New window default profile (Cmd+N)
    NSMenuItem *newDefItem = [[NSMenuItem alloc] initWithTitle:@"New window default profile" action:@selector(newWindow:) keyEquivalent:@"n"];
    [newDefItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];
    [newDefItem setTarget:self];
    [self.connectionSubmenu addItem:newDefItem];

    // d) New window with profile (sottomenu profili)
    NSMenuItem *newWinProfItem = [[NSMenuItem alloc] initWithTitle:@"New window with profile" action:nil keyEquivalent:@""];
    NSMenu *newWinProfMenu = [[NSMenu alloc] initWithTitle:@"New window with profile"];
    for (NSDictionary *p in profiles) {
        NSString *name = p[@"name"] ? p[@"name"] : @"Untitled";
        NSMenuItem *newPItem = [[NSMenuItem alloc] initWithTitle:name action:@selector(newWindowWithProfileItem:) keyEquivalent:@""];
        [newPItem setRepresentedObject:p];
        [newPItem setTarget:self];
        [newWinProfMenu addItem:newPItem];
    }
    if ([profiles count] == 0) {
        [newWinProfMenu addItemWithTitle:@"(No Profiles)" action:nil keyEquivalent:@""];
    }
    [newWinProfItem setSubmenu:newWinProfMenu];
    [self.connectionSubmenu addItem:newWinProfItem];

    // e) Separatore
    [self.connectionSubmenu addItem:[NSMenuItem separatorItem]];

    // f) Disconnect this window (Cmd+D)
    NSMenuItem *discItem = [[NSMenuItem alloc] initWithTitle:@"Disconnect this window" action:@selector(disconnectSession:) keyEquivalent:@"d"];
    [discItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];
    [discItem setTarget:self];
    [self.connectionSubmenu addItem:discItem];

    // g) Close this window (Cmd+W)
    NSMenuItem *closeItem = [[NSMenuItem alloc] initWithTitle:@"Close this window" action:@selector(performClose:) keyEquivalent:@"w"];
    [closeItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];
    [closeItem setTarget:nil];
    [self.connectionSubmenu addItem:closeItem];

    // h) Separatore
    [self.connectionSubmenu addItem:[NSMenuItem separatorItem]];

    // i) Manage Profiles...
    NSMenuItem *manageProfilesItem = [[NSMenuItem alloc] initWithTitle:@"Manage Profiles..." action:@selector(showProfilesWindow:) keyEquivalent:@","];
    [manageProfilesItem setTarget:self];
    [self.connectionSubmenu addItem:manageProfilesItem];

    // l) Separatore
    [self.connectionSubmenu addItem:[NSMenuItem separatorItem]];

    // m) Exit (Cmd+Q)
    NSMenuItem *exitItem = [[NSMenuItem alloc] initWithTitle:@"Exit" action:@selector(terminate:) keyEquivalent:@"q"];
    [exitItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];
    [exitItem setTarget:NSApp];
    [self.connectionSubmenu addItem:exitItem];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    NSArray<NSString *> *arguments = [[NSProcessInfo processInfo] arguments];
    NSUInteger testIndex = [arguments indexOfObject:@"--test-script"];
    BOOL isHeadlessTest = (testIndex != NSNotFound);

    [self setupMenuBar];
    [self setupDockIcon];
    [self setupStatusItem];

    if (!isHeadlessTest) {
        self.keymapController = [[PLATOKeymapWindowController alloc] init];
        __weak PLATOAppDelegate *weakSelf = self;
        self.keymapController.closeHandler = ^{ weakSelf.keyboardReferenceEnabled = NO; [weakSelf applyKeyboardReferencePresentation]; };
    }
    self.keyboardReferenceEnabled = NO;

    self.terminalControllers = [NSMutableArray array];

    self.scriptList = plato_scripts_create();
    plato_scripts_load(self.scriptList, NULL);
    if (self.scriptList && self.scriptList->count == 0) {
        plato_script_t def_s = {0};
        snprintf(def_s.name, sizeof(def_s.name), "Cyber1 Auto Login");
        def_s.enabled = true;
        def_s.char_delay_ms = 20;
        def_s.next_delay_ms = 250;
        def_s.command_delay_ms = 0;
        snprintf(def_s.body, sizeof(def_s.body), "%s", PLATO_DEFAULT_AUTOLOGIN_SCRIPT);
        plato_scripts_add(self.scriptList, &def_s);
        plato_scripts_save(self.scriptList, NULL);
    }
    [self rebuildScriptsMenu];

    NSDictionary *defProf = [PLATOProfileManager defaultProfile];
    if (isHeadlessTest) {
        NSMutableDictionary *headlessProf = [defProf mutableCopy];
        headlessProf[@"fullScreen"] = @NO;
        defProf = headlessProf;
    }
    [self openNewWindowWithProfile:defProf];

    if (isHeadlessTest) {
        if (testIndex + 1 >= [arguments count]) {
            NSLog(@"[TEST] ERROR: --test-script requires a file path");
            exit(1);
        } else {
            self.testRunner = [[PLATOTestRunner alloc] initWithView:self.view scriptPath:arguments[testIndex + 1]];
            [self.testRunner start];
        }
    } else {
        [NSApp activateIgnoringOtherApps:YES];
    }
}



- (void)setupDockIcon {
    NSString *iconPath = [[NSBundle mainBundle] pathForResource:@"PlatoLives" ofType:@"icns"];
    NSImage *dockIcon = nil;
    if (iconPath) {
        dockIcon = [[NSImage alloc] initWithContentsOfFile:iconPath];
    }
    if (!dockIcon) {
        NSString *resPath = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"PlatoLives.icns"];
        dockIcon = [[NSImage alloc] initWithContentsOfFile:resPath];
    }
    if (!dockIcon) {
        dockIcon = [[NSImage alloc] initWithContentsOfFile:@"platolives-mac/PlatoLives.icns"];
    }
    if (!dockIcon) {
        dockIcon = [[NSImage alloc] initWithContentsOfFile:@"../platolives-mac/PlatoLives.icns"];
    }
    if (dockIcon) {
        [NSApp setApplicationIconImage:dockIcon];
    }
}

- (void)setupStatusItem {
    self.statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSSquareStatusItemLength];
    if (!self.statusItem) return;

    NSImage *icon = [self createStatusBarIcon];
    [self.statusItem.button setImage:icon];
    [self.statusItem.button setToolTip:@"PlatoLives - PLATO Terminal"];

    self.statusMenu = [[NSMenu alloc] initWithTitle:@"PlatoLivesStatusMenu"];
    [self.statusMenu setDelegate:self];
    [self rebuildStatusMenu];
    [self.statusItem setMenu:self.statusMenu];
}

- (NSImage *)createStatusBarIcon {
    return [NSImage imageWithSize:NSMakeSize(18, 18) flipped:NO drawingHandler:^BOOL(NSRect dstRect) {
        NSBezierPath *screen = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(1.0, 1.0, 16.0, 16.0) xRadius:3.0 yRadius:3.0];
        [[NSColor colorWithCalibratedRed:0.06 green:0.02 blue:0.01 alpha:1.0] setFill];
        [screen fill];
        [[NSColor colorWithCalibratedRed:0.95 green:0.42 blue:0.0 alpha:0.85] setStroke];
        [screen setLineWidth:1.0];
        [screen stroke];

        NSDictionary *attrs = @{
            NSFontAttributeName: [NSFont boldSystemFontOfSize:11.0],
            NSForegroundColorAttributeName: [NSColor colorWithCalibratedRed:1.0 green:0.62 blue:0.05 alpha:1.0]
        };
        NSString *glyph = @"P";
        NSSize strSize = [glyph sizeWithAttributes:attrs];
        NSRect textRect = NSMakeRect((18.0 - strSize.width) / 2.0, (18.0 - strSize.height) / 2.0 + 0.5, strSize.width, strSize.height);
        [glyph drawInRect:textRect withAttributes:attrs];
        return YES;
    }];
}

- (void)rebuildStatusMenu {
    if (!self.statusMenu) return;
    [self.statusMenu removeAllItems];

    NSMenuItem *headerItem = [[NSMenuItem alloc] initWithTitle:@"PlatoLives 4.3" action:nil keyEquivalent:@""];
    [headerItem setEnabled:NO];
    [self.statusMenu addItem:headerItem];

    PLATOTerminalWindowController *active = [self activeTerminalController];
    NSString *statusText = @"Status: Ready";
    if (active && active.view && active.view->terminal) {
        plato_terminal_t *t = active.view->terminal;
        NSString *pName = active.currentProfile[@"name"] ? active.currentProfile[@"name"] : @"Cyber1";
        if (t->user_name[0] && t->user_group[0]) {
            statusText = [NSString stringWithFormat:@"%@ (%s/%s)", pName, t->user_name, t->user_group];
        } else if (active.view->transport && active.view->transport->connected) {
            statusText = [NSString stringWithFormat:@"%@: Connected", pName];
        } else {
            statusText = [NSString stringWithFormat:@"%@: Connecting...", pName];
        }
    }
    NSMenuItem *infoItem = [[NSMenuItem alloc] initWithTitle:statusText action:nil keyEquivalent:@""];
    [infoItem setEnabled:NO];
    [self.statusMenu addItem:infoItem];

    [self.statusMenu addItem:[NSMenuItem separatorItem]];

    NSArray *profiles = [PLATOProfileManager loadProfiles];

    // Nuova finestra standard
    NSMenuItem *newWin = [[NSMenuItem alloc] initWithTitle:@"New Window" action:@selector(newWindow:) keyEquivalent:@""];
    [newWin setTarget:self];
    [self.statusMenu addItem:newWin];

    // Nuova finestra con profilo specifico
    NSMenuItem *newWinProfItem = [[NSMenuItem alloc] initWithTitle:@"New Window with Profile" action:nil keyEquivalent:@""];
    NSMenu *newWinProfSub = [[NSMenu alloc] initWithTitle:@"New Window with Profile"];
    for (NSDictionary *p in profiles) {
        NSString *name = p[@"name"] ? p[@"name"] : @"Untitled";
        if ([p[@"isDefault"] boolValue]) name = [name stringByAppendingString:@" ★"];
        NSMenuItem *npi = [[NSMenuItem alloc] initWithTitle:name action:@selector(newWindowWithProfileItem:) keyEquivalent:@""];
        [npi setRepresentedObject:p];
        [npi setTarget:self];
        [newWinProfSub addItem:npi];
    }
    [newWinProfItem setSubmenu:newWinProfSub];
    [self.statusMenu addItem:newWinProfItem];

    [self.statusMenu addItem:[NSMenuItem separatorItem]];

    // Connessione profilo predefinito
    NSMenuItem *connDef = [[NSMenuItem alloc] initWithTitle:@"Connect to Default Profile" action:@selector(connectDefaultProfile:) keyEquivalent:@""];
    [connDef setTarget:self];
    [self.statusMenu addItem:connDef];

    // Connessione rapida sessione corrente
    NSMenuItem *quickSubItem = [[NSMenuItem alloc] initWithTitle:@"Quick Connect" action:nil keyEquivalent:@""];
    NSMenu *quickSub = [[NSMenu alloc] initWithTitle:@"Quick Connect"];
    for (NSDictionary *p in profiles) {
        NSString *name = p[@"name"] ? p[@"name"] : @"Untitled";
        if ([p[@"isDefault"] boolValue]) name = [name stringByAppendingString:@" ★"];
        NSMenuItem *pi = [[NSMenuItem alloc] initWithTitle:name action:@selector(quickConnectProfile:) keyEquivalent:@""];
        [pi setRepresentedObject:p];
        [pi setTarget:self];
        [quickSub addItem:pi];
    }
    [quickSubItem setSubmenu:quickSub];
    [self.statusMenu addItem:quickSubItem];

    NSMenuItem *profItem = [[NSMenuItem alloc] initWithTitle:@"Connection Profiles..." action:@selector(showProfilesWindow:) keyEquivalent:@""];
    [profItem setTarget:self];
    [self.statusMenu addItem:profItem];

    [self.statusMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem *discItem = [[NSMenuItem alloc] initWithTitle:@"Disconnect" action:@selector(disconnectSession:) keyEquivalent:@""];
    [discItem setTarget:self];
    [self.statusMenu addItem:discItem];

    [self.statusMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem *about = [[NSMenuItem alloc] initWithTitle:@"About PlatoLives" action:@selector(showAbout:) keyEquivalent:@""];
    [about setTarget:self];
    [self.statusMenu addItem:about];

    NSMenuItem *quit = [[NSMenuItem alloc] initWithTitle:@"Quit PlatoLives" action:@selector(terminate:) keyEquivalent:@"q"];
    [quit setTarget:NSApp];
    [self.statusMenu addItem:quit];
}

- (void)newWindow:(id)sender {
    NSDictionary *defProf = [PLATOProfileManager defaultProfile];
    [self openNewWindowWithProfile:defProf];
}

- (void)newWindowWithProfileItem:(id)sender {
    NSMenuItem *item = (NSMenuItem *)sender;
    NSDictionary *p = [item representedObject];
    if (p) {
        [self openNewWindowWithProfile:p];
    }
}

- (void)updateDisplayModeMenu:(NSInteger)mode {
    NSInteger targetTag = 1002;
    if (mode == 0) targetTag = 1001;
    else if (mode == 1) targetTag = 1002;
    else if (mode == 2) targetTag = 1003;
    else if (mode == 3) targetTag = 1004;
    else if (mode == 4) targetTag = 1005;

    NSMenu *targetMenu = self.viewMenu;
    if (!targetMenu) {
        targetMenu = [[[NSApp mainMenu] itemWithTitle:@"View"] submenu];
    }
    if (targetMenu) {
        for (NSInteger tag = 1001; tag <= 1005; tag++) {
            [[targetMenu itemWithTag:tag] setState:(tag == targetTag ? NSControlStateValueOn : NSControlStateValueOff)];
        }
    }
}

- (void)selectDisplayMenuItem:(NSMenuItem *)selectedItem {
    if (!selectedItem) return;
    NSInteger tag = [selectedItem tag];
    NSInteger m = (tag == 1001 ? 0 : (tag == 1003 ? 2 : (tag == 1004 ? 3 : (tag == 1005 ? 4 : 1))));
    [self updateDisplayModeMenu:m];
}

- (void)updatePersistenceMenuForDuration:(NSTimeInterval)duration {
    NSInteger targetTag = 2001;
    NSInteger ms = (NSInteger)(duration * 1000.0 + 0.5);
    if (ms >= 4000) targetTag = 2006;
    else if (ms >= 1800) targetTag = 2005;
    else if (ms >= 800) targetTag = 2004;
    else if (ms >= 400) targetTag = 2003;
    else if (ms >= 150) targetTag = 2002;
    else targetTag = 2001;

    if (self.decayMenu) {
        for (NSMenuItem *item in [self.decayMenu itemArray]) {
            [item setState:([item tag] == targetTag ? NSControlStateValueOn : NSControlStateValueOff)];
        }
    }
}

- (void)requestDisplayMode:(NSInteger)newMode sender:(id)sender {
    PLATOTerminalWindowController *active = [self activeTerminalController];
    if (!active || !active.view) return;

    NSInteger currentMode = [active.view currentDisplayMode];
    if (currentMode == newMode) return;

    BOOL currentIsColor = (currentMode == 3 || currentMode == 4);
    BOOL newIsColor = (newMode == 3 || newMode == 4);
    BOOL isConnected = (active.view->transport && active.view->transport->connected);

    if (isConnected && (currentIsColor != newIsColor)) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Reconnect Required for Display Mode Change"];
        [alert setInformativeText:@"Switching between Monochrome and Color requires reconnecting to negotiate terminal capabilities with the PLATO mainframe. Do you want to reconnect now?"];
        [alert addButtonWithTitle:@"Reconnect"];
        [alert addButtonWithTitle:@"Cancel"];
        [alert setAlertStyle:NSAlertStyleInformational];

        NSModalResponse resp = [alert runModal];
        if (resp != NSAlertFirstButtonReturn) {
            [self updateDisplayModeMenu:currentMode];
            return;
        }

        // Applica modalita sulla vista attiva
        if (newMode == 0) [active.view setDisplayCrisp];
        else if (newMode == 1) [active.view setDisplayRealPlasma];
        else if (newMode == 2) [active.view setDisplaySplit];
        else if (newMode == 3) [active.view setDisplayCrispColor];
        else if (newMode == 4) [active.view setDisplayRealColorCRT];

        [self updateDisplayModeMenu:newMode];

        // Riconnette immediatamente la sessione corrente con la nuova modalita
        if (active.currentProfile) {
            NSMutableDictionary *p = [active.currentProfile mutableCopy];
            p[@"displayMode"] = @(newMode);
            [active applyProfile:p];
        } else {
            NSDictionary *def = [PLATOProfileManager defaultProfile];
            [active applyProfile:def];
        }
        return;
    }

    // Cambio istantaneo tra modalita dello stesso tipo (es. Real Plasma <-> Crisp)
    if (newMode == 0) [active.view setDisplayCrisp];
    else if (newMode == 1) [active.view setDisplayRealPlasma];
    else if (newMode == 2) [active.view setDisplaySplit];
    else if (newMode == 3) [active.view setDisplayCrispColor];
        else if (newMode == 4) [active.view setDisplayRealColorCRT];

    [self updateDisplayModeMenu:newMode];
}

- (void)setDisplayCrisp:(id)sender {
    [self requestDisplayMode:0 sender:sender];
}

- (void)setDisplayRealPlasma:(id)sender {
    [self requestDisplayMode:1 sender:sender];
}

- (void)setDisplaySplit:(id)sender {
    [self requestDisplayMode:2 sender:sender];
}

- (void)setDisplayCrispColor:(id)sender {
    [self requestDisplayMode:3 sender:sender];
}
- (void)setDisplayRealColorCRT:(id)sender {
    [self requestDisplayMode:4 sender:sender];
}

- (void)setCRTBeamMedium:(id)sender { [self selectCRTBeamLevel:0]; }
- (void)setCRTBeamHigh:(id)sender   { [self selectCRTBeamLevel:1]; }
- (void)setCRTBeamUltra:(id)sender  { [self selectCRTBeamLevel:2]; }

- (void)selectCRTBeamLevel:(NSInteger)level {
    PLATOTerminalWindowController *active = [self activeTerminalController];
    if (active && active.view) {
        [active.view setCRTBeamLevel:level];
    }
    [self updateCRTBeamMenu:level];
    [[NSUserDefaults standardUserDefaults] setInteger:level forKey:@"crtBeamLevel"];
}

- (void)updateCRTBeamMenu:(NSInteger)level {
    NSInteger targetTag = 3001 + level;
    if (self.crtBeamMenu) {
        for (NSMenuItem *item in [self.crtBeamMenu itemArray]) {
            [item setState:([item tag] == targetTag ? NSControlStateValueOn : NSControlStateValueOff)];
        }
    }
}

- (void)selectCRTDecay:(id)sender {
    NSMenuItem *item = (NSMenuItem *)sender;
    NSInteger tag = [item tag];
    NSTimeInterval duration = 0.02;
    switch (tag) {
        case 4001: duration = 0.02; break;
        case 4002: duration = 0.20; break;
        case 4003: duration = 0.50; break;
        case 4004: duration = 1.00; break;
        case 4005: duration = 2.00; break;
        case 4006: duration = 5.00; break;
        default: break;
    }
    PLATOTerminalWindowController *active = [self activeTerminalController];
    if (active && active.view) {
        [active.view setCRTDecayDuration:duration];
    }
    [self updateCRTDecayMenuForDuration:duration];
    [[NSUserDefaults standardUserDefaults] setInteger:(NSInteger)(duration * 1000.0 + 0.5) forKey:@"crtPersistenceMs"];
    [[NSUserDefaults standardUserDefaults] setInteger:tag forKey:@"crtPersistenceTag"];
}

- (void)updateCRTDecayMenuForDuration:(NSTimeInterval)duration {
    NSInteger targetTag = 4001;
    NSInteger ms = (NSInteger)(duration * 1000.0 + 0.5);
    if (ms >= 4000) targetTag = 4006;
    else if (ms >= 1800) targetTag = 4005;
    else if (ms >= 800) targetTag = 4004;
    else if (ms >= 400) targetTag = 4003;
    else if (ms >= 150) targetTag = 4002;
    else targetTag = 4001;

    if (self.crtDecayMenu) {
        for (NSMenuItem *item in [self.crtDecayMenu itemArray]) {
            [item setState:([item tag] == targetTag ? NSControlStateValueOn : NSControlStateValueOff)];
        }
    }
}

- (void)selectPlasmaDistortion:(id)sender {
    NSMenuItem *item = (NSMenuItem *)sender;
    NSInteger tag = [item tag];
    NSInteger dist = (tag == 6002) ? 1 : ((tag == 6003) ? 2 : 0);
    PLATOTerminalWindowController *active = [self activeTerminalController];
    if (active && active.view) {
        [active.view setPlasmaDistortion:dist];
    }
    [self updatePlasmaDistortionMenu:dist];
    [[NSUserDefaults standardUserDefaults] setInteger:dist forKey:@"plasmaDistortion"];
    [[NSUserDefaults standardUserDefaults] setInteger:tag forKey:@"plasmaDistortionTag"];
}

- (void)updatePlasmaDistortionMenu:(NSInteger)dist {
    NSInteger targetTag = (dist == 1) ? 6002 : ((dist == 2) ? 6003 : 6001);
    if (self.plasmaDistortionMenu) {
        for (NSMenuItem *item in [self.plasmaDistortionMenu itemArray]) {
            [item setState:([item tag] == targetTag ? NSControlStateValueOn : NSControlStateValueOff)];
        }
    }
}

- (void)selectCRTDistortion:(id)sender {
    NSMenuItem *item = (NSMenuItem *)sender;
    NSInteger tag = [item tag];
    NSInteger dist = (tag == 5002) ? 1 : ((tag == 5003) ? 2 : 0);
    PLATOTerminalWindowController *active = [self activeTerminalController];
    if (active && active.view) {
        [active.view setCRTDistortion:dist];
    }
    [self updateCRTDistortionMenu:dist];
    [[NSUserDefaults standardUserDefaults] setInteger:dist forKey:@"crtDistortion"];
    [[NSUserDefaults standardUserDefaults] setInteger:tag forKey:@"crtDistortionTag"];
}

- (void)updateCRTDistortionMenu:(NSInteger)dist {
    NSInteger targetTag = (dist == 1) ? 5002 : ((dist == 2) ? 5003 : 5001);
    if (self.crtDistortionMenu) {
        for (NSMenuItem *item in [self.crtDistortionMenu itemArray]) {
            [item setState:([item tag] == targetTag ? NSControlStateValueOn : NSControlStateValueOff)];
        }
    }
}


- (void)selectPlasmaDecay:(id)sender {
    NSMenuItem *selectedItem = (NSMenuItem *)sender;
    NSTimeInterval duration = 0.10;
    switch ([selectedItem tag]) {
        case 2001: duration = 0.10; break;
        case 2002: duration = 0.20; break;
        case 2003: duration = 0.50; break;
        case 2004: duration = 1.00; break;
        case 2005: duration = 2.00; break;
        case 2006: duration = 5.00; break;
        default: break;
    }
    [self.view setPlasmaDecayDuration:duration];
    [self updatePersistenceMenuForDuration:duration];
}

- (void)toggleMainFullScreen:(id)sender {
    if (self.window) [self.window toggleFullScreen:sender];
}

- (void)toggleFPSCounter:(id)sender {
    self.fpsCounterEnabled = !self.fpsCounterEnabled;
    [self.view setFPSCounterVisible:self.fpsCounterEnabled];
    [self.fpsCounterMenuItem setTitle:self.fpsCounterEnabled ? @"Hide Performance HUD (FPS/CPU)" : @"Show Performance HUD (FPS/CPU)"];
    [self.fpsCounterMenuItem setState:self.fpsCounterEnabled ? NSControlStateValueOn : NSControlStateValueOff];
}

- (void)toggleKeyboardReference:(id)sender {
    self.keyboardReferenceEnabled = !self.keyboardReferenceEnabled;
    [self applyKeyboardReferencePresentation];
}

- (void)copyAllTextVerbatim:(id)sender {
    [self.view copyTextToPasteboardCompact:NO];
}

- (void)copyAllTextCompact:(id)sender {
    [self.view copyTextToPasteboardCompact:YES];
}

- (void)copySelectedText:(id)sender {
    [self showTextBufferWindow:sender];
}

- (void)copyScreen:(id)sender {
    [self copyAllTextVerbatim:sender];
}

- (void)copyScreenImage:(id)sender {
    [self.view copyScreenToPasteboard];
}

- (void)copyTextAction:(id)sender {
    if (!self.textBufferController) {
        self.textBufferController = [[PLATOTextBufferWindowController alloc] init];
        self.textBufferController.appDelegate = self;
    }
    if ([self.textBufferController.window isVisible]) {
        [self.textBufferController onCopy:sender];
    } else {
        [self.textBufferController showAndFocus];
    }
}

- (void)showTextBufferWindow:(id)sender {
    if (!self.textBufferController) {
        self.textBufferController = [[PLATOTextBufferWindowController alloc] init];
        self.textBufferController.appDelegate = self;
    }
    [self.textBufferController showAndFocus];
}

- (void)pasteText:(id)sender {
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    NSString *text = [pb stringForType:NSPasteboardTypeString];
    if (text && [text length] > 0) {
        [self.view pasteText:text];
    }
}

- (void)cancelPaste:(id)sender {
    [self.view cancelPaste];
}

- (void)showProfilesWindow:(id)sender {
    if (!self.profilesController) {
        self.profilesController = [[PLATOProfilesWindowController alloc] init];
        self.profilesController.delegate = self;
    }
    [self.profilesController showWindow:nil];
    [self.profilesController.window makeKeyAndOrderFront:nil];
}

- (void)profilesController:(PLATOProfilesWindowController *)controller didSelectConnectProfile:(NSDictionary *)profile {
    PLATOTerminalWindowController *active = [self activeTerminalController];
    if (active) {
        [active applyProfile:profile];
    } else {
        [self openNewWindowWithProfile:profile];
    }
    [self rebuildConnectionMenu];
    [self rebuildScriptsMenu];
}

- (void)profilesControllerDidUpdateProfiles:(PLATOProfilesWindowController *)controller {
    [self rebuildConnectionMenu];
    [self rebuildScriptsMenu];
}

- (void)connectDefaultProfile:(id)sender {
    NSDictionary *def = [PLATOProfileManager defaultProfile];
    PLATOTerminalWindowController *active = [self activeTerminalController];
    if (active) [active applyProfile:def];
    else [self openNewWindowWithProfile:def];
}

- (void)quickConnectProfile:(id)sender {
    NSMenuItem *item = (NSMenuItem *)sender;
    NSDictionary *p = [item representedObject];
    if (p) {
        PLATOTerminalWindowController *active = [self activeTerminalController];
        if (active) [active applyProfile:p];
        else [self openNewWindowWithProfile:p];
    }
}

- (void)disconnectSession:(id)sender {
    PLATOTerminalWindowController *active = [self activeTerminalController];
    if (active) {
        [active.view disconnect];
        [active.window setTitle:@"PlatoLives - Disconnected"];
    }
}

- (void)showAbout:(id)sender {
    NSString *version = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
    if (!version) version = @"4.3";
    NSDictionary *options = @{
        NSAboutPanelOptionApplicationName: @"PlatoLives",
        NSAboutPanelOptionApplicationVersion: version,
        NSAboutPanelOptionVersion: @"Build 1",
        NSAboutPanelOptionCredits: [[NSAttributedString alloc] initWithString:
            @"A modern, native PLATO terminal emulator for macOS.\n\nCreated by Fabio Montarsolo\nfabio.montarsolo@gmail.com\n\n"
             "Open-source software. Keeping the PLATO experience alive on modern hardware."]
    };
    [NSApp orderFrontStandardAboutPanelWithOptions:options];
}

- (void)toggleTrafficLog:(id)sender {
    if (!self.view || !self.view->transport) return;
    bool enabled = !plato_transport_is_logging(self.view->transport);
    plato_transport_set_logging(self.view->transport, enabled);
    NSMenuItem *item = (NSMenuItem *)sender;
    [item setTitle:enabled ? @"Disable Diagnostic Log" : @"Enable Diagnostic Log"];
    [item setState:enabled ? NSControlStateValueOn : NSControlStateValueOff];
}

- (void)dumpZ80Ram:(id)sender {
    if (!self.view || !self.view->terminal) return;
    plato_microtutor_dump_ram(&self.view->terminal->mt, "z80ram.dmp");
}

- (void)bootFromMte:(id)sender {
    if (!self.view || !self.view->terminal) return;
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    [panel setCanChooseFiles:YES];
    [panel setCanChooseDirectories:NO];
    [panel setAllowsMultipleSelection:NO];
    [panel setMessage:@"Select MicroTutor Executable (.mte) Floppy Image"];
    [panel setPrompt:@"Boot"];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [panel setAllowedFileTypes:@[@"mte"]];
#pragma clang diagnostic pop

    if ([panel runModal] == NSModalResponseOK) {
        NSURL *url = [[panel URLs] firstObject];
        if (url) {
            const char *path = [[url path] UTF8String];
            if (path) {
                if (self.view.scriptRunner && plato_script_is_running(self.view.scriptRunner)) {
                    plato_script_stop(self.view.scriptRunner);
                }
                plato_microtutor_boot_mte(&self.view->terminal->mt, path);
                [self.view setNeedsDisplay:YES];
            }
        }
    }
}

- (void)toggleZ80Log:(id)sender {
    if (!self.view) return;
    NSMenuItem *item = (NSMenuItem *)sender;
    BOOL enabled = [item state] != NSControlStateValueOn;
    [self.view setMicroTutorLogEnabled:enabled];
    [item setTitle:enabled ? @"Disable Z80 Diagnostic Log" : @"Enable Z80 Diagnostic Log"];
    [item setState:enabled ? NSControlStateValueOn : NSControlStateValueOff];
}

- (void)toggleRendererPerformanceLog:(id)sender {
    NSMenuItem *item = (NSMenuItem *)sender;
    BOOL enabled = [item state] != NSControlStateValueOn;
    [self.view setRendererPerformanceLogEnabled:enabled];
    [item setTitle:enabled ? @"Disable Renderer Performance Log" : @"Enable Renderer Performance Log"];
    [item setState:enabled ? NSControlStateValueOn : NSControlStateValueOff];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}



- (void)newWindowForTab:(id)sender {
    NSDictionary *defProf = [PLATOProfileManager defaultProfile];
    [self openNewTabWithProfile:defProf];
}

- (void)selectTabAtPosition:(id)sender {
    NSWindow *win = [self activeTerminalController].window;
    if (!win) return;
    NSArray<NSWindow *> *tabs = win.tabbedWindows ? win.tabbedWindows : @[win];
    NSInteger pos = [(NSMenuItem *)sender tag];
    NSInteger idx = (pos == 9) ? (NSInteger)[tabs count] - 1 : pos - 1;
    if (idx < 0 || idx >= (NSInteger)[tabs count]) return;
    [tabs[idx] makeKeyAndOrderFront:nil];
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
    if (self.scriptList) {
        plato_scripts_save(self.scriptList, NULL);
        plato_scripts_free(self.scriptList);
        self.scriptList = NULL;
    }
}


- (void)showScriptingReference:(id)sender {
    if (!self.scriptRefController) {
        self.scriptRefController = [[PLATOScriptRefWindowController alloc] init];
    }
    [self.scriptRefController showWindow:nil];
    [self.scriptRefController.window makeKeyAndOrderFront:nil];
}

- (void)showScriptsWindow:(id)sender {
    if (!self.scriptsController) {
        self.scriptsController = [[PLATOScriptsWindowController alloc] initWithScriptList:self.scriptList];
        self.scriptsController.delegate = self;
    }
    [self.scriptsController refreshList];
    [self.scriptsController showWindow:nil];
    [self.scriptsController.window makeKeyAndOrderFront:nil];
}

- (void)cancelScriptExecution:(id)sender {
    PLATOTerminalWindowController *tc = [self activeTerminalController];
    if (tc && tc->_scriptRunner) {
        plato_script_stop(tc->_scriptRunner);
    }
}

- (void)executeScriptMenuItem:(id)sender {
    NSInteger idx = [sender tag];
    if (self.scriptList && idx >= 0 && idx < (NSInteger)self.scriptList->count) {
        plato_script_t *s = &self.scriptList->scripts[idx];
        PLATOTerminalWindowController *tc = [self activeTerminalController];
        if (tc) {
            [tc runScriptStruct:s];
        }
    }
}

- (void)rebuildScriptsMenu {
    if (!self.scriptsSubmenu) return;
    while ([self.scriptsSubmenu numberOfItems] > 3) {
        [self.scriptsSubmenu removeItemAtIndex:3];
    }
    if (self.scriptList) {
        for (size_t i = 0; i < self.scriptList->count; i++) {
            plato_script_t *s = &self.scriptList->scripts[i];
            if (s->enabled) {
                NSString *title = [NSString stringWithUTF8String:s->name];
                if (strlen(s->hotkey_display) > 0) {
                    title = [title stringByAppendingFormat:@"  (%s)", s->hotkey_display];
                }
                NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:@selector(executeScriptMenuItem:) keyEquivalent:@""];
                [item setTarget:self];
                [item setTag:i];
                [self.scriptsSubmenu addItem:item];
            }
        }
    }
}

- (void)scriptsControllerDidUpdateScripts:(PLATOScriptsWindowController *)controller {
    if (self.scriptList) {
        plato_scripts_save(self.scriptList, NULL);
        [self rebuildScriptsMenu];
    }
}

@end