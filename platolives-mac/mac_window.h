#import <Cocoa/Cocoa.h>
#import "mac_view.h"

@class PLATOTestRunner;
@class PLATOTerminalWindowController;
@class PLATOTextBufferWindowController;

@protocol PLATOTerminalWindowDelegate <NSObject>
- (void)terminalWindowControllerWillClose:(PLATOTerminalWindowController *)controller;
@end

@interface PLATOTerminalWindowController : NSWindowController <NSWindowDelegate>
@property (nonatomic, weak) id<PLATOTerminalWindowDelegate> appDelegate;
@property (nonatomic, strong) PLATOView *view;
@property (nonatomic, copy) NSDictionary *currentProfile;

- (instancetype)initWithProfile:(NSDictionary *)profile;
- (void)applyProfile:(NSDictionary *)profile;
- (void)updateTitleWithMetadata;
@end

@interface PLATOAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate, NSMenuDelegate, PLATOTerminalWindowDelegate>
@property (nonatomic, strong, readonly) NSWindow *window; // Finestra attiva corrente
@property (nonatomic, strong, readonly) PLATOView *view;   // Vista attiva corrente
@property (nonatomic, strong) PLATOTestRunner *testRunner;

- (PLATOTerminalWindowController *)activeTerminalController;
- (PLATOTerminalWindowController *)openNewWindowWithProfile:(NSDictionary *)profile;

- (void)newWindow:(id)sender;
- (void)newWindowWithProfileItem:(id)sender;
- (void)activeWindowDidChange:(PLATOTerminalWindowController *)controller;

- (void)setDisplayCrisp:(id)sender;
- (void)setDisplayRealPlasma:(id)sender;
- (void)setDisplaySplit:(id)sender;
- (void)selectPlasmaDecay:(id)sender;
- (void)toggleKeyboardReference:(id)sender;
- (void)toggleTrafficLog:(id)sender;
- (void)toggleRendererPerformanceLog:(id)sender;
- (void)toggleMainFullScreen:(id)sender;
- (void)toggleFPSCounter:(id)sender;
- (void)showAbout:(id)sender;
- (void)rebuildConnectionMenu;
- (void)showProfilesWindow:(id)sender;
- (void)connectDefaultProfile:(id)sender;
- (void)disconnectSession:(id)sender;
- (void)quickConnectProfile:(id)sender;
- (void)copyScreen:(id)sender;
- (void)copyScreenImage:(id)sender;
- (void)copyTextAction:(id)sender;
- (void)showTextBufferWindow:(id)sender;
- (void)pasteText:(id)sender;
- (void)cancelPaste:(id)sender;
@end
