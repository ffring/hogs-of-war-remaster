#import <AppKit/AppKit.h>
#import "settings_store.h"
#include <math.h>
#include <string.h>

extern int HogsSettingsSelfTest(void);

@interface HogsPanelController : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property(nonatomic, strong) NSWindow *window;
@property(nonatomic, strong) NSSlider *turnSlider;
@property(nonatomic, strong) NSSlider *aimSlider;
@property(nonatomic, strong) NSTextField *turnValue;
@property(nonatomic, strong) NSTextField *aimValue;
@property(nonatomic, strong) NSButton *invertY;
@property(nonatomic, strong) NSButton *invertX;
@property(nonatomic, strong) NSTextField *status;
@property(nonatomic) BOOL canApply;
- (void)buildWindowWithSettings:(HogsMousePanelSettings)settings show:(BOOL)show;
- (BOOL)renderPreviewAtPath:(NSString *)path;
- (BOOL)renderAlertPreviewAtPath:(NSString *)path saving:(BOOL)saving;
@end

static NSTextField *PanelLabel(NSString *text, NSRect frame, CGFloat size, NSFontWeight weight) {
    NSTextField *field = [NSTextField labelWithString:text];
    field.frame = frame;
    field.font = [NSFont systemFontOfSize:size weight:weight];
    field.textColor = NSColor.labelColor;
    return field;
}

static NSAlert *PanelAlert(BOOL saving, NSString *detail) {
    NSAlert *alert = [[NSAlert alloc] init];
    alert.alertStyle = saving ? NSAlertStyleWarning : NSAlertStyleCritical;
    alert.messageText = saving ? @"Настройки не сохранены." : @"Не удалось открыть настройки мыши.";
    alert.informativeText = detail ?: (saving
        ? @"Проверьте файл и повторите попытку."
        : @"Проверьте установку игры и файл настроек, затем откройте панель ещё раз.");
    [alert addButtonWithTitle:@"Закрыть"];
    alert.window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    alert.window.backgroundColor = [NSColor colorWithWhite:0.95 alpha:1.0];
    alert.window.contentView.wantsLayer = YES;
    alert.window.contentView.layer.backgroundColor = [NSColor colorWithWhite:0.95 alpha:1.0].CGColor;
    return alert;
}

@implementation HogsPanelController

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    (void)notification;
    NSError *error = nil;
    HogsMousePanelSettings settings;
    if (!HogsMousePanelRead(&settings, &error)) {
        NSAlert *alert = PanelAlert(NO, nil);
        [alert runModal];
        [NSApp terminate:nil];
        return;
    }
    self.canApply = YES;
    [self buildWindowWithSettings:settings show:YES];
}

- (void)buildWindowWithSettings:(HogsMousePanelSettings)settings show:(BOOL)show {
    NSRect rect = NSMakeRect(0, 0, 600, 430);
    self.window = [[NSWindow alloc] initWithContentRect:rect
        styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable)
        backing:NSBackingStoreBuffered defer:NO];
    self.window.title = @"Мышь в Hogs of War";
    self.window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    self.window.opaque = YES;
    self.window.backgroundColor = [NSColor colorWithWhite:0.95 alpha:1.0];
    self.window.delegate = self;
    self.window.contentMinSize = rect.size;
    self.window.contentMaxSize = rect.size;

    NSView *content = self.window.contentView;
    content.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    content.wantsLayer = YES;
    content.layer.backgroundColor = [NSColor colorWithWhite:0.95 alpha:1.0].CGColor;
    [content addSubview:PanelLabel(@"Мышь в Hogs of War", NSMakeRect(34, 374, 520, 28), 20, NSFontWeightSemibold)];

    [content addSubview:PanelLabel(@"Поворот", NSMakeRect(36, 326, 220, 20), 14, NSFontWeightMedium)];
    self.turnSlider = [self sliderWithFrame:NSMakeRect(36, 292, 430, 28) minimum:0.25 maximum:6.0];
    self.turnSlider.doubleValue = (double)settings.nativeUnitsPerPixel / 4.0;
    self.turnValue = PanelLabel(@"", NSMakeRect(480, 294, 84, 24), 14, NSFontWeightRegular);
    self.turnValue.alignment = NSTextAlignmentRight;
    [content addSubview:self.turnSlider]; [content addSubview:self.turnValue];

    [content addSubview:PanelLabel(@"Прицел", NSMakeRect(36, 244, 220, 20), 14, NSFontWeightMedium)];
    self.aimSlider = [self sliderWithFrame:NSMakeRect(36, 210, 430, 28) minimum:0.25 maximum:6.0];
    self.aimSlider.doubleValue = (double)settings.aimSensitivityPercent / 100.0;
    self.aimValue = PanelLabel(@"", NSMakeRect(480, 212, 84, 24), 14, NSFontWeightRegular);
    self.aimValue.alignment = NSTextAlignmentRight;
    [content addSubview:self.aimSlider]; [content addSubview:self.aimValue];

    self.invertY = [NSButton checkboxWithTitle:@"Инверсия прицела по вертикали" target:nil action:nil];
    self.invertY.frame = NSMakeRect(36, 157, 340, 24);
    self.invertY.state = settings.invertY ? NSControlStateValueOn : NSControlStateValueOff;
    [content addSubview:self.invertY];
    self.invertX = [NSButton checkboxWithTitle:@"Инверсия прицела по горизонтали" target:nil action:nil];
    self.invertX.frame = NSMakeRect(36, 124, 340, 24);
    self.invertX.state = settings.invertX ? NSControlStateValueOn : NSControlStateValueOff;
    [content addSubview:self.invertX];

    [content addSubview:PanelLabel(@"Изменения применятся при возвращении в игру.", NSMakeRect(36, 76, 510, 20), 12, NSFontWeightRegular)];
    self.status = PanelLabel(@"", NSMakeRect(36, 43, 330, 20), 12, NSFontWeightRegular);
    self.status.textColor = NSColor.secondaryLabelColor;
    [content addSubview:self.status];

    NSButton *apply = [NSButton buttonWithTitle:@"Применить" target:self action:@selector(applySettings:)];
    apply.frame = NSMakeRect(367, 28, 105, 32);
    apply.bezelStyle = NSBezelStyleRounded;
    [content addSubview:apply];
    NSButton *close = [NSButton buttonWithTitle:@"Закрыть" target:self action:@selector(closePanel:)];
    close.frame = NSMakeRect(480, 28, 84, 32);
    close.bezelStyle = NSBezelStyleRounded;
    [content addSubview:close];

    [self.turnSlider setTarget:self]; [self.turnSlider setAction:@selector(sliderChanged:)];
    [self.aimSlider setTarget:self]; [self.aimSlider setAction:@selector(sliderChanged:)];
    [self updateValueLabels];
    [self.window center];
    if (show) {
        [self.window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
    }
}

