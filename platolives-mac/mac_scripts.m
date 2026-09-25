#import "mac_scripts.h"

@interface PLATOScriptsWindowController ()
@property (nonatomic, strong) NSTableView *tableView;
@property (nonatomic, strong) NSTextField *nameField;
@property (nonatomic, strong) NSButton *enabledCheckbox;
@property (nonatomic, strong) NSTextField *charDelayField;
@property (nonatomic, strong) NSTextField *nextDelayField;
@property (nonatomic, strong) NSTextField *cmdDelayField;
@property (nonatomic, strong) NSTextField *hotkeyLabel;
@property (nonatomic, strong) NSButton *recordBtn;
@property (nonatomic, strong) NSButton *clearKeyBtn;
@property (nonatomic, strong) NSTextView *bodyTextView;
@property (nonatomic) NSInteger currentEditingIndex;
@property (nonatomic) BOOL isRecordingHotkey;
@property (nonatomic) BOOL isUpdatingUI;
@end

@implementation PLATOScriptsWindowController

- (instancetype)initWithScriptList:(plato_script_list_t *)list {
    NSRect frame = NSMakeRect(0, 0, 920, 620);
    NSWindow *win = [[NSWindow alloc] initWithContentRect:frame
                                                styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable)
                                                  backing:NSBackingStoreBuffered defer:NO];
    [win setTitle:@"Manage Scripts"];
    [win setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]];
    [win setBackgroundColor:[NSColor colorWithCalibratedRed:16.0/255.0 green:12.0/255.0 blue:10.0/255.0 alpha:1.0]];
    [win center];

    self = [super initWithWindow:win];
    if (self) {
        _scriptList = list;
        _currentEditingIndex = -1;
        _isRecordingHotkey = NO;
        _isUpdatingUI = NO;
        [self setupUI];
    }
    return self;
}

