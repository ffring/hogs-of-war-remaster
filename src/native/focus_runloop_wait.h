#ifndef HOGS_FOCUS_RUNLOOP_WAIT_H
#define HOGS_FOCUS_RUNLOOP_WAIT_H

#include <CoreFoundation/CoreFoundation.h>
#include <errno.h>
#include <time.h>

static void hogs_focus_wait_deadline(CFRunLoopTimerRef timer, void *info)
{
    (void)timer;
    (void)info;
}

/* Service Cocoa's default run loop for a real bounded interval. The one-shot
   timer keeps the mode alive even before AppKit has registered any sources. */
static inline void hogs_focus_runloop_wait_50ms(void)
{
    CFRunLoopTimerContext context = {0, NULL, NULL, NULL, NULL};
    CFAbsoluteTime deadline = CFAbsoluteTimeGetCurrent() + 0.05;
    CFRunLoopTimerRef timer = CFRunLoopTimerCreate(
        kCFAllocatorDefault, deadline, 0.0, 0, 0,
        hogs_focus_wait_deadline, &context);
    if (!timer) {
        struct timespec delay = {0, 50000000};
        while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {}
        return;
    }
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, false);
    CFRunLoopTimerInvalidate(timer);
    CFRelease(timer);
}

#endif
