#import "mac_keymap_window.h"
#import "mac_window.h"

@interface PLATOKeyboardProfilesWindowController ()
@property (nonatomic, strong) NSTableView *profilesTable;
@property (nonatomic, strong) NSTableView *bindingsTable;
@property (nonatomic, strong) NSScrollView *profScroll;
@property (nonatomic, strong) NSScrollView *bindScroll;
@property (nonatomic, strong) NSTextField *profHeader;

@property (nonatomic, strong) NSTextField *profileTitleLabel;
@property (nonatomic, strong) NSTextField *profileDescLabel;
@property (nonatomic, strong) NSTextField *statusInfoLabel;

@property (nonatomic, strong) NSButton *addProfileBtn;
@property (nonatomic, strong) NSButton *duplicateBtn;
@property (nonatomic, strong) NSButton *renameBtn;
@property (nonatomic, strong) NSButton *deleteBtn;

@property (nonatomic, strong) NSButton *recordKeyBtn;
@property (nonatomic, strong) NSButton *editBindingBtn;
@property (nonatomic, strong) NSButton *removeBindingBtn;
@property (nonatomic, strong) NSButton *setActiveBtn;

@property (nonatomic) NSInteger selectedProfileIndex;
@property (nonatomic) BOOL isRecordingKey;
@end

static NSString *formatPhysicalKey(const plato_key_binding_t *b) {
    if (!b) return @"";
    NSString *mods = @"";
    if (b->modifiers & PLATO_KEY_MOD_CTRL)  mods = [mods stringByAppendingString:@"Ctrl + "];
    if (b->modifiers & PLATO_KEY_MOD_ALT)   mods = [mods stringByAppendingString:@"Alt + "];
    if (b->modifiers & PLATO_KEY_MOD_SHIFT) mods = [mods stringByAppendingString:@"Shift + "];
    if (b->modifiers & PLATO_KEY_MOD_GUI)   mods = [mods stringByAppendingString:@"Cmd + "];

    NSString *key = @"";
    if (b->vkey != 0) {
        switch (b->vkey) {
            case PLATO_VKEY_UP:        key = @"Up Arrow"; break;
            case PLATO_VKEY_DOWN:      key = @"Down Arrow"; break;
            case PLATO_VKEY_LEFT:      key = @"Left Arrow"; break;
            case PLATO_VKEY_RIGHT:     key = @"Right Arrow"; break;
            case PLATO_VKEY_PAGEUP:    key = @"Page Up"; break;
            case PLATO_VKEY_PAGEDOWN:  key = @"Page Down"; break;
            case PLATO_VKEY_HOME:      key = @"Home"; break;
            case PLATO_VKEY_END:       key = @"End"; break;
            case PLATO_VKEY_RETURN:    key = @"Return / Enter"; break;
            case PLATO_VKEY_BACKSPACE: key = @"Backspace"; break;
            case PLATO_VKEY_ESCAPE:    key = @"Escape"; break;
            case PLATO_VKEY_TAB:       key = @"Tab"; break;
            case PLATO_VKEY_F1:        key = @"F1"; break;
            case PLATO_VKEY_F2:        key = @"F2"; break;
            case PLATO_VKEY_F3:        key = @"F3"; break;
            case PLATO_VKEY_F4:        key = @"F4"; break;
            case PLATO_VKEY_F5:        key = @"F5"; break;
            case PLATO_VKEY_F6:        key = @"F6"; break;
            case PLATO_VKEY_F7:        key = @"F7"; break;
            case PLATO_VKEY_F8:        key = @"F8"; break;
            case PLATO_VKEY_F9:        key = @"F9"; break;
            case PLATO_VKEY_F10:       key = @"F10"; break;
            case PLATO_VKEY_F11:       key = @"F11"; break;
            case PLATO_VKEY_F12:       key = @"F12"; break;
            case PLATO_VKEY_NUMPAD0:   key = @"Numpad 0"; break;
            case PLATO_VKEY_NUMPAD1:   key = @"Numpad 1"; break;
            case PLATO_VKEY_NUMPAD2:   key = @"Numpad 2"; break;
            case PLATO_VKEY_NUMPAD3:   key = @"Numpad 3"; break;
            case PLATO_VKEY_NUMPAD4:   key = @"Numpad 4"; break;
            case PLATO_VKEY_NUMPAD5:   key = @"Numpad 5"; break;
            case PLATO_VKEY_NUMPAD6:   key = @"Numpad 6"; break;
            case PLATO_VKEY_NUMPAD7:   key = @"Numpad 7"; break;
            case PLATO_VKEY_NUMPAD8:   key = @"Numpad 8"; break;
            case PLATO_VKEY_NUMPAD9:   key = @"Numpad 9"; break;
            default:                   key = [NSString stringWithFormat:@"VKey 0x%02X", b->vkey]; break;
        }
    } else if (b->codepoint != 0) {
        if (b->codepoint == ' ') key = @"Space";
        else key = [NSString stringWithFormat:@"'%c'", (char)b->codepoint];
    } else {
        key = @"(None)";
    }
    return [mods stringByAppendingString:key];
}

