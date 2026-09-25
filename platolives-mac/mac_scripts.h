#import <Cocoa/Cocoa.h>
#include "plato/plato_script.h"

@class PLATOScriptsWindowController;

@protocol PLATOScriptsDelegate <NSObject>
- (void)scriptsControllerDidUpdateScripts:(PLATOScriptsWindowController *)controller;
@end

@interface PLATOScriptsWindowController : NSWindowController <NSTableViewDataSource, NSTableViewDelegate, NSWindowDelegate>
@property (nonatomic, weak) id<PLATOScriptsDelegate> delegate;
@property (nonatomic, assign) plato_script_list_t *scriptList;

- (instancetype)initWithScriptList:(plato_script_list_t *)list;
- (void)refreshList;
@end
