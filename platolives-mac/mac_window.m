#import "mac_window.h"
#import "test_runner.h"

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

@implementation PLATOTerminalWindowController

- (instancetype)initWithProfile:(NSDictionary *)profile {
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

        [self applyProfile:profile];
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
    if (!p) return;
    _currentProfile = [p copy];

    [self.view resetTerminal];

    NSInteger mode = [p[@"displayMode"] integerValue];
    if (mode == 0) [self.view setDisplayCrisp];
    else if (mode == 2) [self.view setDisplaySplit];
    else [self.view setDisplayRealPlasma];

    NSInteger ms = [p[@"persistenceMs"] integerValue];
    if (ms <= 0) ms = 100;
    [self.view setPlasmaDecayDuration:(NSTimeInterval)ms / 1000.0];

    NSString *host = p[@"host"] ? p[@"host"] : @"cyberserv.org";
    int pVal = [p[@"port"] intValue];
    int port = (pVal > 0 ? pVal : 8005);
    [self.view connectToHost:host port:port];

    NSString *name = p[@"name"] ? p[@"name"] : @"PLATO";
    [self.window setTitle:[NSString stringWithFormat:@"PlatoLives - %@ (%@:%d)", name, host, port]];
    [self.window makeKeyAndOrderFront:nil];

    BOOL wantFull = [p[@"fullScreen"] boolValue];
    BOOL isFull = ((self.window.styleMask & NSWindowStyleMaskFullScreen) != 0);
    if (wantFull != isFull) {
        dispatch_async(dispatch_get_main_queue(), ^{ [self.window toggleFullScreen:nil]; });
    }
}