static NSString *formatTargetAction(const plato_key_binding_t *b) {
    if (!b) return @"";
    if (b->type == PLATO_BIND_ACTION_RAW_BYTE) {
        char ch = (char)b->target.raw_byte;
        if (ch >= 32 && ch <= 126) return [NSString stringWithFormat:@"Type '%c'", ch];
        if (ch == 9) return @"Type Tab (0x09)";
        if (ch == 10 || ch == 13) return @"Type Return (0x0D)";
        if (ch == 27) return @"Type Esc (0x1B)";
        return [NSString stringWithFormat:@"Raw Byte 0x%02X", b->target.raw_byte];
    } else if (b->type == PLATO_BIND_ACTION_PLATO_KEY) {
        switch ((plato_special_key_t)b->target.plato_key) {
            case PLATO_KEY_NEXT:   return @"PLATO: NEXT";
            case PLATO_KEY_BACK:   return @"PLATO: BACK";
            case PLATO_KEY_HELP:   return @"PLATO: HELP";
            case PLATO_KEY_LAB:    return @"PLATO: LAB";
            case PLATO_KEY_DATA:   return @"PLATO: DATA";
            case PLATO_KEY_STOP:   return @"PLATO: STOP";
            case PLATO_KEY_EDIT:   return @"PLATO: EDIT";
            case PLATO_KEY_COPY:   return @"PLATO: COPY";
            case PLATO_KEY_MICRO:  return @"PLATO: MICRO";
            case PLATO_KEY_FONT:   return @"PLATO: FONT";
            case PLATO_KEY_SUPER:  return @"PLATO: SUPER (Up)";
            case PLATO_KEY_SUB:    return @"PLATO: SUB (Down)";
            case PLATO_KEY_ACCESS: return @"PLATO: ACCESS / TAB";
            case PLATO_KEY_ERASE:  return @"PLATO: ERASE (Left)";
            case PLATO_KEY_ANS:    return @"PLATO: ANS";
            case PLATO_KEY_TERM:   return @"PLATO: TERM";
            case PLATO_KEY_SQUARE: return @"PLATO: SQUARE";
            default:               return [NSString stringWithFormat:@"PLATO Key %u", b->target.plato_key];
        }
    } else if (b->type == PLATO_BIND_ACTION_PLATO_SEQ) {
        return [NSString stringWithFormat:@"PLATO Sequence 0x%08X", b->target.sequence];
    }
    return @"(None)";
}

@implementation PLATOKeyboardProfilesWindowController

- (instancetype)initWithKeymapManager:(plato_keymap_manager_t *)mgr {
    NSRect frame = NSMakeRect(0, 0, 960, 620);
    NSWindow *win = [[NSWindow alloc] initWithContentRect:frame
                                                styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable)
                                                  backing:NSBackingStoreBuffered defer:NO];
    [win setTitle:@"Manage Keyboard Profiles"];
    [win setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]];
    [win setBackgroundColor:[NSColor colorWithCalibratedRed:16.0/255.0 green:12.0/255.0 blue:10.0/255.0 alpha:1.0]];
    [win center];

    self = [super initWithWindow:win];
    if (self) {
        _keymapManager = mgr;
        _selectedProfileIndex = mgr ? mgr->active_profile_index : 0;
        _isRecordingKey = NO;
        [self setupUI];
    }
    return self;
}

