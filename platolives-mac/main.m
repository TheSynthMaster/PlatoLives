#import <Cocoa/Cocoa.h>
#import "mac_window.h"

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        PLATOAppDelegate *delegate = [[PLATOAppDelegate alloc] init];
        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}
