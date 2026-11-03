#import "settings_store.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static NSString * const HogsPanelErrorDomain = @"HogsMouseSettingsPanel";
enum { HogsPanelErrorInvalid = 1, HogsPanelErrorPath = 2, HogsPanelErrorIO = 3 };

HogsMousePanelSettings HogsMousePanelDefaults(void) {
    return (HogsMousePanelSettings){ .nativeUnitsPerPixel = 12,
        .aimSensitivityPercent = 300, .invertY = YES, .invertX = YES };
}

NSString *HogsMousePanelProductionGameRoot(void) {
    return [NSHomeDirectory() stringByAppendingPathComponent:
        @"Library/Application Support/Hogs of War/prefix/drive_c/HogsofWar"];
}

static void SetError(NSError **error, NSInteger code, NSString *message) {
    if (error) *error = [NSError errorWithDomain:HogsPanelErrorDomain code:code
        userInfo:@{NSLocalizedDescriptionKey: message ?: @"Не удалось прочитать настройки."}];
}

static BOOL IsWhitespace(unichar c) {
    return [[NSCharacterSet whitespaceCharacterSet] characterIsMember:c];
}

static BOOL IsTargetKey(NSString *key) {
    NSString *lower = key.lowercaseString;
    return [lower isEqualToString:@"nativeunitsperpixel"] ||
        [lower isEqualToString:@"aimsensitivitypercent"] ||
        [lower isEqualToString:@"inverty"] || [lower isEqualToString:@"invertx"];
}

static NSArray<NSString *> *SplitRecords(NSString *text) {
    NSMutableArray<NSString *> *records = [NSMutableArray array];
    NSUInteger start = 0, length = text.length;
    for (NSUInteger i = 0; i < length; ++i) {
        unichar c = [text characterAtIndex:i];
        if (c != '\r' && c != '\n') continue;
        NSUInteger end = i + 1;
        if (c == '\r' && end < length && [text characterAtIndex:end] == '\n') ++end;
        [records addObject:[text substringWithRange:NSMakeRange(start, end - start)]];
        i = end - 1; start = end;
    }
    if (start < length) [records addObject:[text substringFromIndex:start]];
    return records;
}

static NSString *BodyOfRecord(NSString *record, NSString **ending) {
    NSUInteger n = record.length;
    if (n >= 2 && [record characterAtIndex:n - 2] == '\r' &&
        [record characterAtIndex:n - 1] == '\n') {
        if (ending) *ending = @"\r\n";
        return [record substringToIndex:n - 2];
    }
    if (n && ([record characterAtIndex:n - 1] == '\r' || [record characterAtIndex:n - 1] == '\n')) {
        if (ending) *ending = [record substringFromIndex:n - 1];
        return [record substringToIndex:n - 1];
    }
    if (ending) *ending = @"";
    return record;
}