- (void)setupUI {
    NSView *content = [self.window contentView];
    NSColor *plasmaOrange = [NSColor colorWithCalibratedRed:255.0/255.0 green:110.0/255.0 blue:0.0/255.0 alpha:1.0];
    NSColor *boxBg = [NSColor colorWithCalibratedRed:24.0/255.0 green:18.0/255.0 blue:14.0/255.0 alpha:1.0];

    // =========================================================
    // PANNELLO SINISTRO: LISTA PROFILI
    // =========================================================
    NSTextField *profHeader = [NSTextField labelWithString:@"PROFILES"];
    [profHeader setFont:[NSFont boldSystemFontOfSize:12]];
    [profHeader setTextColor:plasmaOrange];
    [profHeader setFrame:NSMakeRect(20, 580, 240, 20)];
    [content addSubview:profHeader];

    _profHeader = profHeader;
    NSScrollView *profScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 70, 250, 500)];
    _profScroll = profScroll;
    [profScroll setHasVerticalScroller:YES];
    [profScroll setBorderType:NSBezelBorder];
    [profScroll setDrawsBackground:YES];
    [profScroll setBackgroundColor:boxBg];

    _profilesTable = [[NSTableView alloc] initWithFrame:[profScroll bounds]];
    NSTableColumn *colP = [[NSTableColumn alloc] initWithIdentifier:@"name"];
    [colP setTitle:@"Profile"];
    [colP setWidth:230];
    [_profilesTable addTableColumn:colP];
    [_profilesTable setDataSource:self];
    [_profilesTable setDelegate:self];
    [_profilesTable setHeaderView:nil];
    [profScroll setDocumentView:_profilesTable];
    [content addSubview:profScroll];

    _addProfileBtn = [[NSButton alloc] initWithFrame:NSMakeRect(20, 34, 55, 26)];
    [_addProfileBtn setBezelStyle:NSBezelStyleRounded];
    [_addProfileBtn setTitle:@"New"];
    [_addProfileBtn setTarget:self];
    [_addProfileBtn setAction:@selector(onNewProfile:)];
    [content addSubview:_addProfileBtn];

    _duplicateBtn = [[NSButton alloc] initWithFrame:NSMakeRect(78, 34, 75, 26)];
    [_duplicateBtn setBezelStyle:NSBezelStyleRounded];
    [_duplicateBtn setTitle:@"Duplicate"];
    [_duplicateBtn setTarget:self];
    [_duplicateBtn setAction:@selector(onDuplicateProfile:)];
    [content addSubview:_duplicateBtn];

    _renameBtn = [[NSButton alloc] initWithFrame:NSMakeRect(156, 34, 62, 26)];
    [_renameBtn setBezelStyle:NSBezelStyleRounded];
    [_renameBtn setTitle:@"Rename"];
    [_renameBtn setTarget:self];
    [_renameBtn setAction:@selector(onRenameProfile:)];
    [content addSubview:_renameBtn];

    _deleteBtn = [[NSButton alloc] initWithFrame:NSMakeRect(221, 34, 52, 26)];
    [_deleteBtn setBezelStyle:NSBezelStyleRounded];
    [_deleteBtn setTitle:@"Del"];
    [_deleteBtn setTarget:self];
    [_deleteBtn setAction:@selector(onDeleteProfile:)];
    [content addSubview:_deleteBtn];

    // =========================================================
    // PANNELLO DESTRO: MAPPINGS & CONTROLLI
    // =========================================================
    _profileTitleLabel = [NSTextField labelWithString:@""];
    [_profileTitleLabel setFont:[NSFont boldSystemFontOfSize:16]];
    [_profileTitleLabel setTextColor:plasmaOrange];
    [_profileTitleLabel setFrame:NSMakeRect(290, 578, 640, 24)];
    [content addSubview:_profileTitleLabel];

    _profileDescLabel = [NSTextField labelWithString:@""];
    [_profileDescLabel setFont:[NSFont systemFontOfSize:12]];
    [_profileDescLabel setTextColor:[NSColor colorWithWhite:0.7 alpha:1.0]];
    [_profileDescLabel setFrame:NSMakeRect(290, 556, 640, 18)];
    [content addSubview:_profileDescLabel];

    NSScrollView *bindScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(290, 115, 650, 430)];
    [bindScroll setHasVerticalScroller:YES];
    [bindScroll setBorderType:NSBezelBorder];
    [bindScroll setDrawsBackground:YES];
    [bindScroll setBackgroundColor:boxBg];

    _bindingsTable = [[NSTableView alloc] initWithFrame:[bindScroll bounds]];

    NSTableColumn *colKey = [[NSTableColumn alloc] initWithIdentifier:@"key"];
    [colKey setTitle:@"Physical Key / Combo"];
    [colKey setWidth:170];
    [_bindingsTable addTableColumn:colKey];

    NSTableColumn *colAct = [[NSTableColumn alloc] initWithIdentifier:@"action"];
    [colAct setTitle:@"Target Action"];
    [colAct setWidth:220];
    [_bindingsTable addTableColumn:colAct];

    NSTableColumn *colLbl = [[NSTableColumn alloc] initWithIdentifier:@"label"];
    [colLbl setTitle:@"Description"];
    [colLbl setWidth:230];
    [_bindingsTable addTableColumn:colLbl];

    [_bindingsTable setDataSource:self];
    [_bindingsTable setDelegate:self];
    [_bindingsTable setTarget:self];
    [_bindingsTable setDoubleAction:@selector(onTableDoubleClicked:)];
    [bindScroll setDocumentView:_bindingsTable];
    [content addSubview:bindScroll];

    _recordKeyBtn = [[NSButton alloc] initWithFrame:NSMakeRect(290, 72, 195, 30)];
    [_recordKeyBtn setBezelStyle:NSBezelStyleRounded];
    [_recordKeyBtn setTitle:@"◉ Press Key to Bind..."];
    [_recordKeyBtn setTarget:self];
    [_recordKeyBtn setAction:@selector(onRecordKey:)];
    [content addSubview:_recordKeyBtn];

    _editBindingBtn = [[NSButton alloc] initWithFrame:NSMakeRect(490, 72, 120, 30)];
    [_editBindingBtn setBezelStyle:NSBezelStyleRounded];
    [_editBindingBtn setTitle:@"✎ Edit Binding..."];
    [_editBindingBtn setTarget:self];
    [_editBindingBtn setAction:@selector(onEditBinding:)];
    [content addSubview:_editBindingBtn];

    _removeBindingBtn = [[NSButton alloc] initWithFrame:NSMakeRect(615, 72, 140, 30)];
    [_removeBindingBtn setBezelStyle:NSBezelStyleRounded];
    [_removeBindingBtn setTitle:@"- Remove Binding"];
    [_removeBindingBtn setTarget:self];
    [_removeBindingBtn setAction:@selector(onRemoveBinding:)];
    [content addSubview:_removeBindingBtn];

    _statusInfoLabel = [NSTextField labelWithString:@""];
    [_statusInfoLabel setFont:[NSFont systemFontOfSize:11]];
    [_statusInfoLabel setTextColor:plasmaOrange];
    [_statusInfoLabel setFrame:NSMakeRect(290, 36, 420, 20)];
    [content addSubview:_statusInfoLabel];

    _setActiveBtn = [[NSButton alloc] initWithFrame:NSMakeRect(710, 30, 150, 32)];
    [_setActiveBtn setBezelStyle:NSBezelStyleRounded];
    [_setActiveBtn setTitle:@"Set as Active Profile"];
    [_setActiveBtn setTarget:self];
    [_setActiveBtn setAction:@selector(onSetActive:)];
    [content addSubview:_setActiveBtn];

    NSButton *closeBtn = [[NSButton alloc] initWithFrame:NSMakeRect(865, 30, 75, 32)];
    [closeBtn setBezelStyle:NSBezelStyleRounded];
    [closeBtn setTitle:@"Close"];
    [closeBtn setTarget:self];
    [closeBtn setAction:@selector(onClose:)];
    [content addSubview:closeBtn];

    [self refreshList];
    [self applyCurrentThemeColors];
}