- (BOOL)renderPreviewAtPath:(NSString *)path {
    self.canApply = NO;
    [self buildWindowWithSettings:HogsMousePanelDefaults() show:NO];
    NSView *view = self.window.contentView;
    self.window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    view.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    for (NSView *subview in view.subviews) {
        subview.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
        [subview setNeedsDisplay:YES];
    }
    [view displayIfNeeded];
    [self.window display];
    return [self writePreviewOfView:view path:path];
}

- (BOOL)renderAlertPreviewAtPath:(NSString *)path saving:(BOOL)saving {
    NSAlert *alert = PanelAlert(saving, saving
        ? @"Не удалось проверить сохранённые настройки. Проверьте файл."
        : nil);
    [alert layout];
    NSWindow *window = alert.window;
    window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    window.opaque = YES;
    window.backgroundColor = [NSColor colorWithWhite:0.95 alpha:1.0];
    NSView *view = window.contentView;
    view.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    [view displayIfNeeded];
    [window display];
    return [self writePreviewOfView:view path:path];
}

- (BOOL)writePreviewOfView:(NSView *)view path:(NSString *)path {
    NSBitmapImageRep *rep = [view bitmapImageRepForCachingDisplayInRect:view.bounds];
    if (!rep) return NO;
    [view cacheDisplayInRect:view.bounds toBitmapImageRep:rep];
    NSData *png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    return [png writeToFile:path options:NSDataWritingAtomic error:nil];
}

- (NSSlider *)sliderWithFrame:(NSRect)frame minimum:(double)minimum maximum:(double)maximum {
    NSSlider *slider = [[NSSlider alloc] initWithFrame:frame];
    slider.minValue = minimum;
    slider.maxValue = maximum;
    slider.numberOfTickMarks = 24;
    slider.tickMarkPosition = NSTickMarkPositionBelow;
    slider.allowsTickMarkValuesOnly = YES;
    slider.continuous = NO;
    return slider;
}

- (void)sliderChanged:(id)sender { (void)sender; [self updateValueLabels]; }

- (void)updateValueLabels {
    self.turnValue.stringValue = [NSString stringWithFormat:@"%.2g×", self.turnSlider.doubleValue];
    self.aimValue.stringValue = [NSString stringWithFormat:@"%.2g×", self.aimSlider.doubleValue];
}

- (void)applySettings:(id)sender {
    (void)sender;
    if (!self.canApply) return;
    HogsMousePanelSettings settings = {
        .nativeUnitsPerPixel = (NSInteger)llround(self.turnSlider.doubleValue * 4.0),
        .aimSensitivityPercent = (NSInteger)llround(self.aimSlider.doubleValue * 100.0),
        .invertY = self.invertY.state == NSControlStateValueOn,
        .invertX = self.invertX.state == NSControlStateValueOn
    };
    NSError *error = nil;
    if (!HogsMousePanelWrite(settings, &error)) {
        self.status.stringValue = @"Не удалось сохранить настройки.";
        NSAlert *alert = PanelAlert(YES, error.localizedDescription);
        [alert runModal];
        return;
    }
    self.status.stringValue = @"Настройки сохранены.";
}

- (void)closePanel:(id)sender { (void)sender; [NSApp terminate:nil]; }
- (BOOL)windowShouldClose:(NSWindow *)sender { (void)sender; [NSApp terminate:nil]; return YES; }

@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        if (argc == 2 && strcmp(argv[1], "--self-test") == 0) return HogsSettingsSelfTest();
        if (argc == 3 && strcmp(argv[1], "--render-preview") == 0) {
            [NSApplication sharedApplication];
            HogsPanelController *preview = [[HogsPanelController alloc] init];
            return [preview renderPreviewAtPath:[NSString stringWithUTF8String:argv[2]]] ? 0 : 1;
        }
        if (argc == 4 && strcmp(argv[1], "--render-alert") == 0) {
            [NSApplication sharedApplication];
            HogsPanelController *preview = [[HogsPanelController alloc] init];
            BOOL saving = strcmp(argv[2], "save") == 0;
            if (!saving && strcmp(argv[2], "open") != 0) return 2;
            return [preview renderAlertPreviewAtPath:[NSString stringWithUTF8String:argv[3]] saving:saving] ? 0 : 1;
        }
        if (argc == 2 && strcmp(argv[1], "--help") == 0) {
            puts("HogsMouse Settings Panel\n  --self-test   run isolated temporary-file checks\n  --help        show this message");
            return 0;
        }
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        HogsPanelController *delegate = [[HogsPanelController alloc] init];
        app.delegate = delegate;
        [app run];
    }
    return 0;
}
