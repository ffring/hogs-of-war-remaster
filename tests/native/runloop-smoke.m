#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#include "focus_runloop_wait.h"
#include <stdio.h>

static unsigned fired;
static void timer_fired(CFRunLoopTimerRef timer, void *info)
{
    (void)timer; (void)info; ++fired;
}

int main(void)
{
    CFRunLoopTimerContext context = {0, NULL, NULL, NULL, NULL};
    CFRunLoopTimerRef timer = CFRunLoopTimerCreate(
        kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + 0.005, 0.01,
        0, 0, timer_fired, &context);
    if (!timer) return 2;
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);
    for (unsigned i = 0; i < 4; ++i) hogs_focus_runloop_wait_50ms();
    CFRunLoopTimerInvalidate(timer);
    CFRelease(timer);
    printf("timer_callbacks=%u\n", fired);
    return fired >= 4 ? 0 : 1;
}
