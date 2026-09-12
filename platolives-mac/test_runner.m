#import "test_runner.h"
#import "mac_view.h"

@interface PLATOTestRunner ()
@property (nonatomic, weak) PLATOView *view;
@property (nonatomic, copy) NSString *scriptPath;
@property (nonatomic, strong) NSArray<NSDictionary *> *commands;
@property (nonatomic) NSUInteger index;
@property (nonatomic, copy) NSString *screenshotsDirectory;
@property (nonatomic, copy) NSString *resultsPath;
@end

@implementation PLATOTestRunner

- (instancetype)initWithView:(PLATOView *)view scriptPath:(NSString *)path {
    self = [super init];
    if (self) { _view = view; _scriptPath = [path stringByExpandingTildeInPath]; }
    return self;
}

- (void)log:(NSString *)message {
    NSString *line = [NSString stringWithFormat:@"%@ %@\n", [NSDate date], message];
    NSLog(@"[TEST] %@", message);
    NSFileHandle *handle = [NSFileHandle fileHandleForWritingAtPath:self.resultsPath];
    if (handle) { [handle seekToEndOfFile]; [handle writeData:[line dataUsingEncoding:NSUTF8StringEncoding]]; [handle closeFile]; }
}

- (void)fail:(NSString *)message line:(NSNumber *)line {
    NSNumber *lineNumber = line ? line : @0;
    [self log:[NSString stringWithFormat:@"ERROR line %@: %@", lineNumber, message]];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 100 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{ [NSApp terminate:nil]; });
}