- (void)applyCurrentThemeColors {
    PLATOThemeColors *tc = [PLATOThemeColors currentThemeColors];
    [self.window setBackgroundColor:tc.bgDark];
    if (_profScroll) [_profScroll setBackgroundColor:tc.bgBox];
    if (_profilesTable) [_profilesTable setBackgroundColor:tc.bgBox];
    if (_bindScroll) [_bindScroll setBackgroundColor:tc.bgBox];
    if (_bindingsTable) [_bindingsTable setBackgroundColor:tc.bgBox];
    if (_profHeader) [_profHeader setTextColor:tc.normal];
    if (_profileTitleLabel) [_profileTitleLabel setTextColor:tc.normal];
    if (_profileDescLabel) [_profileDescLabel setTextColor:tc.muted];
    if (_statusInfoLabel) [_statusInfoLabel setTextColor:tc.normal];
    [_profilesTable reloadData];
    [_bindingsTable reloadData];
}

- (void)refreshList {
    if (!_keymapManager) return;
    if (_selectedProfileIndex < 0 || _selectedProfileIndex >= _keymapManager->profile_count) {
        _selectedProfileIndex = _keymapManager->active_profile_index;
    }
    [_profilesTable reloadData];
    [_profilesTable selectRowIndexes:[NSIndexSet indexSetWithIndex:_selectedProfileIndex] byExtendingSelection:NO];
    [self updateSelectedProfileView];
}

- (void)updateSelectedProfileView {
    if (!_keymapManager || _selectedProfileIndex < 0 || _selectedProfileIndex >= _keymapManager->profile_count) {
        [_profileTitleLabel setStringValue:@"No Profile Selected"];
        [_profileDescLabel setStringValue:@""];
        [_statusInfoLabel setStringValue:@""];
        [_bindingsTable reloadData];
        return;
    }

    const plato_keymap_profile_t *p = &_keymapManager->profiles[_selectedProfileIndex];
    NSString *badge = p->is_builtin ? @" [Built-in / Read-Only]" : @" [User Defined]";
    [_profileTitleLabel setStringValue:[NSString stringWithFormat:@"%s%@", p->name, badge]];
    [_profileDescLabel setStringValue:[NSString stringWithUTF8String:p->description]];

    BOOL isBuiltin = p->is_builtin;
    [_renameBtn setEnabled:!isBuiltin];
    [_deleteBtn setEnabled:!isBuiltin];
    [_recordKeyBtn setEnabled:!isBuiltin];
    NSInteger selRow = [_bindingsTable selectedRow];
    BOOL canEditBind = (!isBuiltin && selRow >= 0 && selRow < p->binding_count);
    [_editBindingBtn setEnabled:canEditBind];
    [_removeBindingBtn setEnabled:canEditBind];

    BOOL isActive = (_selectedProfileIndex == _keymapManager->active_profile_index);
    [_setActiveBtn setEnabled:!isActive];
    [_setActiveBtn setTitle:isActive ? @"✓ Active Profile" : @"Set as Active Profile"];

    if (isBuiltin) {
        [_statusInfoLabel setStringValue:@"Built-in profile is read-only. Click 'Duplicate' to customize."];
    } else {
        [_statusInfoLabel setStringValue:[NSString stringWithFormat:@"%d custom key bindings configured.", p->binding_count]];
    }

    [_bindingsTable reloadData];
}

// =========================================================
// GESTIONE EVENTI RECORD KEY ("Press Key to Bind...")
// =========================================================
- (void)onRecordKey:(id)sender {
    if (!_keymapManager || _selectedProfileIndex < 0 || _selectedProfileIndex >= _keymapManager->profile_count) return;
    if (_keymapManager->profiles[_selectedProfileIndex].is_builtin) return;

    _isRecordingKey = YES;
    [_statusInfoLabel setStringValue:@"◉ PRESS ANY KEY OR COMBINATION (or Esc to cancel)..."];
    [self.window makeFirstResponder:self.window];
}