- (void)setupUI {
    NSView *content = [self.window contentView];

    // Colori tema Plasma
    NSColor *plasmaOrange = [NSColor colorWithCalibratedRed:255.0/255.0 green:110.0/255.0 blue:0.0/255.0 alpha:1.0];
    NSColor *boxBg = [NSColor colorWithCalibratedRed:24.0/255.0 green:18.0/255.0 blue:14.0/255.0 alpha:1.0];

    // Tabella lista script a sinistra
    NSScrollView *tableScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 58, 220, 542)];
    [tableScroll setHasVerticalScroller:YES];
    [tableScroll setBorderType:NSBezelBorder];
    [tableScroll setDrawsBackground:YES];
    [tableScroll setBackgroundColor:boxBg];

    _tableView = [[NSTableView alloc] initWithFrame:[tableScroll bounds]];
    NSTableColumn *col = [[NSTableColumn alloc] initWithIdentifier:@"name"];
    [col setTitle:@"Scripts"];
    [col setWidth:200];
    [_tableView addTableColumn:col];
    [_tableView setHeaderView:nil];
    [_tableView setDataSource:self];
    [_tableView setDelegate:self];
    [_tableView setBackgroundColor:boxBg];
    [tableScroll setDocumentView:_tableView];
    [content addSubview:tableScroll];

    // Pulsanti +, Clone, - sotto la lista
    NSButton *addBtn = [[NSButton alloc] initWithFrame:NSMakeRect(20, 20, 65, 26)];
    [addBtn setBezelStyle:NSBezelStyleRounded];
    [addBtn setTitle:@"+ New"];
    [addBtn setTarget:self];
    [addBtn setAction:@selector(onNewScript:)];
    [content addSubview:addBtn];

    NSButton *cloneBtn = [[NSButton alloc] initWithFrame:NSMakeRect(90, 20, 75, 26)];
    [cloneBtn setBezelStyle:NSBezelStyleRounded];
    [cloneBtn setTitle:@"Clone"];
    [cloneBtn setTarget:self];
    [cloneBtn setAction:@selector(onCloneScript:)];
    [content addSubview:cloneBtn];

    NSButton *delBtn = [[NSButton alloc] initWithFrame:NSMakeRect(170, 20, 70, 26)];
    [delBtn setBezelStyle:NSBezelStyleRounded];
    [delBtn setTitle:@"Delete"];
    [delBtn setTarget:self];
    [delBtn setAction:@selector(onDeleteScript:)];
    [content addSubview:delBtn];

    // Layout colonna destra
    CGFloat rx = 260, rw = 640;
    CGFloat fx = rx + 110, fw = rw - 110;

    // Titolo form
    NSTextField *titleLbl = [NSTextField labelWithString:@"Script Configuration"];
    [titleLbl setFont:[NSFont boldSystemFontOfSize:14]];
    [titleLbl setTextColor:plasmaOrange];
    [titleLbl setFrame:NSMakeRect(rx, 578, rw, 22)];
    [content addSubview:titleLbl];

    // Script Name
    NSTextField *nameLbl = [NSTextField labelWithString:@"Script Name:"];
    [nameLbl setTextColor:plasmaOrange];
    [nameLbl setFrame:NSMakeRect(rx, 545, 100, 18)];
    [content addSubview:nameLbl];

    _nameField = [[NSTextField alloc] initWithFrame:NSMakeRect(fx, 543, fw - 110, 22)];
    [content addSubview:_nameField];

    _enabledCheckbox = [NSButton checkboxWithTitle:@"Enabled" target:nil action:nil];
    [_enabledCheckbox setFrame:NSMakeRect(fx + fw - 95, 543, 95, 22)];
    [content addSubview:_enabledCheckbox];

    // Hotkey
    NSTextField *hkTitleLbl = [NSTextField labelWithString:@"Hotkey:"];
    [hkTitleLbl setTextColor:plasmaOrange];
    [hkTitleLbl setFrame:NSMakeRect(rx, 510, 100, 18)];
    [content addSubview:hkTitleLbl];

    _hotkeyLabel = [NSTextField labelWithString:@"None"];
    [_hotkeyLabel setFont:[NSFont boldSystemFontOfSize:13]];
    [_hotkeyLabel setTextColor:[NSColor whiteColor]];
    [_hotkeyLabel setFrame:NSMakeRect(fx, 510, 200, 18)];
    [content addSubview:_hotkeyLabel];

    _recordBtn = [[NSButton alloc] initWithFrame:NSMakeRect(fx + 210, 506, 120, 26)];
    [_recordBtn setBezelStyle:NSBezelStyleRounded];
    [_recordBtn setTitle:@"Record Hotkey"];
    [_recordBtn setTarget:self];
    [_recordBtn setAction:@selector(onRecordHotkey:)];
    [content addSubview:_recordBtn];

    _clearKeyBtn = [[NSButton alloc] initWithFrame:NSMakeRect(fx + 335, 506, 75, 26)];
    [_clearKeyBtn setBezelStyle:NSBezelStyleRounded];
    [_clearKeyBtn setTitle:@"Clear"];
    [_clearKeyBtn setTarget:self];
    [_clearKeyBtn setAction:@selector(onClearHotkey:)];
    [content addSubview:_clearKeyBtn];

    // Delays
    NSTextField *delaysLbl = [NSTextField labelWithString:@"Delays (ms):"];
    [delaysLbl setTextColor:plasmaOrange];
    [delaysLbl setFrame:NSMakeRect(rx, 475, 100, 18)];
    [content addSubview:delaysLbl];

    NSTextField *cLbl = [NSTextField labelWithString:@"Char:"];
    [cLbl setTextColor:[NSColor lightGrayColor]];
    [cLbl setFrame:NSMakeRect(fx, 475, 45, 18)];
    [content addSubview:cLbl];
    _charDelayField = [[NSTextField alloc] initWithFrame:NSMakeRect(fx + 45, 473, 65, 22)];
    [content addSubview:_charDelayField];

    NSTextField *nLbl = [NSTextField labelWithString:@"Next:"];
    [nLbl setTextColor:[NSColor lightGrayColor]];
    [nLbl setFrame:NSMakeRect(fx + 125, 475, 45, 18)];
    [content addSubview:nLbl];
    _nextDelayField = [[NSTextField alloc] initWithFrame:NSMakeRect(fx + 170, 473, 65, 22)];
    [content addSubview:_nextDelayField];

    NSTextField *cmLbl = [NSTextField labelWithString:@"Cmd:"];
    [cmLbl setTextColor:[NSColor lightGrayColor]];
    [cmLbl setFrame:NSMakeRect(fx + 250, 475, 45, 18)];
    [content addSubview:cmLbl];
    _cmdDelayField = [[NSTextField alloc] initWithFrame:NSMakeRect(fx + 295, 473, 65, 22)];
    [content addSubview:_cmdDelayField];

    // Body Editor
    NSTextField *bodyLbl = [NSTextField labelWithString:@"Script Commands:"];
    [bodyLbl setTextColor:plasmaOrange];
    [bodyLbl setFrame:NSMakeRect(rx, 442, rw, 18)];
    [content addSubview:bodyLbl];

    NSScrollView *bodyScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(rx, 58, rw, 380)];
    [bodyScroll setHasVerticalScroller:YES];
    [bodyScroll setBorderType:NSBezelBorder];

    _bodyTextView = [[NSTextView alloc] initWithFrame:[bodyScroll bounds]];
    [_bodyTextView setFont:[NSFont monospacedSystemFontOfSize:12 weight:NSFontWeightRegular]];
    [_bodyTextView setBackgroundColor:boxBg];
    [_bodyTextView setTextColor:[NSColor colorWithCalibratedRed:255.0/255.0 green:180.0/255.0 blue:60.0/255.0 alpha:1.0]];
    [_bodyTextView setInsertionPointColor:plasmaOrange];
    [_bodyTextView setEditable:YES];
    [_bodyTextView setSelectable:YES];
    [_bodyTextView setRichText:NO];
    [_bodyTextView setAllowsUndo:YES];
    [_bodyTextView setAutomaticQuoteSubstitutionEnabled:NO];
    [_bodyTextView setAutomaticDashSubstitutionEnabled:NO];
    [_bodyTextView setAutomaticSpellingCorrectionEnabled:NO];
    [bodyScroll setDocumentView:_bodyTextView];
    [content addSubview:bodyScroll];

    // Pulsanti in basso a destra: Close e Save
    NSButton *closeBtn = [[NSButton alloc] initWithFrame:NSMakeRect(rx + rw - 225, 18, 105, 30)];
    [closeBtn setBezelStyle:NSBezelStyleRounded];
    [closeBtn setTitle:@"Close"];
    [closeBtn setTarget:self];
    [closeBtn setAction:@selector(onCloseWindow:)];
    [content addSubview:closeBtn];

    NSButton *saveBtn = [[NSButton alloc] initWithFrame:NSMakeRect(rx + rw - 110, 18, 110, 30)];
    [saveBtn setBezelStyle:NSBezelStyleRounded];
    [saveBtn setTitle:@"Save"];
    [saveBtn setKeyEquivalent:[NSString stringWithFormat:@"%c", 13]];
    [saveBtn setTarget:self];
    [saveBtn setAction:@selector(onSave:)];
    [content addSubview:saveBtn];

    [self refreshList];
    if (self.scriptList && self.scriptList->count > 0) {
        [self loadScriptAtIndex:0];
    }
}