- (NSArray<NSDictionary *> *)parse:(NSString *)text error:(NSString **)error {
    NSMutableArray *result = [NSMutableArray array];
    NSArray *lines = [text componentsSeparatedByCharactersInSet:[NSCharacterSet newlineCharacterSet]];
    for (NSUInteger i = 0; i < [lines count]; i++) {
        NSString *raw = [lines[i] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
        if ([raw length] == 0 || [raw hasPrefix:@"#"]) continue;
        NSRange space = [raw rangeOfCharacterFromSet:[NSCharacterSet whitespaceCharacterSet]];
        NSString *op = space.location == NSNotFound ? raw : [raw substringToIndex:space.location];
        NSString *arg = space.location == NSNotFound ? @"" : [[raw substringFromIndex:space.location + 1] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
        op = [[op lowercaseString] stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@"-"]];
        NSDictionary *aliases = @{ @"invia": @"send", @"attendi": @"wait", @"tasto": @"key", @"salva-screen": @"screenshot", @"schermata": @"screenshot" };
        if (aliases[op]) op = aliases[op];
        NSSet *valid = [NSSet setWithArray:@[@"send", @"send-env", @"key", @"wait", @"screenshot", @"renderer", @"diagnostic-log", @"plasma-log", @"quit"]];
        if (![valid containsObject:op]) { if (error) *error = [NSString stringWithFormat:@"line %lu: unknown command '%@'", (unsigned long)i + 1, op]; return nil; }
        if (![op isEqualToString:@"quit"] && [arg length] == 0) { if (error) *error = [NSString stringWithFormat:@"line %lu: missing argument", (unsigned long)i + 1]; return nil; }
        if ([op isEqualToString:@"diagnostic-log"] && ![[NSSet setWithArray:@[@"on", @"off"]] containsObject:[arg lowercaseString]]) {
            if (error) *error = [NSString stringWithFormat:@"line %lu: diagnostic-log expects on or off", (unsigned long)i + 1]; return nil;
        }
        [result addObject:@{ @"op": op, @"arg": arg, @"line": @(i + 1) }];
    }
    return result;
}

- (void)start {
    NSError *readError = nil;
    NSString *text = [NSString stringWithContentsOfFile:self.scriptPath encoding:NSUTF8StringEncoding error:&readError];
    if (!text) { NSLog(@"[TEST] ERROR: %@", [readError localizedDescription]); [NSApp terminate:nil]; return; }
    NSString *base = [self.scriptPath stringByDeletingLastPathComponent];
    self.screenshotsDirectory = [base stringByAppendingPathComponent:@"screenshots"];
    self.resultsPath = [base stringByAppendingPathComponent:@"test-results.log"];
    [self.view setPlasmaProfilePath:[base stringByAppendingPathComponent:@"plasma-profile.log"]];

    NSError *directoryError = nil;
    if (![[NSFileManager defaultManager] createDirectoryAtPath:self.screenshotsDirectory
                                  withIntermediateDirectories:YES
                                                   attributes:nil
                                                        error:&directoryError]) {
        NSLog(@"[TEST] ERROR: cannot create screenshots directory: %@", [directoryError localizedDescription]);
        [NSApp terminate:nil];
        return;
    }
    [[NSData data] writeToFile:self.resultsPath atomically:YES];
    NSString *parseError = nil; self.commands = [self parse:text error:&parseError];
    if (!self.commands) { [self fail:parseError line:@0]; return; }
    self.index = 0; [self log:[NSString stringWithFormat:@"START %@ (%lu commands)", self.scriptPath, (unsigned long)[self.commands count]]];
    [self runNext];
}

- (double)secondsForWait:(NSString *)value valid:(BOOL *)valid {
    NSString *v = [value lowercaseString]; double factor = 1.0;
    if ([v hasSuffix:@"ms"]) { factor = 0.001; v = [v substringToIndex:[v length] - 2]; }
    else if ([v hasSuffix:@"s"]) v = [v substringToIndex:[v length] - 1];
    NSScanner *scanner = [NSScanner scannerWithString:v]; double number = 0.0;
    *valid = [scanner scanDouble:&number] && [scanner isAtEnd] && number >= 0.0;
    return number * factor;
}

- (void)runNext {
    if (self.index >= [self.commands count]) { [self log:@"COMPLETE"]; return; }
    NSDictionary *cmd = self.commands[self.index++]; NSString *op = cmd[@"op"], *arg = cmd[@"arg"]; NSNumber *line = cmd[@"line"];
    if ([op isEqualToString:@"wait"]) {
        BOOL valid = NO; double seconds = [self secondsForWait:arg valid:&valid];
        if (!valid) { [self fail:[NSString stringWithFormat:@"invalid wait value: %@", arg] line:line]; return; }
        [self log:[NSString stringWithFormat:@"line %@ wait %@", line, arg]];
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(seconds * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{ [self runNext]; }); return;
    }
    NSString *error = nil;
    if ([op isEqualToString:@"send"]) {
        if (![self.view sendTestText:arg error:&error]) { [self fail:error line:line]; return; }
        [self log:[NSString stringWithFormat:@"line %@ send %@", line, arg]];
    } else if ([op isEqualToString:@"send-env"]) {
        NSString *value = [[[NSProcessInfo processInfo] environment] objectForKey:arg];
        if (!value) { [self fail:[NSString stringWithFormat:@"environment variable not found: %@", arg] line:line]; return; }
        if (![self.view sendTestText:value error:&error]) { [self fail:error line:line]; return; }
        [self log:[NSString stringWithFormat:@"line %@ send-env %@ [REDACTED]", line, arg]];
    } else if ([op isEqualToString:@"key"]) {
        if (![self.view sendTestKey:arg error:&error]) { [self fail:error line:line]; return; }
        [self log:[NSString stringWithFormat:@"line %@ key %@", line, arg]];
    } else if ([op isEqualToString:@"renderer"]) {
        NSString *mode = [arg lowercaseString];
        if ([mode isEqualToString:@"crisp"]) [self.view setDisplayCrisp];
        else if ([mode isEqualToString:@"plasma"] || [mode isEqualToString:@"real-plasma"]) [self.view setDisplayRealPlasma];
        else if ([mode isEqualToString:@"split"]) [self.view setDisplaySplit];
        else { [self fail:[NSString stringWithFormat:@"unknown renderer: %@", arg] line:line]; return; }
        [self log:[NSString stringWithFormat:@"line %@ renderer %@", line, arg]];
    } else if ([op isEqualToString:@"diagnostic-log"]) {
        BOOL enabled = [[arg lowercaseString] isEqualToString:@"on"];
        [self.view setDiagnosticLogEnabled:enabled];
        [self log:[NSString stringWithFormat:@"line %@ diagnostic-log %@", line, enabled ? @"on" : @"off"]];
    } else if ([op isEqualToString:@"plasma-log"]) {
        [self.view writePlasmaProfileWithTag:arg];
        [self log:[NSString stringWithFormat:@"line %@ plasma-log %@", line, arg]];
    } else if ([op isEqualToString:@"screenshot"]) {
        NSString *name = [arg lastPathComponent];
        if (![[[name pathExtension] lowercaseString] isEqualToString:@"png"]) name = [name stringByAppendingPathExtension:@"png"];
        NSString *path = [self.screenshotsDirectory stringByAppendingPathComponent:name];
        if (![self.view saveRenderedScreenshot:path error:&error]) { [self fail:error line:line]; return; }
        [self log:[NSString stringWithFormat:@"line %@ screenshot %@", line, path]];
    } else if ([op isEqualToString:@"quit"]) {
        [self log:[NSString stringWithFormat:@"line %@ quit", line]]; [NSApp terminate:nil]; return;
    }
    dispatch_async(dispatch_get_main_queue(), ^{ [self runNext]; });
}

@end