- (void)keyDown:(NSEvent *)event {
    if (_isRecordingKey && _selectedProfileIndex >= 0) {
        unichar c = 0;
        NSString *chars = [event charactersIgnoringModifiers];
        if ([chars length] > 0) c = [chars characterAtIndex:0];

        // Esc per annullare
        if (c == 27) {
            _isRecordingKey = NO;
            [self updateSelectedProfileView];
            return;
        }

        NSEventModifierFlags flags = [event modifierFlags];
        uint8_t mods = 0;
        if (flags & NSEventModifierFlagShift)   mods |= PLATO_KEY_MOD_SHIFT;
        if (flags & NSEventModifierFlagControl) mods |= PLATO_KEY_MOD_CTRL;
        if (flags & NSEventModifierFlagOption)  mods |= PLATO_KEY_MOD_ALT;
        if (flags & NSEventModifierFlagCommand) mods |= PLATO_KEY_MOD_GUI;

        uint16_t vk = 0;
        uint32_t cp = 0;

        // Gestione Numpad
        if (flags & NSEventModifierFlagNumericPad) {
            switch ([event keyCode]) {
                case 82: vk = PLATO_VKEY_NUMPAD0; break;
                case 83: vk = PLATO_VKEY_NUMPAD1; break;
                case 84: vk = PLATO_VKEY_NUMPAD2; break;
                case 85: vk = PLATO_VKEY_NUMPAD3; break;
                case 86: vk = PLATO_VKEY_NUMPAD4; break;
                case 87: vk = PLATO_VKEY_NUMPAD5; break;
                case 88: vk = PLATO_VKEY_NUMPAD6; break;
                case 89: vk = PLATO_VKEY_NUMPAD7; break;
                case 91: vk = PLATO_VKEY_NUMPAD8; break;
                case 92: vk = PLATO_VKEY_NUMPAD9; break;
            }
        }

        if (vk == 0) {
            switch ([event keyCode]) {
                case 126: vk = PLATO_VKEY_UP; break;
                case 125: vk = PLATO_VKEY_DOWN; break;
                case 123: vk = PLATO_VKEY_LEFT; break;
                case 124: vk = PLATO_VKEY_RIGHT; break;
                case 48:  vk = PLATO_VKEY_TAB; break;
                case 36:  vk = PLATO_VKEY_RETURN; break;
                case 51:  vk = PLATO_VKEY_BACKSPACE; break;
                case 116: vk = PLATO_VKEY_PAGEUP; break;
                case 121: vk = PLATO_VKEY_PAGEDOWN; break;
                case 115: vk = PLATO_VKEY_HOME; break;
                case 119: vk = PLATO_VKEY_END; break;
                default:
                    if (c >= NSF1FunctionKey && c <= NSF12FunctionKey) {
                        vk = (uint16_t)(PLATO_VKEY_F1 + (c - NSF1FunctionKey));
                    } else if (c >= 32 && c <= 126) {
                        cp = (uint32_t)c;
                    }
                    break;
            }
        }

        _isRecordingKey = NO;
        plato_key_binding_t newB;
        memset(&newB, 0, sizeof(newB));
        newB.vkey = vk;
        newB.codepoint = cp;
        newB.modifiers = mods;
        [self showUniversalActionPickerForBinding:&newB isEditing:NO editIndex:-1];
        return;
    }
    [super keyDown:event];
}