- (void)refreshList {
    _isUpdatingUI = YES;
    [_tableView reloadData];
    _isUpdatingUI = NO;
}

- (void)loadScriptAtIndex:(NSInteger)row {
    if (!self.scriptList || row < 0 || row >= (NSInteger)self.scriptList->count) {
        _currentEditingIndex = -1;
        [_nameField setStringValue:@""];
        [_enabledCheckbox setState:NSControlStateValueOff];
        [_hotkeyLabel setStringValue:@"None"];
        [_charDelayField setStringValue:@""];
        [_nextDelayField setStringValue:@""];
        [_cmdDelayField setStringValue:@""];
        [_bodyTextView setString:@""];
        return;
    }

    _isUpdatingUI = YES;
    _currentEditingIndex = row;
    [_tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:row] byExtendingSelection:NO];

    plato_script_t *s = &self.scriptList->scripts[row];
    [_nameField setStringValue:[NSString stringWithUTF8String:s->name]];
    [_enabledCheckbox setState:s->enabled ? NSControlStateValueOn : NSControlStateValueOff];

    NSString *hkDisp = (strlen(s->hotkey_display) > 0) ? [NSString stringWithUTF8String:s->hotkey_display] : @"None";
    [_hotkeyLabel setStringValue:hkDisp];

    [_charDelayField setStringValue:[NSString stringWithFormat:@"%d", s->char_delay_ms]];
    [_nextDelayField setStringValue:[NSString stringWithFormat:@"%d", s->next_delay_ms]];
    [_cmdDelayField setStringValue:[NSString stringWithFormat:@"%d", s->command_delay_ms]];

    NSString *bodyStr = [NSString stringWithUTF8String:s->body];
    [_bodyTextView setString:bodyStr ? bodyStr : @""];

    _isUpdatingUI = NO;
}

