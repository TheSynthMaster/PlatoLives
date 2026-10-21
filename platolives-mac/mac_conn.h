#import <Cocoa/Cocoa.h>
#include "plato/plato_transport.h"

typedef void (^mac_conn_done_block_t)(BOOL success);

@interface PLATOConnectingWindowController : NSWindowController

+ (instancetype)sharedController;

- (void)showForParentWindow:(NSWindow *)parentWindow
                profileName:(NSString *)name
                       host:(NSString *)host
                       port:(int)port
                  transport:(plato_transport_t *)t
                 completion:(mac_conn_done_block_t)completion;

- (void)closeConnectingWindow;

@end
