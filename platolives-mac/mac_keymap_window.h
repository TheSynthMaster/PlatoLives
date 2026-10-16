#import <Cocoa/Cocoa.h>
#include "plato/plato_keymap.h"
#include "plato/plato_keyboard.h"

@class PLATOKeyboardProfilesWindowController;

@protocol PLATOKeyboardProfilesDelegate <NSObject>
- (void)keyboardProfilesDidUpdate:(PLATOKeyboardProfilesWindowController *)controller;
@end

@interface PLATOKeyboardProfilesWindowController : NSWindowController <NSTableViewDataSource, NSTableViewDelegate, NSWindowDelegate>
@property (nonatomic, weak) id<PLATOKeyboardProfilesDelegate> delegate;
@property (nonatomic, assign) plato_keymap_manager_t *keymapManager;

- (instancetype)initWithKeymapManager:(plato_keymap_manager_t *)mgr;
- (void)refreshList;
@end
