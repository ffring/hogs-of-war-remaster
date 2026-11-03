#import <AppKit/AppKit.h>
#include "hogs_mac_focus.h"

pid_t hogs_mac_frontmost_pid(void)
{
    @autoreleasepool {
        NSRunningApplication *frontmost = NSWorkspace.sharedWorkspace.frontmostApplication;
        if (frontmost == nil) return (pid_t)-1;
        return frontmost.processIdentifier;
    }
}

#ifdef HOGS_MAC_FOCUS_QUERY
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    char *end = NULL;
    long parsed;
    pid_t frontmost;

    if (argc != 2) {
        fprintf(stderr, "usage: %s target-pid\n", argv[0]);
        return 2;
    }
    errno = 0;
    parsed = strtol(argv[1], &end, 10);
    if (errno || end == argv[1] || *end != '\0' || parsed <= 0 || parsed > INT_MAX) {
        fprintf(stderr, "invalid pid\n");
        return 2;
    }

    frontmost = hogs_mac_frontmost_pid();
    if (frontmost <= 0) {
        puts("frontmost=unavailable");
        return 1;
    }
    printf("frontmost=%d front_pid=%d target_pid=%ld\n",
           frontmost == (pid_t)parsed, (int)frontmost, parsed);
    return 0;
}
#endif
