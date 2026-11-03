#ifndef HOGS_SETTINGS_STORE_H
#define HOGS_SETTINGS_STORE_H

#import <Foundation/Foundation.h>

typedef struct {
    NSInteger nativeUnitsPerPixel;   /* 1..24, default 4 */
    NSInteger aimSensitivityPercent; /* 25..600, default 100 */
    BOOL invertY;
    BOOL invertX;
} HogsMousePanelSettings;

FOUNDATION_EXPORT HogsMousePanelSettings HogsMousePanelDefaults(void);
FOUNDATION_EXPORT NSString *HogsMousePanelProductionGameRoot(void);
FOUNDATION_EXPORT BOOL HogsMousePanelRead(HogsMousePanelSettings *settings,
                                           NSError **error);
FOUNDATION_EXPORT BOOL HogsMousePanelWrite(HogsMousePanelSettings settings,
                                            NSError **error);

/* Used only by the temporary-directory CLI self-test. */
FOUNDATION_EXPORT BOOL HogsMousePanelReadForTest(NSString *gameRoot,
                                                  HogsMousePanelSettings *settings,
                                                  NSError **error);
FOUNDATION_EXPORT BOOL HogsMousePanelWriteForTest(NSString *gameRoot,
                                                   HogsMousePanelSettings settings,
                                                   NSInteger failureStage,
                                                   NSError **error);

#endif