static NSString *Trim(NSString *value) {
    return [value stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
}

static NSString *ValueWithoutComment(NSString *raw) {
    NSUInteger n = raw.length;
    for (NSUInteger i = 1; i < n; ++i) {
        unichar c = [raw characterAtIndex:i];
        if ((c == ';' || c == '#') && IsWhitespace([raw characterAtIndex:i - 1]))
            return Trim([raw substringToIndex:i]);
    }
    return Trim(raw);
}

static BOOL ParseInteger(NSString *value, NSInteger *result) {
    NSString *clean = ValueWithoutComment(value);
    if (!clean.length) return NO;
    NSScanner *scanner = [NSScanner scannerWithString:clean];
    NSInteger parsed = 0;
    if (![scanner scanInteger:&parsed] || !scanner.isAtEnd) return NO;
    if (result) *result = parsed;
    return YES;
}

static BOOL ParseINI(NSData *data, HogsMousePanelSettings *result, NSError **error) {
    HogsMousePanelSettings value = HogsMousePanelDefaults();
    NSString *text = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    if (!text) { SetError(error, HogsPanelErrorInvalid, @"Файл настроек повреждён. Исправьте его и повторите."); return NO; }

    NSMutableSet<NSString *> *sections = [NSMutableSet set];
    NSMutableSet<NSString *> *targetKeys = [NSMutableSet set];
    NSString *section = @"";
    NSArray<NSString *> *records = SplitRecords(text);
    for (NSString *record in records) {
        NSString *body = BodyOfRecord(record, NULL);
        NSString *trimmed = Trim(body);
        if (!trimmed.length || [trimmed hasPrefix:@";"] || [trimmed hasPrefix:@"#"]) continue;
        if ([trimmed hasPrefix:@"["]) {
            if (![trimmed hasSuffix:@"]"] || trimmed.length < 3 ||
                [[trimmed substringToIndex:trimmed.length - 1] containsString:@"]"]) {
                SetError(error, HogsPanelErrorInvalid, @"Файл настроек повреждён. Исправьте его и повторите."); return NO;
            }
            NSString *name = Trim([trimmed substringWithRange:NSMakeRange(1, trimmed.length - 2)]);
            if (!name.length || [name containsString:@"["] || [sections containsObject:name.lowercaseString]) {
                SetError(error, HogsPanelErrorInvalid, @"Файл настроек повреждён. Исправьте его и повторите."); return NO;
            }
            section = name.lowercaseString;
            [sections addObject:section];
            continue;
        }
        NSRange equal = [body rangeOfString:@"="];
        if (equal.location == NSNotFound || !Trim([body substringToIndex:equal.location]).length || !section.length) {
            SetError(error, HogsPanelErrorInvalid, @"Файл настроек повреждён. Исправьте его и повторите."); return NO;
        }
        NSString *key = Trim([body substringToIndex:equal.location]);
        NSString *lower = key.lowercaseString;
        if (![section isEqualToString:@"mouse"] || !IsTargetKey(key)) continue;
        if ([targetKeys containsObject:lower]) {
            SetError(error, HogsPanelErrorInvalid, @"В файле повторяются настройки мыши. Исправьте файл и повторите."); return NO;
        }
        [targetKeys addObject:lower];
        NSInteger parsed = 0;
        if (!ParseInteger([body substringFromIndex:equal.location + 1], &parsed)) {
            SetError(error, HogsPanelErrorInvalid, @"Значение настройки мыши не распознано. Исправьте файл и повторите."); return NO;
        }
        if ([lower isEqualToString:@"nativeunitsperpixel"]) value.nativeUnitsPerPixel = parsed;
        else if ([lower isEqualToString:@"aimsensitivitypercent"]) value.aimSensitivityPercent = parsed;
        else if ([lower isEqualToString:@"inverty"]) {
            if (parsed != 0 && parsed != 1) { SetError(error, HogsPanelErrorInvalid, @"Значение настройки должно быть 0 или 1."); return NO; }
            value.invertY = parsed == 1;
        } else if ([lower isEqualToString:@"invertx"]) {
            if (parsed != 0 && parsed != 1) { SetError(error, HogsPanelErrorInvalid, @"Значение настройки должно быть 0 или 1."); return NO; }
            value.invertX = parsed == 1;
        }
    }
    if (value.nativeUnitsPerPixel < 1 || value.nativeUnitsPerPixel > 24 ||
        value.aimSensitivityPercent < 25 || value.aimSensitivityPercent > 600 ||
        (value.invertY != NO && value.invertY != YES) || (value.invertX != NO && value.invertX != YES)) {
        SetError(error, HogsPanelErrorInvalid, @"Значение настройки вне допустимого диапазона. Исправьте файл и повторите."); return NO;
    }
    if (result) *result = value;
    return YES;
}

static NSString *DetectNewline(NSArray<NSString *> *records) {
    for (NSString *record in records) {
        NSString *ending = nil; (void)BodyOfRecord(record, &ending);
        if (ending.length) return ending;
    }
    return @"\n";
}

static NSString *ReplaceValue(NSString *record, NSString *newValue) {
    NSString *ending = nil;
    NSString *body = BodyOfRecord(record, &ending);
    NSRange eq = [body rangeOfString:@"="];
    if (eq.location == NSNotFound) return record;
    NSString *after = [body substringFromIndex:eq.location + 1];
    NSUInteger lead = 0;
    while (lead < after.length && IsWhitespace([after characterAtIndex:lead])) ++lead;
    NSUInteger suffix = after.length;
    /* Preserve trailing whitespace and a whitespace-delimited inline comment. */
    for (NSUInteger i = lead; i < after.length; ++i) {
        unichar c = [after characterAtIndex:i];
        if ((c == ';' || c == '#') && i > 0 && IsWhitespace([after characterAtIndex:i - 1])) {
            suffix = i - 1; break;
        }
    }
    if (suffix == after.length) {
        while (suffix > lead && IsWhitespace([after characterAtIndex:suffix - 1])) --suffix;
    }
    NSString *leading = [after substringToIndex:lead];
    NSString *endingWhitespaceOrComment = [after substringFromIndex:suffix];
    return [[[body substringToIndex:eq.location + 1] stringByAppendingString:leading]
        stringByAppendingFormat:@"%@%@%@", newValue, endingWhitespaceOrComment, ending];
}

static NSString *SettingsValue(HogsMousePanelSettings s, NSString *key) {
    NSString *lower = key.lowercaseString;
    if ([lower isEqualToString:@"nativeunitsperpixel"]) return [NSString stringWithFormat:@"%ld", (long)s.nativeUnitsPerPixel];
    if ([lower isEqualToString:@"aimsensitivitypercent"]) return [NSString stringWithFormat:@"%ld", (long)s.aimSensitivityPercent];
    if ([lower isEqualToString:@"inverty"]) return s.invertY ? @"1" : @"0";
    return s.invertX ? @"1" : @"0";
}

static NSArray<NSString *> *TargetKeys(void) {
    return @[@"NativeUnitsPerPixel", @"AimSensitivityPercent", @"InvertY", @"InvertX"];
}

static NSString *AppendMissing(NSString *record, NSString *newline, NSArray<NSString *> *keys,
                               NSMutableSet<NSString *> *found, HogsMousePanelSettings settings,
                               BOOL ensureLeadingNewline) {
    NSMutableString *result = [NSMutableString stringWithString:record ?: @""];
    if (ensureLeadingNewline && result.length) {
        unichar last = [result characterAtIndex:result.length - 1];
        if (last != '\r' && last != '\n') [result appendString:newline];
    }
    for (NSString *key in keys) {
        if ([found containsObject:key.lowercaseString]) continue;
        [result appendFormat:@"%@=%@%@", key, SettingsValue(settings, key), newline];
    }
    return result;
}

static NSData *RewriteINI(NSData *source, HogsMousePanelSettings settings, NSError **error) {
    if (settings.nativeUnitsPerPixel < 1 || settings.nativeUnitsPerPixel > 24 ||
        settings.aimSensitivityPercent < 25 || settings.aimSensitivityPercent > 600) {
        SetError(error, HogsPanelErrorInvalid, @"Значение настройки вне допустимого диапазона."); return nil;
    }
    HogsMousePanelSettings ignored;
    if (!ParseINI(source, &ignored, error)) return nil;
    NSString *text = [[NSString alloc] initWithData:source encoding:NSUTF8StringEncoding];
    NSArray<NSString *> *records = SplitRecords(text);
    NSString *newline = DetectNewline(records);
    NSArray<NSString *> *keys = TargetKeys();
    NSMutableSet<NSString *> *found = [NSMutableSet set];
    NSMutableArray<NSString *> *out = [NSMutableArray array];
    NSString *section = @"";
    BOOL sawMouse = NO;
    for (NSString *record in records) {
        NSString *body = BodyOfRecord(record, NULL);
        NSString *trimmed = Trim(body);
        BOOL isHeader = [trimmed hasPrefix:@"["];
        if (isHeader) {
            if ([section isEqualToString:@"mouse"]) {
                [out addObject:AppendMissing(@"", newline, keys, found, settings, NO)];
            }
            NSString *name = Trim([trimmed substringWithRange:NSMakeRange(1, trimmed.length - 2)]);
            section = name.lowercaseString;
            if ([section isEqualToString:@"mouse"]) { sawMouse = YES; [found removeAllObjects]; }
            [out addObject:record];
            continue;
        }
        if ([section isEqualToString:@"mouse"] && trimmed.length &&
            ![trimmed hasPrefix:@";"] && ![trimmed hasPrefix:@"#"]) {
            NSRange eq = [body rangeOfString:@"="];
            NSString *key = Trim([body substringToIndex:eq.location]);
            if (IsTargetKey(key)) {
                [found addObject:key.lowercaseString];
                [out addObject:ReplaceValue(record, SettingsValue(settings, key))];
                continue;
            }
        }
        [out addObject:record];
    }
    if ([section isEqualToString:@"mouse"]) {
        NSString *missing = AppendMissing(@"", newline, keys, found, settings, NO);
        if (missing.length) {
            if (out.count) {
                NSString *last = out.lastObject;
                unichar final = last.length ? [last characterAtIndex:last.length - 1] : '\n';
                if (last.length && final != '\r' && final != '\n')
                    out[out.count - 1] = [last stringByAppendingString:newline];
            }
            [out addObject:missing];
        }
    } else if (!sawMouse) {
        NSMutableString *textOut = [NSMutableString string];
        for (NSString *part in out) [textOut appendString:part];
        if (textOut.length && ![textOut hasSuffix:@"\n"] && ![textOut hasSuffix:@"\r"]) [textOut appendString:newline];
        [textOut appendFormat:@"[Mouse]%@", newline];
        [textOut appendString:AppendMissing(@"", newline, keys, [NSMutableSet set], settings, NO)];
        return [textOut dataUsingEncoding:NSUTF8StringEncoding];
    }
    NSMutableString *result = [NSMutableString string];
    for (NSString *part in out) [result appendString:part];
    return [result dataUsingEncoding:NSUTF8StringEncoding];
}

static BOOL ValidateRootPath(NSString *root, BOOL productionOnly, NSError **error) {
    char resolved[PATH_MAX];
    if (!root.isAbsolutePath || [[root pathComponents] containsObject:@".."] ||
        !realpath(root.fileSystemRepresentation, resolved) ||
        strcmp(resolved, root.fileSystemRepresentation) != 0) {
        SetError(error, HogsPanelErrorPath, @"Папка игры недоступна. Откройте игру через установленный лаунчер."); return NO;
    }
    if (productionOnly && ![root isEqualToString:HogsMousePanelProductionGameRoot()]) {
        SetError(error, HogsPanelErrorPath, @"Папка игры недоступна. Откройте игру через установленный лаунчер."); return NO;
    }
    NSArray<NSString *> *parts = [root pathComponents];
    NSString *walk = @"/";
    for (NSUInteger i = 1; i < parts.count; ++i) {
        walk = [walk stringByAppendingPathComponent:parts[i]];
        struct stat st;
        if (lstat(walk.fileSystemRepresentation, &st) != 0 || S_ISLNK(st.st_mode) ||
            (i + 1 < parts.count && !S_ISDIR(st.st_mode))) {
            SetError(error, HogsPanelErrorPath, @"Папка игры недоступна. Проверьте установку и повторите."); return NO;
        }
    }
    struct stat rootStat;
    if (lstat(root.fileSystemRepresentation, &rootStat) != 0 || !S_ISDIR(rootStat.st_mode) || S_ISLNK(rootStat.st_mode)) {
        SetError(error, HogsPanelErrorPath, @"Папка игры недоступна. Проверьте установку и повторите."); return NO;
    }
    return YES;
}

static int OpenGameDirectory(NSString *root, BOOL productionOnly, NSError **error) {
    if (!ValidateRootPath(root, productionOnly, error)) return -1;
    int fd = open(root.fileSystemRepresentation, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (fd < 0) SetError(error, HogsPanelErrorPath, @"Папка игры недоступна. Проверьте установку и повторите.");
    return fd;
}

static BOOL ReadINI(int dirfd, NSData **data, BOOL *missing, NSError **error) {
    int fd = openat(dirfd, "HogsMouse.ini", O_RDONLY | O_NOFOLLOW);
    if (fd < 0 && errno == ENOENT) { if (missing) *missing = YES; *data = [NSData data]; return YES; }
    if (fd < 0) { SetError(error, HogsPanelErrorPath, @"Не удалось прочитать настройки. Проверьте файл и повторите."); return NO; }
    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0 || st.st_size > 1024 * 1024) {
        close(fd); SetError(error, HogsPanelErrorInvalid, @"Файл настроек имеет неверный формат."); return NO;
    }
    NSMutableData *result = [NSMutableData dataWithCapacity:(NSUInteger)st.st_size];
    uint8_t buffer[8192];
    for (;;) {
        ssize_t got = read(fd, buffer, sizeof(buffer));
        if (got == 0) break;
        if (got < 0) { if (errno == EINTR) continue; close(fd); SetError(error, HogsPanelErrorIO, @"Не удалось прочитать настройки. Повторите попытку."); return NO; }
        [result appendBytes:buffer length:(NSUInteger)got];
        if (result.length > 1024 * 1024) { close(fd); SetError(error, HogsPanelErrorInvalid, @"Файл настроек слишком большой."); return NO; }
    }
    close(fd); if (missing) *missing = NO; *data = result; return YES;
}

static BOOL CommitBytes(int dirfd, NSData *bytes, BOOL *didRename) {
    if (didRename) *didRename = NO;
    char tempName[80];
    snprintf(tempName, sizeof(tempName), ".HogsMouse.ini.%ld.%08x.tmp", (long)getpid(), arc4random());
    int fd = openat(dirfd, tempName, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (fd < 0) return NO;
    BOOL ok = YES;
    const uint8_t *bytesPtr = bytes.bytes;
    NSUInteger remaining = bytes.length;
    while (remaining) {
        ssize_t wrote = write(fd, bytesPtr, remaining);
        if (wrote < 0 && errno == EINTR) continue;
        if (wrote <= 0) { ok = NO; break; }
        bytesPtr += wrote; remaining -= (NSUInteger)wrote;
    }
    if (ok && fsync(fd) != 0) ok = NO;
    if (close(fd) != 0) ok = NO;
    if (ok) {
        struct stat st;
        int statResult = fstatat(dirfd, "HogsMouse.ini", &st, AT_SYMLINK_NOFOLLOW);
        if (statResult == 0) {
            if (S_ISLNK(st.st_mode) || !S_ISREG(st.st_mode)) ok = NO;
        } else if (errno != ENOENT) ok = NO;
    }
    if (ok && renameat(dirfd, tempName, dirfd, "HogsMouse.ini") == 0) {
        if (didRename) *didRename = YES;
        if (fsync(dirfd) != 0) ok = NO;
    } else {
        ok = NO;
    }
    if (!ok && (!didRename || !*didRename)) unlinkat(dirfd, tempName, 0);
    return ok;
}

static BOOL RollbackCommittedWrite(int dirfd, NSData *newBytes, NSData *oldBytes, BOOL hadOldFile) {
    NSData *current = nil; BOOL missing = NO;
    if (!ReadINI(dirfd, &current, &missing, NULL) || missing || ![current isEqualToData:newBytes]) return NO;
    if (!hadOldFile) {
        if (unlinkat(dirfd, "HogsMouse.ini", 0) != 0) return NO;
        return fsync(dirfd) == 0;
    }
    BOOL didRename = NO;
    return CommitBytes(dirfd, oldBytes, &didRename) && didRename;
}

static BOOL AtomicWriteINI(int dirfd, NSData *bytes, NSData *oldBytes, BOOL hadOldFile,
                           NSInteger failureStage, NSError **error) {
    struct stat st;
    int statResult = fstatat(dirfd, "HogsMouse.ini", &st, AT_SYMLINK_NOFOLLOW);
    if (statResult == 0) {
        if (S_ISLNK(st.st_mode) || !S_ISREG(st.st_mode)) {
            SetError(error, HogsPanelErrorPath, @"Файл настроек недоступен. Проверьте файл и повторите."); return NO;
        }
    } else if (errno != ENOENT) {
        SetError(error, HogsPanelErrorIO, @"Не удалось сохранить настройки. Повторите попытку."); return NO;
    }
    if (failureStage == 1) {
        SetError(error, HogsPanelErrorIO, @"Не удалось сохранить настройки. Прежние значения сохранены."); return NO;
    }
    BOOL didRename = NO;
    BOOL committed = CommitBytes(dirfd, bytes, &didRename);
    if (failureStage == 2 && didRename) committed = NO;
    if (!committed) {
        BOOL rollbackOK = didRename && RollbackCommittedWrite(dirfd, bytes, oldBytes, hadOldFile);
        SetError(error, HogsPanelErrorIO, rollbackOK || !didRename ?
            @"Не удалось сохранить настройки. Прежние значения сохранены." :
            @"Не удалось проверить сохранённые настройки. Проверьте файл.");
        return NO;
    }
    NSData *readback = nil; BOOL missing = NO;
    if (!ReadINI(dirfd, &readback, &missing, error) || missing || ![readback isEqualToData:bytes]) {
        BOOL rollbackOK = RollbackCommittedWrite(dirfd, bytes, oldBytes, hadOldFile);
        SetError(error, HogsPanelErrorIO, rollbackOK ?
            @"Не удалось сохранить настройки. Прежние значения сохранены." :
            @"Не удалось проверить сохранённые настройки. Проверьте файл.");
        return NO;
    }
    return YES;
}

static BOOL ReadAtRoot(NSString *root, BOOL productionOnly, HogsMousePanelSettings *settings, NSError **error) {
    int dirfd = OpenGameDirectory(root, productionOnly, error);
    if (dirfd < 0) return NO;
    NSData *data = nil; BOOL missing = NO;
    BOOL ok = ReadINI(dirfd, &data, &missing, error);
    close(dirfd);
    if (!ok) return NO;
    if (missing) { if (settings) *settings = HogsMousePanelDefaults(); return YES; }
    return ParseINI(data, settings, error);
}

static BOOL WriteAtRoot(NSString *root, BOOL productionOnly, HogsMousePanelSettings settings,
                        NSInteger failureStage, NSError **error) {
    int dirfd = OpenGameDirectory(root, productionOnly, error);
    if (dirfd < 0) return NO;
    NSData *original = nil; BOOL missing = NO;
    if (!ReadINI(dirfd, &original, &missing, error)) { close(dirfd); return NO; }
    NSData *source = missing ? [NSData data] : original;
    NSData *updated = RewriteINI(source, settings, error);
    if (!updated) { close(dirfd); return NO; }
    BOOL ok = AtomicWriteINI(dirfd, updated, source, !missing, failureStage, error);
    close(dirfd);
    return ok;
}

BOOL HogsMousePanelRead(HogsMousePanelSettings *settings, NSError **error) {
    return ReadAtRoot(HogsMousePanelProductionGameRoot(), YES, settings, error);
}

BOOL HogsMousePanelWrite(HogsMousePanelSettings settings, NSError **error) {
    return WriteAtRoot(HogsMousePanelProductionGameRoot(), YES, settings, 0, error);
}

BOOL HogsMousePanelReadForTest(NSString *gameRoot, HogsMousePanelSettings *settings, NSError **error) {
    return ReadAtRoot(gameRoot, NO, settings, error);
}

BOOL HogsMousePanelWriteForTest(NSString *gameRoot, HogsMousePanelSettings settings,
                                NSInteger failureStage, NSError **error) {
    return WriteAtRoot(gameRoot, NO, settings, failureStage, error);
}
