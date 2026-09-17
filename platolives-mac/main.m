#import <Cocoa/Cocoa.h>
#import "mac_window.h"

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSDictionary *spellDefaults = @{
            @"NSAutomaticSpellingCorrectionEnabled": @NO,
            @"NSContinuousSpellCheckingEnabled": @NO,
            @"NSGrammarCheckingEnabled": @NO
        };
        [[NSUserDefaults standardUserDefaults] registerDefaults:spellDefaults];

        NSApplication *app = [NSApplication sharedApplication];
        BOOL isHeadlessTest = NO;
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--test-script") == 0) {
                isHeadlessTest = YES;
                break;
            }
        }
        if (isHeadlessTest) {
            [app setActivationPolicy:NSApplicationActivationPolicyAccessory];
        } else {
            [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        }
        PLATOAppDelegate *delegate = [[PLATOAppDelegate alloc] init];
        [app setDelegate:delegate];
        if (isHeadlessTest) {
            [app finishLaunching];
            [delegate applicationDidFinishLaunching:[NSNotification notificationWithName:NSApplicationDidFinishLaunchingNotification object:app]];
            while (1) {
                @autoreleasepool {
                    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, true);
                }
            }
        } else {
            [app run];
        }
    }
    return 0;
}