- (void)windowWillClose:(NSNotification *)notification {
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

@implementation PLATOKeymapWindowController
- (instancetype)init {
    NSRect frame = NSMakeRect(0, 0, 620, 720);
    NSWindow *window = [[PLATOKeymapWindow alloc] initWithContentRect:frame
                                                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                             NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable)
                                                    backing:NSBackingStoreBuffered defer:NO];
    self = [super initWithWindow:window];
    if (self) {
        [window setTitle:@"PLATO Keyboard Reference"];
        [window setDelegate:self];
        [window center];
        NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:[[window contentView] bounds]];
        [scroll setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [scroll setHasVerticalScroller:YES];
        [scroll setBorderType:NSNoBorder];
        NSTextView *textView = [[NSTextView alloc] initWithFrame:[[window contentView] bounds]];
        [textView setEditable:NO];
        [textView setSelectable:YES];
        [textView setDrawsBackground:YES];
        [textView setBackgroundColor:[NSColor colorWithCalibratedRed:0.035 green:0.010 blue:0.003 alpha:1.0]];
        [textView setTextColor:[NSColor colorWithCalibratedRed:1.0 green:0.48 blue:0.12 alpha:0.96]];
        [textView setFont:[NSFont monospacedSystemFontOfSize:14.0 weight:NSFontWeightRegular]];
        [textView setTextContainerInset:NSMakeSize(18.0, 18.0)];
        [[textView textStorage] setAttributedString:[[NSAttributedString alloc] initWithString:PLATOKeyboardReferenceText()
                                                                                   attributes:@{
            NSFontAttributeName: [NSFont monospacedSystemFontOfSize:14.0 weight:NSFontWeightRegular],
            NSForegroundColorAttributeName: [NSColor colorWithCalibratedRed:1.0 green:0.48 blue:0.12 alpha:0.96]
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
@property (nonatomic, strong) NSPopUpButton *decayPopup;
@property (nonatomic, strong) NSButton *fullscreenCheckbox;
@property (nonatomic, strong) NSButton *defaultCheckbox;
@end

@implementation PLATOProfilesWindowController

- (instancetype)init {
    NSRect frame = NSMakeRect(0, 0, 640, 430);
    NSWindow *win = [[NSWindow alloc] initWithContentRect:frame
                                                styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                                                  backing:NSBackingStoreBuffered defer:NO];
    [win setTitle:@"Connection Profiles"];
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

    NSScrollView *tableScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 55, 190, 350)];
    [tableScroll setHasVerticalScroller:YES];
    [tableScroll setBorderType:NSBezelBorder];

    _tableView = [[NSTableView alloc] initWithFrame:[tableScroll bounds]];
    NSTableColumn *col = [[NSTableColumn alloc] initWithIdentifier:@"name"];
    [col setTitle:@"Profiles"];
    [col setWidth:170];
    [_tableView addTableColumn:col];
    [_tableView setHeaderView:nil];
    [_tableView setDataSource:self];
    [_tableView setDelegate:self];
    [tableScroll setDocumentView:_tableView];
    [content addSubview:tableScroll];

    NSSegmentedControl *seg = [NSSegmentedControl segmentedControlWithLabels:@[@"+", @"-"]
                                                               trackingMode:NSSegmentSwitchTrackingMomentary
                                                                     target:self
                                                                     action:@selector(onAddRemove:)];
    [seg setFrame:NSMakeRect(20, 20, 60, 24)];
    [content addSubview:seg];

    CGFloat rx = 230, rw = 380;

    NSTextField *titleLbl = [NSTextField labelWithString:@"Profile Settings"];
    [titleLbl setFont:[NSFont boldSystemFontOfSize:14]];
    [titleLbl setFrame:NSMakeRect(rx, 375, rw, 22)];
    [content addSubview:titleLbl];

    NSTextField *nameLbl = [NSTextField labelWithString:@"Profile Name:"];
    [nameLbl setFrame:NSMakeRect(rx, 345, 100, 18)];
    [content addSubview:nameLbl];
    _nameField = [[NSTextField alloc] initWithFrame:NSMakeRect(rx + 105, 343, 260, 22)];
    [content addSubview:_nameField];

    NSTextField *hostLbl = [NSTextField labelWithString:@"Server Host:"];
    [hostLbl setFrame:NSMakeRect(rx, 310, 100, 18)];
    [content addSubview:hostLbl];
    _hostField = [[NSTextField alloc] initWithFrame:NSMakeRect(rx + 105, 308, 260, 22)];
    [content addSubview:_hostField];

    NSTextField *portLbl = [NSTextField labelWithString:@"Port:"];
    [portLbl setFrame:NSMakeRect(rx, 275, 100, 18)];
    [content addSubview:portLbl];
    _portField = [[NSTextField alloc] initWithFrame:NSMakeRect(rx + 105, 273, 100, 22)];
    [content addSubview:_portField];

    NSTextField *modeLbl = [NSTextField labelWithString:@"Display Mode:"];
    [modeLbl setFrame:NSMakeRect(rx, 235, 100, 18)];
    [content addSubview:modeLbl];
    _modePopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(rx + 105, 232, 200, 25) pullsDown:NO];
    [_modePopup addItemsWithTitles:@[@"Real Plasma", @"Crisp", @"Split"]];
    [content addSubview:_modePopup];

    NSTextField *decayLbl = [NSTextField labelWithString:@"Persistence:"];
    [decayLbl setFrame:NSMakeRect(rx, 195, 100, 18)];
    [content addSubview:decayLbl];
    _decayPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(rx + 105, 192, 200, 25) pullsDown:NO];
    [_decayPopup addItemsWithTitles:@[@"100 ms (Authentic)", @"200 ms (Warm Glow)", @"500 ms (Medium)", @"1000 ms (Long/Radar)", @"2000 ms (Ultra Persistence)", @"5000 ms (5s Extreme / Storage Tube)"]];
    [content addSubview:_decayPopup];

    _fullscreenCheckbox = [NSButton checkboxWithTitle:@"Launch in Full Screen" target:nil action:nil];
    [_fullscreenCheckbox setFrame:NSMakeRect(rx + 105, 155, 240, 20)];
    [content addSubview:_fullscreenCheckbox];

    _defaultCheckbox = [NSButton checkboxWithTitle:@"Default profile at startup" target:nil action:nil];
    [_defaultCheckbox setFrame:NSMakeRect(rx + 105, 125, 240, 20)];
    [content addSubview:_defaultCheckbox];

    NSButton *saveBtn = [[NSButton alloc] initWithFrame:NSMakeRect(rx + 140, 20, 100, 28)];
    [saveBtn setTitle:@"Save"];
    [saveBtn setTarget:self];
    [saveBtn setAction:@selector(onSave:)];
    [content addSubview:saveBtn];

    NSButton *connBtn = [[NSButton alloc] initWithFrame:NSMakeRect(rx + 250, 20, 120, 28)];
    [connBtn setTitle:@"Connect Now"];
    [connBtn setKeyEquivalent:@"\r"];
    [connBtn setTarget:self];
    [connBtn setAction:@selector(onConnectNow:)];
    [content addSubview:connBtn];

    [_tableView reloadData];
    if ([_profiles count] > 0) {
        [_tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
        [self loadProfileToForm:_profiles[0]];
    }
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    return [self.profiles count];
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row {
    NSTextField *label = [NSTextField labelWithString:@""];
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
        [self loadProfileToForm:self.profiles[sel]];
    }
}

- (void)loadProfileToForm:(NSDictionary *)p {
    [self.nameField setStringValue:(p[@"name"] ? p[@"name"] : @"")];
    [self.hostField setStringValue:(p[@"host"] ? p[@"host"] : @"")];
    [self.portField setStringValue:[NSString stringWithFormat:@"%@", (p[@"port"] ? p[@"port"] : @8005)]];
    NSInteger mode = [p[@"displayMode"] integerValue];
    [self.modePopup selectItemAtIndex:(mode == 0 ? 1 : (mode == 2 ? 2 : 0))];

    NSInteger ms = [p[@"persistenceMs"] integerValue];
    NSInteger decayIdx = 0;
    if (ms == 200) decayIdx = 1;
    else if (ms == 500) decayIdx = 2;
    else if (ms == 1000) decayIdx = 3;
    else if (ms == 2000) decayIdx = 4;
    else if (ms == 5000) decayIdx = 5;
    [self.decayPopup selectItemAtIndex:decayIdx];

    [self.fullscreenCheckbox setState:[p[@"fullScreen"] boolValue] ? NSControlStateValueOn : NSControlStateValueOff];
    [self.defaultCheckbox setState:[p[@"isDefault"] boolValue] ? NSControlStateValueOn : NSControlStateValueOff];
}

- (void)saveCurrentFormToProfile {
    NSInteger sel = [self.tableView selectedRow];
    if (sel < 0 || sel >= (NSInteger)[self.profiles count]) return;
    NSMutableDictionary *p = self.profiles[sel];
    p[@"name"] = [self.nameField stringValue];
    p[@"host"] = [self.hostField stringValue];
    p[@"port"] = @([self.portField intValue]);

    NSInteger mIdx = [self.modePopup indexOfSelectedItem];
    p[@"displayMode"] = @(mIdx == 1 ? 0 : (mIdx == 2 ? 2 : 1));

    NSInteger dIdx = [self.decayPopup indexOfSelectedItem];
    NSInteger ms = 100;
    if (dIdx == 1) ms = 200;
    else if (dIdx == 2) ms = 500;
    else if (dIdx == 3) ms = 1000;
    else if (dIdx == 4) ms = 2000;
    else if (dIdx == 5) ms = 5000;
    p[@"persistenceMs"] = @(ms);

    p[@"fullScreen"] = @([self.fullscreenCheckbox state] == NSControlStateValueOn);
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
    NSInteger sel = [self.tableView selectedRow];
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
            @"fullScreen": @YES,
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

@interface PLATOAppDelegate () <PLATOProfilesDelegate>
@property (nonatomic, strong) NSMutableArray<PLATOTerminalWindowController *> *terminalControllers;
@property (nonatomic, strong) PLATOKeymapWindowController *keymapController;
@property (nonatomic, strong) PLATOProfilesWindowController *profilesController;
@property (nonatomic, strong) NSMenu *connectionSubmenu;
@property (nonatomic, strong) NSMenu *profileNewWindowSubmenu;
@property (nonatomic, strong) NSMenu *decayMenu;
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

    // Menu PlatoLives (App)
    NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
    NSMenu *appMenu = [[NSMenu alloc] initWithTitle:@"PlatoLives"];
    NSMenuItem *aboutItem = [[NSMenuItem alloc] initWithTitle:@"About PlatoLives" action:@selector(showAbout:) keyEquivalent:@""];
    [aboutItem setTarget:self];
    [appMenu addItem:aboutItem];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:@"Hide PlatoLives" action:@selector(hide:) keyEquivalent:@"h"];
    [appMenu addItemWithTitle:@"Quit PlatoLives" action:@selector(terminate:) keyEquivalent:@"q"];
    [appMenuItem setSubmenu:appMenu];
    [mainMenu addItem:appMenuItem];

    // Menu File
    NSMenuItem *fileMenuItem = [[NSMenuItem alloc] init];
    NSMenu *fileMenu = [[NSMenu alloc] initWithTitle:@"File"];

    NSMenuItem *newWinItem = [[NSMenuItem alloc] initWithTitle:@"New Window" action:@selector(newWindow:) keyEquivalent:@"n"];
    [newWinItem setTarget:self];
    [fileMenu addItem:newWinItem];

    NSMenuItem *newWinProfItem = [[NSMenuItem alloc] initWithTitle:@"New Window with Profile" action:nil keyEquivalent:@""];
    self.profileNewWindowSubmenu = [[NSMenu alloc] initWithTitle:@"New Window with Profile"];
    [newWinProfItem setSubmenu:self.profileNewWindowSubmenu];
    [fileMenu addItem:newWinProfItem];

    [fileMenu addItem:[NSMenuItem separatorItem]];
    NSMenuItem *closeWinItem = [[NSMenuItem alloc] initWithTitle:@"Close Window" action:@selector(performClose:) keyEquivalent:@"w"];
    [fileMenu addItem:closeWinItem];

    [fileMenuItem setSubmenu:fileMenu];
    [mainMenu addItem:fileMenuItem];

    // Menu Edit
    NSMenuItem *editMenuItem = [[NSMenuItem alloc] init];
    NSMenu *editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];

    NSMenuItem *copyItem = [[NSMenuItem alloc] initWithTitle:@"Copy Screen" action:@selector(copyScreen:) keyEquivalent:@"c"];
    [copyItem setTarget:self];
    [editMenu addItem:copyItem];

    NSMenuItem *pasteItem = [[NSMenuItem alloc] initWithTitle:@"Paste" action:@selector(pasteText:) keyEquivalent:@"v"];
    [pasteItem setTarget:self];
    [editMenu addItem:pasteItem];

    [editMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem *cancelPasteItem = [[NSMenuItem alloc] initWithTitle:@"Cancel Paste" action:@selector(cancelPaste:) keyEquivalent:@"."];
    [cancelPasteItem setTarget:self];
    [editMenu addItem:cancelPasteItem];

    [editMenuItem setSubmenu:editMenu];
    [mainMenu addItem:editMenuItem];

    // Menu Connection (Dinamico)
    NSMenuItem *connMenuItem = [[NSMenuItem alloc] init];
    self.connectionSubmenu = [[NSMenu alloc] initWithTitle:@"Connection"];
    [self.connectionSubmenu setDelegate:self];
    [connMenuItem setSubmenu:self.connectionSubmenu];
    [mainMenu addItem:connMenuItem];

    // Menu View
    NSMenuItem *viewMenuItem = [[NSMenuItem alloc] init];
    NSMenu *viewMenu = [[NSMenu alloc] initWithTitle:@"View"];
    NSMenuItem *crispItem = [[NSMenuItem alloc] initWithTitle:@"Crisp" action:@selector(setDisplayCrisp:) keyEquivalent:@""];
    NSMenuItem *plasmaItem = [[NSMenuItem alloc] initWithTitle:@"Real Plasma" action:@selector(setDisplayRealPlasma:) keyEquivalent:@""];
    NSMenuItem *splitItem = [[NSMenuItem alloc] initWithTitle:@"Split: Real Plasma | Crisp" action:@selector(setDisplaySplit:) keyEquivalent:@""];
    [crispItem setTarget:self]; [crispItem setTag:1001];
    [plasmaItem setTarget:self]; [plasmaItem setTag:1002]; [plasmaItem setState:NSControlStateValueOn];
    [splitItem setTarget:self]; [splitItem setTag:1003];
    [viewMenu addItem:crispItem]; [viewMenu addItem:plasmaItem]; [viewMenu addItem:splitItem];
    [viewMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem *decayItem = [[NSMenuItem alloc] initWithTitle:@"Plasma Persistence" action:nil keyEquivalent:@""];
    self.decayMenu = [[NSMenu alloc] initWithTitle:@"Plasma Persistence"];
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
    [viewMenuItem setSubmenu:viewMenu]; [mainMenu addItem:viewMenuItem];

    // Menu Tools
    NSMenuItem *toolsMenuItem = [[NSMenuItem alloc] init];
    NSMenu *toolsMenu = [[NSMenu alloc] initWithTitle:@"Tools"];
    NSMenuItem *logItem = [[NSMenuItem alloc] initWithTitle:@"Enable Diagnostic Log" action:@selector(toggleTrafficLog:) keyEquivalent:@""];
    [logItem setTarget:self]; [toolsMenu addItem:logItem];
    NSMenuItem *rendererLogItem = [[NSMenuItem alloc] initWithTitle:@"Enable Renderer Performance Log" action:@selector(toggleRendererPerformanceLog:) keyEquivalent:@""];
    [rendererLogItem setTarget:self]; [rendererLogItem setState:NSControlStateValueOff]; [toolsMenu addItem:rendererLogItem];
    [toolsMenuItem setSubmenu:toolsMenu]; [mainMenu addItem:toolsMenuItem];

    // Menu Window (standard macOS: Minimize Cmd+M, Zoom, window list)
    NSMenuItem *windowMenuItem = [[NSMenuItem alloc] init];
    NSMenu *windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
    [windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [windowMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
    [windowMenu addItem:[NSMenuItem separatorItem]];
    [windowMenu addItemWithTitle:@"Bring All to Front" action:@selector(arrangeInFront:) keyEquivalent:@""];
    [windowMenuItem setSubmenu:windowMenu]; [mainMenu addItem:windowMenuItem];
    [NSApp setWindowsMenu:windowMenu];

    [NSApp setMainMenu:mainMenu];
    [self rebuildConnectionMenu];
}

- (void)menuNeedsUpdate:(NSMenu *)menu {
    if (menu == self.connectionSubmenu) {
        [self rebuildConnectionMenu];
    } else if (menu == self.statusMenu) {
        [self rebuildStatusMenu];
    }
}

- (void)rebuildConnectionMenu {
    if (!self.connectionSubmenu) return;
    [self.connectionSubmenu removeAllItems];
    if (self.profileNewWindowSubmenu) [self.profileNewWindowSubmenu removeAllItems];

    NSMenuItem *defConnItem = [[NSMenuItem alloc] initWithTitle:@"Connect to Default Profile" action:@selector(connectDefaultProfile:) keyEquivalent:@"r"];
    [defConnItem setTarget:self];
    [self.connectionSubmenu addItem:defConnItem];

    NSMenuItem *manageProfilesItem = [[NSMenuItem alloc] initWithTitle:@"Connection Profiles..." action:@selector(showProfilesWindow:) keyEquivalent:@","];
    [manageProfilesItem setTarget:self];
    [self.connectionSubmenu addItem:manageProfilesItem];

    [self.connectionSubmenu addItem:[NSMenuItem separatorItem]];

    NSArray *profiles = [PLATOProfileManager loadProfiles];
    for (NSDictionary *p in profiles) {
        NSString *name = p[@"name"] ? p[@"name"] : @"Untitled";
        NSString *title = [NSString stringWithFormat:@"Connect to %@", name];
        if ([p[@"isDefault"] boolValue]) {
            title = [title stringByAppendingString:@" ★"];
        }

        NSMenuItem *pItem = [[NSMenuItem alloc] initWithTitle:title action:@selector(quickConnectProfile:) keyEquivalent:@""];
        [pItem setRepresentedObject:p];
        [pItem setTarget:self];
        [self.connectionSubmenu addItem:pItem];

        if (self.profileNewWindowSubmenu) {
            NSMenuItem *newPItem = [[NSMenuItem alloc] initWithTitle:name action:@selector(newWindowWithProfileItem:) keyEquivalent:@""];
            [newPItem setRepresentedObject:p];
            [newPItem setTarget:self];
            [self.profileNewWindowSubmenu addItem:newPItem];
        }
    }

    [self.connectionSubmenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem *discItem = [[NSMenuItem alloc] initWithTitle:@"Disconnect" action:@selector(disconnectSession:) keyEquivalent:@"d"];
    [discItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagShift];
    [discItem setTarget:self];
    [self.connectionSubmenu addItem:discItem];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    [self setupMenuBar];
    [self setupDockIcon];
    [self setupStatusItem];

    self.keymapController = [[PLATOKeymapWindowController alloc] init];
    __weak PLATOAppDelegate *weakSelf = self;
    self.keymapController.closeHandler = ^{ weakSelf.keyboardReferenceEnabled = NO; [weakSelf applyKeyboardReferencePresentation]; };
    self.keyboardReferenceEnabled = NO;

    self.terminalControllers = [NSMutableArray array];

    NSDictionary *defProf = [PLATOProfileManager defaultProfile];
    [self openNewWindowWithProfile:defProf];

    NSArray<NSString *> *arguments = [[NSProcessInfo processInfo] arguments];
    NSUInteger testIndex = [arguments indexOfObject:@"--test-script"];
    if (testIndex != NSNotFound) {
        if (testIndex + 1 >= [arguments count]) {
            NSLog(@"[TEST] ERROR: --test-script requires a file path");
            [NSApp terminate:nil];
        } else {
            self.testRunner = [[PLATOTestRunner alloc] initWithView:self.view scriptPath:arguments[testIndex + 1]];
            [self.testRunner start];
        }
    }
    [NSApp activateIgnoringOtherApps:YES];
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

    NSMenuItem *headerItem = [[NSMenuItem alloc] initWithTitle:@"PlatoLives 3.2" action:nil keyEquivalent:@""];
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
    NSInteger targetTag = (mode == 0 ? 1001 : (mode == 2 ? 1003 : 1002));
    NSMenu *mainMenu = [NSApp mainMenu];
    for (NSInteger tag = 1001; tag <= 1003; tag++) {
        [[mainMenu itemWithTag:tag] setState:(tag == targetTag ? NSControlStateValueOn : NSControlStateValueOff)];
    }
}

- (void)selectDisplayMenuItem:(NSMenuItem *)selectedItem {
    if (!selectedItem) return;
    NSInteger m = ([selectedItem tag] == 1001 ? 0 : ([selectedItem tag] == 1003 ? 2 : 1));
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

- (void)setDisplayCrisp:(id)sender {
    [self.view setDisplayCrisp];
    [self selectDisplayMenuItem:(NSMenuItem *)sender];
}

- (void)setDisplayRealPlasma:(id)sender {
    [self.view setDisplayRealPlasma];
    [self selectDisplayMenuItem:(NSMenuItem *)sender];
}

- (void)setDisplaySplit:(id)sender {
    [self.view setDisplaySplit];
    [self selectDisplayMenuItem:(NSMenuItem *)sender];
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

- (void)copyScreen:(id)sender {
    [self.view copyScreenToPasteboard];
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
}

- (void)profilesControllerDidUpdateProfiles:(PLATOProfilesWindowController *)controller {
    [self rebuildConnectionMenu];
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
    if (!version) version = @"3.2";
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

@end
