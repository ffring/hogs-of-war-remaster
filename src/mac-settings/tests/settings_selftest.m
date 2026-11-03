#import <Foundation/Foundation.h>
#import "../source/settings_store.h"
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

#define CHECK(expr, label) do { if (!(expr)) { \
    fprintf(stderr, "FAIL: %s (line %d)\n", label, __LINE__); return 1; \
} } while (0)

static NSData *Bytes(NSString *s) { return [s dataUsingEncoding:NSUTF8StringEncoding]; }
static NSString *MakeTempDirectory(void) {
    NSString *base = [NSTemporaryDirectory() stringByAppendingPathComponent:@"hogs-settings-selftest-XXXXXX"];
    char *buffer = strdup(base.fileSystemRepresentation);
    if (!buffer) return nil;
    char *made = mkdtemp(buffer);
    char resolved[PATH_MAX];
    NSString *result = made && realpath(made, resolved) ? [NSString stringWithUTF8String:resolved] : nil;
    free(buffer);
    return result;
}
static BOOL WriteFile(NSString *path, NSData *data) {
    return [data writeToFile:path options:NSDataWritingAtomic error:nil];
}
static BOOL SameSettings(HogsMousePanelSettings a, HogsMousePanelSettings b) {
    return a.nativeUnitsPerPixel == b.nativeUnitsPerPixel &&
        a.aimSensitivityPercent == b.aimSensitivityPercent &&
        a.invertY == b.invertY && a.invertX == b.invertX;
}