// =========================================================
// UNIVERSAL ACTION PICKER: Tasti PLATO o Qualsiasi Carattere
// =========================================================
- (void)showUniversalActionPickerForBinding:(const plato_key_binding_t *)initBind isEditing:(BOOL)isEditing editIndex:(int)editIndex {
    if (!initBind) return;
    plato_key_binding_t b = *initBind;

    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:isEditing ? @"Edit Target Action" : @"Assign Target Action"];
    [alert setInformativeText:[NSString stringWithFormat:@"Physical Key: %@\nChoose what this key will send:", formatPhysicalKey(&b)]];

    // Accessory View interattiva
    NSView *acc = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 360, 140)];

    // 1. Categoria: Carattere Libero vs Tasto Speciale PLATO
    NSSegmentedControl *modeSeg = [NSSegmentedControl segmentedControlWithLabels:@[@"Custom Character", @"PLATO Function Key"]
                                                                   trackingMode:NSSegmentSwitchTrackingSelectOne
                                                                         target:nil
                                                                         action:nil];
    [modeSeg setFrame:NSMakeRect(10, 105, 340, 26)];
    [modeSeg setSelectedSegment:(b.type == PLATO_BIND_ACTION_PLATO_KEY || (b.type == 0 && b.vkey != 0 && (b.vkey < PLATO_VKEY_NUMPAD0 || b.vkey > PLATO_VKEY_NUMPAD9))) ? 1 : 0];
    [acc addSubview:modeSeg];

    // 2. Controlli Carattere Libero (NSTextField)
    NSTextField *charLbl = [NSTextField labelWithString:@"Type character or symbol to send:"];
    [charLbl setFrame:NSMakeRect(10, 75, 230, 20)];
    [acc addSubview:charLbl];

    NSTextField *charInput = [[NSTextField alloc] initWithFrame:NSMakeRect(245, 73, 50, 24)];
    [charInput setAlignment:NSTextAlignmentCenter];
    [charInput setFont:[NSFont boldSystemFontOfSize:15]];
    if (b.type == PLATO_BIND_ACTION_RAW_BYTE && b.target.raw_byte >= 32) {
        [charInput setStringValue:[NSString stringWithFormat:@"%c", (char)b.target.raw_byte]];
    } else if (b.codepoint >= 32 && b.codepoint <= 126) {
        [charInput setStringValue:[NSString stringWithFormat:@"%c", (char)b.codepoint]];
    } else {
        [charInput setStringValue:@""];
    }
    [acc addSubview:charInput];

    // 3. Controlli Tasto Speciale PLATO (NSPopUpButton)
    NSTextField *platoLbl = [NSTextField labelWithString:@"Or select PLATO key:"];
    [platoLbl setFrame:NSMakeRect(10, 42, 140, 20)];
    [acc addSubview:platoLbl];

    NSPopUpButton *platoPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(150, 40, 195, 26) pullsDown:NO];
    NSArray *platoKeyNames = @[
        @"NEXT", @"BACK", @"HELP", @"LAB", @"DATA", @"STOP",
        @"EDIT", @"COPY", @"MICRO", @"FONT", @"SUPER (Up)", @"SUB (Down)",
        @"ERASE (Left)", @"ACCESS / TAB", @"ANS", @"TERM", @"SQUARE"
    ];
    [platoPopup addItemsWithTitles:platoKeyNames];
    static const plato_special_key_t pkeys[] = {
        PLATO_KEY_NEXT, PLATO_KEY_BACK, PLATO_KEY_HELP, PLATO_KEY_LAB,
        PLATO_KEY_DATA, PLATO_KEY_STOP, PLATO_KEY_EDIT, PLATO_KEY_COPY,
        PLATO_KEY_MICRO, PLATO_KEY_FONT, PLATO_KEY_SUPER, PLATO_KEY_SUB,
        PLATO_KEY_ERASE, PLATO_KEY_ACCESS, PLATO_KEY_ANS, PLATO_KEY_TERM, PLATO_KEY_SQUARE
    };
    NSInteger selPlato = 0;
    if (b.type == PLATO_BIND_ACTION_PLATO_KEY) {
        for (int i = 0; i < 17; i++) {
            if (pkeys[i] == b.target.plato_key) { selPlato = i; break; }
        }
    }
    [platoPopup selectItemAtIndex:selPlato];
    [acc addSubview:platoPopup];

    // 4. Descrizione Opzionale
    NSTextField *descInput = [[NSTextField alloc] initWithFrame:NSMakeRect(10, 8, 335, 22)];
    [descInput setPlaceholderString:@"Optional description (e.g. Move Forward, Fire)"];
    if (strlen(b.label) > 0) {
        [descInput setStringValue:[NSString stringWithUTF8String:b.label]];
    }
    [acc addSubview:descInput];

    [alert setAccessoryView:acc];
    [alert addButtonWithTitle:isEditing ? @"Save" : @"Assign"];
    [alert addButtonWithTitle:@"Cancel"];

    if ([alert runModal] == NSAlertFirstButtonReturn) {
        NSInteger mode = [modeSeg selectedSegment];
        NSString *userDesc = [descInput stringValue];

        if (mode == 0) {
            // Qualsiasi Carattere / Simbolo digitato
            NSString *val = [charInput stringValue];
            if ([val length] == 0) val = @" ";
            char raw = [val characterAtIndex:0];
            b.type = PLATO_BIND_ACTION_RAW_BYTE;
            b.target.raw_byte = (uint8_t)raw;
            if ([userDesc length] > 0) {
                snprintf(b.label, sizeof(b.label), "%s", [userDesc UTF8String]);
            } else {
                snprintf(b.label, sizeof(b.label), "Type '%c'", raw);
            }
        } else {
            // Tasto Funzione PLATO
            b.type = PLATO_BIND_ACTION_PLATO_KEY;
            NSInteger sel = [platoPopup indexOfSelectedItem];
            if (sel >= 0 && sel < 17) {
                b.target.plato_key = (uint16_t)pkeys[sel];
            } else {
                b.target.plato_key = (uint16_t)PLATO_KEY_NEXT;
            }
            if ([userDesc length] > 0) {
                snprintf(b.label, sizeof(b.label), "%s", [userDesc UTF8String]);
            } else {
                snprintf(b.label, sizeof(b.label), "PLATO: %s", [[platoPopup titleOfSelectedItem] UTF8String]);
            }
        }

        plato_keymap_profile_t *p = &_keymapManager->profiles[_selectedProfileIndex];
        if (isEditing && editIndex >= 0 && editIndex < p->binding_count) {
            plato_keymap_profile_set_binding(p, editIndex, &b);
        } else {
            plato_keymap_profile_add_binding(p, &b);
        }
        plato_keymap_save_profile(p);
        [self updateSelectedProfileView];

        if ([self.delegate respondsToSelector:@selector(keyboardProfilesDidUpdate:)]) {
            [self.delegate keyboardProfilesDidUpdate:self];
        }
    }
}