- (void)saveCurrentFormToStruct {
    if (!self.scriptList || _currentEditingIndex < 0 || _currentEditingIndex >= (NSInteger)self.scriptList->count) {
        return;
    }

    plato_script_t *s = &self.scriptList->scripts[_currentEditingIndex];

    const char *nameUtf8 = [[_nameField stringValue] UTF8String];
    snprintf(s->name, sizeof(s->name), "%s", nameUtf8 ? nameUtf8 : "Untitled");

    s->enabled = ([_enabledCheckbox state] == NSControlStateValueOn);
    s->char_delay_ms = [_charDelayField intValue];
    s->next_delay_ms = [_nextDelayField intValue];
    s->command_delay_ms = [_cmdDelayField intValue];

    // Sanitizzazione corpo script (rimozione \r)
    NSString *rawBody = [_bodyTextView string];
    const char *bodyUtf8 = [rawBody UTF8String];
    if (bodyUtf8) {
        size_t o = 0;
        for (size_t i = 0; bodyUtf8[i] && o + 1 < sizeof(s->body); i++) {
            if (bodyUtf8[i] == '\r') continue;
            s->body[o++] = bodyUtf8[i];
        }
        s->body[o] = '\0';
    } else {
        s->body[0] = '\0';
    }

    // Persistenza atomica C11
    plato_scripts_save(self.scriptList, NULL);
}

- (void)onSave:(id)sender {
    [self saveCurrentFormToStruct];
    [self refreshList];
    if ([self.delegate respondsToSelector:@selector(scriptsControllerDidUpdateScripts:)]) {
        [self.delegate scriptsControllerDidUpdateScripts:self];
    }
}

- (void)onCloseWindow:(id)sender {
    [self saveCurrentFormToStruct];
    [self.window close];
}

- (void)onNewScript:(id)sender {
    if (!self.scriptList) return;
    [self saveCurrentFormToStruct];

    plato_script_t ns = {0};
    snprintf(ns.name, sizeof(ns.name), "New Script");
    ns.enabled = true;
    ns.char_delay_ms = 20;
    ns.next_delay_ms = 250;
    ns.command_delay_ms = 0;
    snprintf(ns.body, sizeof(ns.body), "%s", PLATO_DEFAULT_AUTOLOGIN_SCRIPT);

    int newIdx = plato_scripts_add(self.scriptList, &ns);
    plato_scripts_save(self.scriptList, NULL);

    [self refreshList];
    if (newIdx >= 0) {
        [self loadScriptAtIndex:newIdx];
    }
    if ([self.delegate respondsToSelector:@selector(scriptsControllerDidUpdateScripts:)]) {
        [self.delegate scriptsControllerDidUpdateScripts:self];
    }
}

- (void)onCloneScript:(id)sender {
    if (!self.scriptList || _currentEditingIndex < 0 || _currentEditingIndex >= (NSInteger)self.scriptList->count) {
        return;
    }
    [self saveCurrentFormToStruct];

    int cloneIdx = plato_scripts_clone(self.scriptList, (size_t)_currentEditingIndex);
    plato_scripts_save(self.scriptList, NULL);

    [self refreshList];
    if (cloneIdx >= 0) {
        [self loadScriptAtIndex:cloneIdx];
    }
    if ([self.delegate respondsToSelector:@selector(scriptsControllerDidUpdateScripts:)]) {
        [self.delegate scriptsControllerDidUpdateScripts:self];
    }
}

- (void)onDeleteScript:(id)sender {
    if (!self.scriptList || _currentEditingIndex < 0 || _currentEditingIndex >= (NSInteger)self.scriptList->count) {
        return;
    }
    if (self.scriptList->count <= 1) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Cannot Delete Script"];
        [alert setInformativeText:@"At least one script must remain in the configuration."];
        [alert runModal];
        return;
    }

    NSInteger delIdx = _currentEditingIndex;
    plato_scripts_delete(self.scriptList, (size_t)delIdx);
    plato_scripts_save(self.scriptList, NULL);

    [self refreshList];
    NSInteger nextIdx = delIdx > 0 ? delIdx - 1 : 0;
    [self loadScriptAtIndex:nextIdx];

    if ([self.delegate respondsToSelector:@selector(scriptsControllerDidUpdateScripts:)]) {
        [self.delegate scriptsControllerDidUpdateScripts:self];
    }
}

- (void)onClearHotkey:(id)sender {
    if (!self.scriptList || _currentEditingIndex < 0 || _currentEditingIndex >= (NSInteger)self.scriptList->count) {
        return;
    }
    plato_script_t *s = &self.scriptList->scripts[_currentEditingIndex];
    s->hotkey_modifiers = 0;
    s->hotkey_key = 0;
    memset(s->hotkey_display, 0, sizeof(s->hotkey_display));
    [_hotkeyLabel setStringValue:@"None"];

    [self saveCurrentFormToStruct];
    if ([self.delegate respondsToSelector:@selector(scriptsControllerDidUpdateScripts:)]) {
        [self.delegate scriptsControllerDidUpdateScripts:self];
    }
}

- (void)onRecordHotkey:(id)sender {
    _isRecordingHotkey = YES;
    [_hotkeyLabel setStringValue:@"Press shortcut (or Esc)..."];
    [self.window makeFirstResponder:self.window];
}