int HogsSettingsSelfTest(void) {
    @autoreleasepool {
        NSError *error = nil;
        NSString *temp = MakeTempDirectory();
        CHECK(temp != nil, "create temporary test root");
        NSString *game = [temp stringByAppendingPathComponent:@"game"];
        CHECK([[NSFileManager defaultManager] createDirectoryAtPath:game withIntermediateDirectories:NO attributes:nil error:&error], "create isolated game directory");
        NSString *ini = [game stringByAppendingPathComponent:@"HogsMouse.ini"];

        NSString *original = @"; Keep this comment\n[Video]\nMode=windowed\n[Mouse]\n  Enable = 1 ; keep inline note\nNativeUnitsPerPixel = 8 ; turn gain\nAimSensitivityPercent=250\nInvertY=0\n# Preserve this line\n[Audio]\nVolume=7\n";
        CHECK(WriteFile(ini, Bytes(original)), "write fixture INI");
        HogsMousePanelSettings settings;
        if (!HogsMousePanelReadForTest(game, &settings, &error)) {
            fprintf(stderr, "Read fixture error: %s\n", error.localizedDescription.UTF8String ?: "unknown");
            CHECK(NO, "read existing settings");
        }
        CHECK(settings.nativeUnitsPerPixel == 8 && settings.aimSensitivityPercent == 250 && !settings.invertY && settings.invertX, "parse known values and default missing horizontal inversion");
        settings.nativeUnitsPerPixel = 12; settings.aimSensitivityPercent = 300;
        settings.invertY = YES; settings.invertX = YES;
        CHECK(HogsMousePanelWriteForTest(game, settings, NO, &error), "write updated settings atomically");
        HogsMousePanelSettings readback;
        CHECK(HogsMousePanelReadForTest(game, &readback, &error) && SameSettings(settings, readback), "read back exact values");
        NSString *updated = [NSString stringWithContentsOfFile:ini encoding:NSUTF8StringEncoding error:nil];
        CHECK([updated containsString:@"; Keep this comment\n"] && [updated containsString:@"Mode=windowed\n"] &&
              [updated containsString:@"  Enable = 1 ; keep inline note\n"] &&
              [updated containsString:@"NativeUnitsPerPixel = 12 ; turn gain\n"] &&
              [updated containsString:@"# Preserve this line\n"] && [updated containsString:@"Volume=7\n"] &&
              [updated containsString:@"InvertX=1\n"], "preserve comments, unknown keys, sections, and inline notes");
        puts("PASS settings round-trip preserves unrelated keys and comments");

        NSString *newGame = [temp stringByAppendingPathComponent:@"new-game"];
        CHECK([[NSFileManager defaultManager] createDirectoryAtPath:newGame withIntermediateDirectories:NO attributes:nil error:&error], "create missing-settings fixture");
        CHECK(HogsMousePanelReadForTest(newGame, &readback, &error) &&
              readback.nativeUnitsPerPixel == 12 && readback.aimSensitivityPercent == 300 && readback.invertY && readback.invertX,
              "use safe defaults for absent keys");
        CHECK(HogsMousePanelWriteForTest(newGame, settings, NO, &error), "create missing INI and settings section");
        NSString *newINI = [NSString stringWithContentsOfFile:[newGame stringByAppendingPathComponent:@"HogsMouse.ini"] encoding:NSUTF8StringEncoding error:nil];
        CHECK([newINI containsString:@"[Mouse]\nNativeUnitsPerPixel=12\nAimSensitivityPercent=300\nInvertY=1\nInvertX=1\n"], "append standard Mouse settings");
        puts("PASS absent INI uses 12/300/1/1 defaults and can be created");

        NSString *tailGame = [temp stringByAppendingPathComponent:@"tail-game"];
        CHECK([[NSFileManager defaultManager] createDirectoryAtPath:tailGame withIntermediateDirectories:NO attributes:nil error:&error], "create unterminated-file fixture");
        NSString *tailINI = [tailGame stringByAppendingPathComponent:@"HogsMouse.ini"];
        CHECK(WriteFile(tailINI, Bytes(@"[Mouse]\nEnable=1")), "write INI without final newline");
        CHECK(HogsMousePanelWriteForTest(tailGame, settings, 0, &error), "update unterminated INI");
        NSString *tailUpdated = [NSString stringWithContentsOfFile:tailINI encoding:NSUTF8StringEncoding error:nil];
        CHECK([tailUpdated containsString:@"Enable=1\nNativeUnitsPerPixel=12\n"], "insert missing keys on separate lines");
        puts("PASS missing keys append safely when source has no final newline");

        NSData *beforeFailure = [NSData dataWithContentsOfFile:ini];
        NSData *malformed = Bytes(@"[Mouse\nNativeUnitsPerPixel=12\n");
        CHECK(WriteFile(ini, malformed), "write malformed fixture");
        CHECK(!HogsMousePanelWriteForTest(game, settings, NO, &error), "refuse malformed INI");
        CHECK([[NSData dataWithContentsOfFile:ini] isEqualToData:malformed], "malformed file remains untouched");
        CHECK(WriteFile(ini, beforeFailure), "restore fixture bytes");
        puts("PASS malformed settings are rejected without overwrite");

        beforeFailure = [NSData dataWithContentsOfFile:ini];
        CHECK(!HogsMousePanelWriteForTest(game, settings, 1, &error), "simulate failure before atomic rename");
        CHECK([[NSData dataWithContentsOfFile:ini] isEqualToData:beforeFailure], "failed atomic write leaves prior settings intact");
        NSArray *files = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:game error:nil];
        for (NSString *name in files) CHECK(![name hasSuffix:@".tmp"], "temporary write cleaned up after failure");
        puts("PASS pre-commit failure rolls back and removes temporary file");

        HogsMousePanelSettings changed = settings; changed.nativeUnitsPerPixel = 4; changed.invertX = NO;
        CHECK(!HogsMousePanelWriteForTest(game, changed, 2, &error), "simulate failure after rename");
        CHECK([[NSData dataWithContentsOfFile:ini] isEqualToData:beforeFailure], "post-rename failure restores exact prior bytes");
        puts("PASS post-rename verification failure atomically restores prior INI bytes");

        NSString *external = [temp stringByAppendingPathComponent:@"external.ini"];
        NSData *externalBefore = Bytes(@"outside data\n");
        CHECK(WriteFile(external, externalBefore), "write symlink target fixture");
        CHECK([[NSFileManager defaultManager] removeItemAtPath:ini error:&error], "remove ordinary INI");
        CHECK(symlink(external.fileSystemRepresentation, ini.fileSystemRepresentation) == 0, "create INI symlink fixture");
        CHECK(!HogsMousePanelWriteForTest(game, settings, NO, &error), "refuse target symlink");
        CHECK([[NSData dataWithContentsOfFile:external] isEqualToData:externalBefore], "do not alter symlink target");
        CHECK([[[NSFileManager defaultManager] attributesOfItemAtPath:ini error:nil][NSFileType] isEqualToString:NSFileTypeSymbolicLink], "leave symlink untouched");
        puts("PASS symlink target is refused and external file remains unchanged");

        NSString *rootLink = [temp stringByAppendingPathComponent:@"linked-game"];
        CHECK(symlink(newGame.fileSystemRepresentation, rootLink.fileSystemRepresentation) == 0, "create symlinked-root fixture");
        CHECK(!HogsMousePanelReadForTest(rootLink, &readback, &error), "refuse symlinked game root");
        puts("PASS symlinked game root is refused");

        CHECK([HogsMousePanelProductionGameRoot() hasSuffix:@"Library/Application Support/Hogs of War/prefix/drive_c/HogsofWar"], "production target is fixed to game folder");
        puts("PASS production store targets only the fixed game INI path");
        [[NSFileManager defaultManager] removeItemAtPath:temp error:nil];
        return 0;
    }
}
