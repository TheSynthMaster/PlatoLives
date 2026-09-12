#import <Cocoa/Cocoa.h>

@class PLATOView;

@interface PLATOTestRunner : NSObject
- (instancetype)initWithView:(PLATOView *)view scriptPath:(NSString *)path;
- (void)start;
@end
