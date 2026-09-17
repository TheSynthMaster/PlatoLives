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
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 100 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
        [NSApp terminate:nil];
        exit(1);
    });
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
        NSDictionary *aliases = @{
            @"invia": @"send", @"attendi": @"wait", @"tasto": @"key",
            @"salva-screen": @"screenshot", @"schermata": @"screenshot",
            @"copy_all": @"copyall", @"copyall": @"copyall", @"copy-all": @"copyall",
            @"copy_area": @"copyarea", @"copyarea": @"copyarea", @"copy-area": @"copyarea",
            @"persistenza": @"persistence", @"distorsione": @"distortion",
            @"schermo": @"display", @"video": @"display", @"headless": @"display"
        };
        if (aliases[op]) op = aliases[op];
        NSSet *valid = [NSSet setWithArray:@[@"send", @"send-env", @"key", @"wait", @"screenshot", @"renderer", @"diagnostic-log", @"plasma-log", @"quit", @"copyall", @"copyarea", @"persistence", @"distortion", @"beam", @"display"]];
        if (![valid containsObject:op]) { if (error) *error = [NSString stringWithFormat:@"line %lu: unknown command '%@'", (unsigned long)i + 1, op]; return nil; }
        if (![op isEqualToString:@"quit"] && ![op isEqualToString:@"copyall"] && [arg length] == 0) { if (error) *error = [NSString stringWithFormat:@"line %lu: missing argument", (unsigned long)i + 1]; return nil; }
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
        if ([mode isEqualToString:@"none"] || [mode isEqualToString:@"off"] || [mode isEqualToString:@"headless"]) {
            [self.view setDisplayNone];
        } else if ([mode isEqualToString:@"crisp"] || [mode isEqualToString:@"crisp-mono"]) {
            [self.view setDisplayCrisp];
        } else if ([mode isEqualToString:@"color"] || [mode isEqualToString:@"crisp-color"]) {
            [self.view setDisplayCrispColor];
        } else if ([mode isEqualToString:@"plasma"] || [mode isEqualToString:@"real-plasma"]) {
            [self.view setDisplayRealPlasma];
        } else if ([mode isEqualToString:@"crt"] || [mode isEqualToString:@"real-color-crt"] || [mode isEqualToString:@"real-crt"]) {
            [self.view setDisplayRealColorCRT];
        } else if ([mode isEqualToString:@"split"]) {
            [self.view setDisplaySplit];
        } else {
            [self fail:[NSString stringWithFormat:@"unknown renderer: %@", arg] line:line]; return;
        }
        [self log:[NSString stringWithFormat:@"line %@ renderer %@", line, arg]];
    } else if ([op isEqualToString:@"persistence"]) {
        NSString *v = [arg lowercaseString];
        double factor = 1.0;
        if ([v hasSuffix:@"ms"]) { factor = 0.001; v = [v substringToIndex:[v length] - 2]; }
        else if ([v hasSuffix:@"s"]) { factor = 1.0; v = [v substringToIndex:[v length] - 1]; }
        else if ([v doubleValue] > 10.0) { factor = 0.001; }
        double dur = [v doubleValue] * factor;
        if (dur < 0.02) dur = 0.02;
        if (dur > 5.00) dur = 5.00;
        [self.view setPlasmaDecayDuration:dur];
        [self.view setCRTDecayDuration:dur];
        [self log:[NSString stringWithFormat:@"line %@ persistence %.3fs (%d ms)", line, dur, (int)(dur * 1000 + 0.5)]];
    } else if ([op isEqualToString:@"distortion"]) {
        NSString *v = [arg lowercaseString];
        NSInteger dist = 2; // Default Cylindrical
        if ([v isEqualToString:@"none"] || [v isEqualToString:@"0"] || [v isEqualToString:@"flat"]) dist = 0;
        else if ([v isEqualToString:@"barrel"] || [v isEqualToString:@"1"] || [v isEqualToString:@"curved"]) dist = 1;
        else if ([v isEqualToString:@"cylindrical"] || [v isEqualToString:@"2"] || [v isEqualToString:@"cylinder"]) dist = 2;
        [self.view setPlasmaDistortion:dist];
        [self.view setCRTDistortion:dist];
        [self log:[NSString stringWithFormat:@"line %@ distortion %ld (%@)", line, (long)dist, (dist == 0 ? @"None" : (dist == 1 ? @"Barrel" : @"Cylindrical"))]];
    } else if ([op isEqualToString:@"beam"]) {
        NSString *v = [arg lowercaseString];
        NSInteger level = 1; // Default High
        if ([v isEqualToString:@"medium"] || [v isEqualToString:@"standard"] || [v isEqualToString:@"0"]) level = 0;
        else if ([v isEqualToString:@"high"] || [v isEqualToString:@"1"]) level = 1;
        else if ([v isEqualToString:@"ultra"] || [v isEqualToString:@"2"]) level = 2;
        [self.view setCRTBeamLevel:level];
        [self log:[NSString stringWithFormat:@"line %@ beam %ld", line, (long)level]];
    } else if ([op isEqualToString:@"display"]) {
        NSString *v = [arg lowercaseString];
        if ([v isEqualToString:@"off"] || [v isEqualToString:@"none"] || [v isEqualToString:@"headless"]) {
            [self.view setDisplayNone];
            [self log:[NSString stringWithFormat:@"line %@ display off (headless)", line]];
        } else if ([v isEqualToString:@"on"]) {
            self.view->graphicsDisabled = NO;
            if (self.view->plasmaLayer) [self.view->plasmaLayer setHidden:NO];
            if (self.view->overlayLayer) [self.view->overlayLayer setHidden:NO];
            [self log:[NSString stringWithFormat:@"line %@ display on", line]];
        }
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
    } else if ([op isEqualToString:@"copyall"]) {
        NSArray<NSString *> *tokens = [arg componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
        BOOL compact = NO;
        NSMutableArray<NSString *> *cleanTokens = [NSMutableArray array];
        for (NSString *t in tokens) {
            if ([t length] == 0) continue;
            if ([[t lowercaseString] isEqualToString:@"compact"] || [[t lowercaseString] hasPrefix:@"compact=true"]) {
                compact = YES;
            } else if ([[t lowercaseString] isEqualToString:@"verbatim"] || [[t lowercaseString] hasPrefix:@"compact=false"]) {
                compact = NO;
            } else {
                [cleanTokens addObject:t];
            }
        }
        NSString *dest = [cleanTokens componentsJoinedByString:@" "];
        NSString *dLower = [dest lowercaseString];
        if ([dLower isEqualToString:@"console"] || [dLower isEqualToString:@"stdout"]) {
            NSString *text = [self.view extractAllTextCompact:compact];
            printf("\n%s\n", [text UTF8String]);
            fflush(stdout);
            [self log:[NSString stringWithFormat:@"line %@ copy-all (compact=%@) -> console", line, compact ? @"true" : @"false"]];
        } else if ([dest length] == 0 || [dLower isEqualToString:@"clipboard"]) {
            [self.view copyTextToPasteboardCompact:compact];
            [self log:[NSString stringWithFormat:@"line %@ copy-all (compact=%@) -> clipboard", line, compact ? @"true" : @"false"]];
        } else {
            NSString *path = dest;
            if ([path hasPrefix:@"file:"]) path = [path substringFromIndex:5];
            if (![path isAbsolutePath]) path = [[self.screenshotsDirectory stringByDeletingLastPathComponent] stringByAppendingPathComponent:path];
            NSString *saveErr = nil;
            if (![self.view saveAllTextToFile:path compact:compact error:&saveErr]) {
                [self fail:[NSString stringWithFormat:@"copy-all failed to save: %@", saveErr] line:line]; return;
            }
            [self log:[NSString stringWithFormat:@"line %@ copy-all (compact=%@) -> %@", line, compact ? @"true" : @"false", path]];
        }
    } else if ([op isEqualToString:@"copyarea"]) {
        NSArray<NSString *> *tokens = [arg componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
        NSMutableArray<NSString *> *cleanTokens = [NSMutableArray array];
        for (NSString *t in tokens) {
            if ([t length] > 0) [cleanTokens addObject:t];
        }
        if ([cleanTokens count] < 4) {
            [self fail:@"copy-area requires 4 coordinates: x1 y1 x2 y2" line:line]; return;
        }
        int x1 = [cleanTokens[0] intValue];
        int y1 = [cleanTokens[1] intValue];
        int x2 = [cleanTokens[2] intValue];
        int y2 = [cleanTokens[3] intValue];

        int c1 = x1, r1 = y1, c2 = x2, r2 = y2;
        if (x1 > 63 || x2 > 63 || y1 > 31 || y2 > 31) {
            c1 = (x1 < 0) ? 0 : (x1 > 511 ? 63 : x1 / 8);
            r1 = (y1 < 0) ? 31 : (y1 > 511 ? 0 : 31 - (y1 / 16));
            c2 = (x2 < 0) ? 0 : (x2 > 511 ? 63 : x2 / 8);
            r2 = (y2 < 0) ? 31 : (y2 > 511 ? 0 : 31 - (y2 / 16));
        }

        BOOL compact = NO;
        NSString *dest = @"clipboard";
        for (NSUInteger k = 4; k < [cleanTokens count]; k++) {
            NSString *tok = cleanTokens[k];
            if ([[tok lowercaseString] isEqualToString:@"compact"]) {
                compact = YES;
            } else if ([[tok lowercaseString] isEqualToString:@"verbatim"]) {
                compact = NO;
            } else if ([[tok lowercaseString] isEqualToString:@"clipboard"]) {
                dest = @"clipboard";
            } else {
                dest = tok;
            }
        }

        NSString *dLower = [dest lowercaseString];
        if ([dLower isEqualToString:@"console"] || [dLower isEqualToString:@"stdout"]) {
            NSString *extracted = [self.view extractTextFromCol:c1 row:r1 toCol:c2 row:r2 compact:compact];
            printf("\n%s\n", [extracted UTF8String]);
            fflush(stdout);
            [self log:[NSString stringWithFormat:@"line %@ copy-area [%d,%d..%d,%d] (compact=%@) -> console", line, c1, r1, c2, r2, compact ? @"true" : @"false"]];
        } else if ([dest length] == 0 || [dLower isEqualToString:@"clipboard"]) {
            NSString *extracted = [self.view extractTextFromCol:c1 row:r1 toCol:c2 row:r2 compact:compact];
            if (extracted) {
                NSPasteboard *pb = [NSPasteboard generalPasteboard];
                [pb clearContents];
                [pb setString:extracted forType:NSPasteboardTypeString];
            }
            [self log:[NSString stringWithFormat:@"line %@ copy-area [%d,%d..%d,%d] (compact=%@) -> clipboard", line, c1, r1, c2, r2, compact ? @"true" : @"false"]];
        } else {
            NSString *path = dest;
            if ([path hasPrefix:@"file:"]) path = [path substringFromIndex:5];
            if (![path isAbsolutePath]) path = [[self.screenshotsDirectory stringByDeletingLastPathComponent] stringByAppendingPathComponent:path];
            NSString *saveErr = nil;
            if (![self.view saveTextToFile:path fromCol:c1 row:r1 toCol:c2 row:r2 compact:compact error:&saveErr]) {
                [self fail:[NSString stringWithFormat:@"copy-area failed to save: %@", saveErr] line:line]; return;
            }
            [self log:[NSString stringWithFormat:@"line %@ copy-area [%d,%d..%d,%d] (compact=%@) -> %@", line, c1, r1, c2, r2, compact ? @"true" : @"false", path]];
        }
    } else if ([op isEqualToString:@"quit"]) {
        [self log:[NSString stringWithFormat:@"line %@ quit", line]];
        [NSApp terminate:nil];
        exit(0);
        return;
    }
    dispatch_async(dispatch_get_main_queue(), ^{ [self runNext]; });
}

@end
