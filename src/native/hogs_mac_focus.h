#ifndef HOGS_MAC_FOCUS_H
#define HOGS_MAC_FOCUS_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns the PID of the macOS frontmost application, or -1 if unavailable. */
pid_t hogs_mac_frontmost_pid(void);

#ifdef __cplusplus
}
#endif

#endif