// =========================================================
// AZIONI PULSANTI
// =========================================================
- (void)onNewProfile:(id)sender {
    if (!_keymapManager) return;
    plato_keymap_profile_t np;
    memset(&np, 0, sizeof(np));
    snprintf(np.name, sizeof(np.name), "Custom Profile %d", _keymapManager->profile_count);
    snprintf(np.description, sizeof(np.description), "User-defined key mappings");

    int idx = plato_keymap_add_profile(_keymapManager, &np);
    if (idx >= 0) {
        plato_keymap_save_profile(&_keymapManager->profiles[idx]);
        _selectedProfileIndex = idx;
        [self refreshList];
        if ([self.delegate respondsToSelector:@selector(keyboardProfilesDidUpdate:)]) {
            [self.delegate keyboardProfilesDidUpdate:self];
        }
    }
}

- (void)onDuplicateProfile:(id)sender {
    if (!_keymapManager || _selectedProfileIndex < 0) return;
    char newName[64];
    snprintf(newName, sizeof(newName), "%s Copy", _keymapManager->profiles[_selectedProfileIndex].name);
    if (plato_keymap_duplicate_profile(_keymapManager, (int)_selectedProfileIndex, newName)) {
        _selectedProfileIndex = _keymapManager->profile_count - 1;
        plato_keymap_save_profile(&_keymapManager->profiles[_selectedProfileIndex]);
        [self refreshList];
        if ([self.delegate respondsToSelector:@selector(keyboardProfilesDidUpdate:)]) {
            [self.delegate keyboardProfilesDidUpdate:self];
        }
    }
}

- (void)onRenameProfile:(id)sender {
    if (!_keymapManager || _selectedProfileIndex < 0) return;
    plato_keymap_profile_t *p = &_keymapManager->profiles[_selectedProfileIndex];
    if (p->is_builtin) return;

    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Edit Profile"];
    [alert setInformativeText:@"Update profile name and description:"];

    NSView *container = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 300, 96)];

    NSTextField *labelName = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 74, 300, 18)];
    [labelName setStringValue:@"Profile Name:"];
    [labelName setBezeled:NO];
    [labelName setDrawsBackground:NO];
    [labelName setEditable:NO];
    [labelName setSelectable:NO];
    [labelName setFont:[NSFont boldSystemFontOfSize:11]];
    [container addSubview:labelName];

    NSTextField *inputName = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 50, 300, 22)];
    [inputName setStringValue:[NSString stringWithUTF8String:p->name]];
    [container addSubview:inputName];

    NSTextField *labelDesc = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 26, 300, 18)];
    [labelDesc setStringValue:@"Description:"];
    [labelDesc setBezeled:NO];
    [labelDesc setDrawsBackground:NO];
    [labelDesc setEditable:NO];
    [labelDesc setSelectable:NO];
    [labelDesc setFont:[NSFont boldSystemFontOfSize:11]];
    [container addSubview:labelDesc];

    NSTextField *inputDesc = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 2, 300, 22)];
    [inputDesc setStringValue:[NSString stringWithUTF8String:p->description]];
    [container addSubview:inputDesc];

    [alert setAccessoryView:container];
    [alert addButtonWithTitle:@"Save"];
    [alert addButtonWithTitle:@"Cancel"];

    if ([alert runModal] == NSAlertFirstButtonReturn && [[inputName stringValue] length] > 0) {
        plato_keymap_update_profile_info(_keymapManager, (int)_selectedProfileIndex,
                                         [[inputName stringValue] UTF8String],
                                         [[inputDesc stringValue] UTF8String]);
        [self refreshList];
        if ([self.delegate respondsToSelector:@selector(keyboardProfilesDidUpdate:)]) {
            [self.delegate keyboardProfilesDidUpdate:self];
        }
    }
}

- (void)onDeleteProfile:(id)sender {
    if (!_keymapManager || _selectedProfileIndex < 0) return;
    plato_keymap_profile_t *p = &_keymapManager->profiles[_selectedProfileIndex];
    if (p->is_builtin) return;

    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Delete Profile"];
    [alert setInformativeText:[NSString stringWithFormat:@"Are you sure you want to delete profile '%s'?", p->name]];
    [alert addButtonWithTitle:@"Delete"];
    [alert addButtonWithTitle:@"Cancel"];

    if ([alert runModal] == NSAlertFirstButtonReturn) {
        plato_keymap_delete_profile(_keymapManager, (int)_selectedProfileIndex);
        _selectedProfileIndex = _keymapManager->active_profile_index;
        [self refreshList];
        if ([self.delegate respondsToSelector:@selector(keyboardProfilesDidUpdate:)]) {
            [self.delegate keyboardProfilesDidUpdate:self];
        }
    }
}