- (void)keyDown:(NSEvent *)event {
    if (_isRecordingHotkey && _currentEditingIndex >= 0) {
        NSEventModifierFlags flags = [event modifierFlags];
        NSString *chars = [event charactersIgnoringModifiers];
        if ([chars length] == 0) return;

        unichar c = [chars characterAtIndex:0];

        // Esc annulla la registrazione
        if (c == 27) {
            _isRecordingHotkey = NO;
            plato_script_t *s = &self.scriptList->scripts[_currentEditingIndex];
            NSString *hkDisp = (strlen(s->hotkey_display) > 0) ? [NSString stringWithUTF8String:s->hotkey_display] : @"None";
            [_hotkeyLabel setStringValue:hkDisp];
            return;
        }

        uint32_t mods = 0;
        if (flags & NSEventModifierFlagControl) mods |= PLATO_HOTKEY_MOD_CTRL;
        if (flags & NSEventModifierFlagOption)  mods |= PLATO_HOTKEY_MOD_ALT;
        if (flags & NSEventModifierFlagShift)   mods |= PLATO_HOTKEY_MOD_SHIFT;

        unichar upc = [[chars uppercaseString] characterAtIndex:0];
        BOOL isFKey = (c >= NSF1FunctionKey && c <= NSF12FunctionKey);
        BOOL isAlphaNum = ((upc >= 'A' && upc <= 'Z') || (upc >= '0' && upc <= '9'));

        if (isAlphaNum || isFKey) {
            // Controllo scorciatoia riservata di sistema (Ctrl+Shift+X)
            if (mods == (PLATO_HOTKEY_MOD_CTRL | PLATO_HOTKEY_MOD_SHIFT) && upc == 'X') {
                NSAlert *alert = [[NSAlert alloc] init];
                [alert setMessageText:@"Reserved Hotkey"];
                [alert setInformativeText:@"Ctrl+Shift+X is reserved to Cancel Script Execution."];
                [alert runModal];
                _isRecordingHotkey = NO;
                return;
            }

            plato_script_t *s = &self.scriptList->scripts[_currentEditingIndex];
            s->hotkey_modifiers = mods;
            s->hotkey_key = isFKey ? (uint32_t)c : (uint32_t)upc;

            NSString *disp = @"";
            if (mods & PLATO_HOTKEY_MOD_CTRL) disp = [disp stringByAppendingString:@"Ctrl+"];
            if (mods & PLATO_HOTKEY_MOD_ALT)  disp = [disp stringByAppendingString:@"Alt+"];
            if (mods & PLATO_HOTKEY_MOD_SHIFT) disp = [disp stringByAppendingString:@"Shift+"];

            if (isFKey) {
                disp = [disp stringByAppendingFormat:@"F%d", c - NSF1FunctionKey + 1];
            } else {
                disp = [disp stringByAppendingFormat:@"%c", upc];
            }

            snprintf(s->hotkey_display, sizeof(s->hotkey_display), "%s", [disp UTF8String]);
            [_hotkeyLabel setStringValue:disp];

            _isRecordingHotkey = NO;
            [self saveCurrentFormToStruct];
            if ([self.delegate respondsToSelector:@selector(scriptsControllerDidUpdateScripts:)]) {
                [self.delegate scriptsControllerDidUpdateScripts:self];
            }
            return;
        }
        return;
    }
    [super keyDown:event];
}

// NSTableView Data Source & Delegate
- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    return self.scriptList ? (NSInteger)self.scriptList->count : 0;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row {
    NSTextField *label = [NSTextField labelWithString:@""];
    [label setFont:[NSFont systemFontOfSize:12]];
    if (self.scriptList && row < (NSInteger)self.scriptList->count) {
        plato_script_t *s = &self.scriptList->scripts[row];
        NSString *title = [NSString stringWithUTF8String:s->name];
        if (!s->enabled) {
            title = [NSString stringWithFormat:@"[Disabled] %@", title];
            [label setTextColor:[NSColor grayColor]];
        } else {
            [label setTextColor:[NSColor colorWithCalibratedRed:255.0/255.0 green:160.0/255.0 blue:40.0/255.0 alpha:1.0]];
        }
        [label setStringValue:title];
    }
    return label;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification {
    if (_isUpdatingUI) return;
    [self saveCurrentFormToStruct];
    NSInteger sel = [_tableView selectedRow];
    if (sel >= 0 && sel < (NSInteger)self.scriptList->count) {
        [self loadScriptAtIndex:sel];
    }
}

@end