- (void)onEditBinding:(id)sender {
    if (!_keymapManager || _selectedProfileIndex < 0 || _selectedProfileIndex >= _keymapManager->profile_count) return;
    plato_keymap_profile_t *p = &_keymapManager->profiles[_selectedProfileIndex];
    if (p->is_builtin) return;
    NSInteger selRow = [_bindingsTable selectedRow];
    if (selRow < 0 || selRow >= p->binding_count) return;

    [self showUniversalActionPickerForBinding:&p->bindings[selRow] isEditing:YES editIndex:(int)selRow];
}

- (void)onRemoveBinding:(id)sender {
    if (!_keymapManager || _selectedProfileIndex < 0) return;
    plato_keymap_profile_t *p = &_keymapManager->profiles[_selectedProfileIndex];
    if (p->is_builtin) return;

    NSInteger row = [_bindingsTable selectedRow];
    if (row >= 0 && row < p->binding_count) {
        plato_keymap_profile_remove_binding(p, (int)row);
        plato_keymap_save_profile(p);
        [self updateSelectedProfileView];
        if ([self.delegate respondsToSelector:@selector(keyboardProfilesDidUpdate:)]) {
            [self.delegate keyboardProfilesDidUpdate:self];
        }
    }
}

- (void)onSetActive:(id)sender {
    if (!_keymapManager || _selectedProfileIndex < 0) return;
    plato_keymap_set_active_by_index(_keymapManager, (int)_selectedProfileIndex);
    plato_keymap_save_all(_keymapManager);
    [_profilesTable reloadData];
    [self updateSelectedProfileView];
    if ([self.delegate respondsToSelector:@selector(keyboardProfilesDidUpdate:)]) {
        [self.delegate keyboardProfilesDidUpdate:self];
    }
}

- (void)onClose:(id)sender {
    [self.window close];
}

// =========================================================
// NSTableView DataSource & Delegate
// =========================================================
- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    if (!_keymapManager) return 0;
    if (tableView == _profilesTable) {
        return _keymapManager->profile_count;
    } else if (tableView == _bindingsTable) {
        if (_selectedProfileIndex >= 0 && _selectedProfileIndex < _keymapManager->profile_count) {
            return _keymapManager->profiles[_selectedProfileIndex].binding_count;
        }
    }
    return 0;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row {
    NSTextField *label = [NSTextField labelWithString:@""];
    [label setFont:[NSFont systemFontOfSize:12]];

    if (tableView == _profilesTable) {
        if (row < _keymapManager->profile_count) {
            const plato_keymap_profile_t *p = &_keymapManager->profiles[row];
            NSString *prefix = (row == _keymapManager->active_profile_index) ? @"► " : @"   ";
            NSString *title = [NSString stringWithFormat:@"%@%s", prefix, p->name];
            if (p->is_builtin) title = [title stringByAppendingString:@" (Built-in)"];
            [label setStringValue:title];
            PLATOThemeColors *tc = [PLATOThemeColors currentThemeColors];
            if (row == _keymapManager->active_profile_index) {
                [label setTextColor:tc.bright];
            } else {
                [label setTextColor:tc.normal];
            }
        }
    } else if (tableView == _bindingsTable) {
        if (_selectedProfileIndex >= 0 && _selectedProfileIndex < _keymapManager->profile_count) {
            const plato_keymap_profile_t *p = &_keymapManager->profiles[_selectedProfileIndex];
            if (row < p->binding_count) {
                const plato_key_binding_t *b = &p->bindings[row];
                NSString *ident = [tableColumn identifier];
                if ([ident isEqualToString:@"key"]) {
                    [label setStringValue:formatPhysicalKey(b)];
                    [label setTextColor:[NSColor colorWithCalibratedRed:255.0/255.0 green:160.0/255.0 blue:50.0/255.0 alpha:1.0]];
                } else if ([ident isEqualToString:@"action"]) {
                    [label setStringValue:formatTargetAction(b)];
                } else if ([ident isEqualToString:@"label"]) {
                    [label setStringValue:[NSString stringWithUTF8String:b->label]];
                    [label setTextColor:[NSColor colorWithWhite:0.6 alpha:1.0]];
                }
            }
        }
    }
    return label;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification {
    if ([notification object] == _profilesTable) {
        NSInteger row = [_profilesTable selectedRow];
        if (row >= 0 && row < _keymapManager->profile_count) {
            _selectedProfileIndex = row;
            [self updateSelectedProfileView];
        }
    } else if ([notification object] == _bindingsTable) {
        if (_selectedProfileIndex >= 0 && _selectedProfileIndex < _keymapManager->profile_count) {
            BOOL isBuiltin = _keymapManager->profiles[_selectedProfileIndex].is_builtin;
            NSInteger selRow = [_bindingsTable selectedRow];
            BOOL canEdit = (!isBuiltin && selRow >= 0 && selRow < _keymapManager->profiles[_selectedProfileIndex].binding_count);
            [_editBindingBtn setEnabled:canEdit];
            [_removeBindingBtn setEnabled:canEdit];
        }
    }
}

- (void)onTableDoubleClicked:(id)sender {
    if (sender == _bindingsTable) {
        [self onEditBinding:sender];
    }
}

@end
