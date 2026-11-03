#ifndef HOGSMOUSE_MESSAGE_PUMP_LOGIC_H
#define HOGSMOUSE_MESSAGE_PUMP_LOGIC_H
#include <stdint.h>
enum { HM_QUEUE_PEEK = 1, HM_QUEUE_GET = 2 };
/* Do not inspect a PeekMessage result until PM_REMOVE removed it. GetMessage
   removes on return and is valid only for a positive result. Scope additionally
   to the live game HWND and its owning thread. */
static inline int hm_should_inspect_removed_message(int api, int result, unsigned remove_flags,
                                                     uintptr_t msg_hwnd, uintptr_t game_hwnd,
                                                     uint32_t current_thread, uint32_t game_thread) {
    if (api == HM_QUEUE_PEEK) {
        if (!result || !(remove_flags & 1u)) return 0; /* PM_REMOVE */
    } else if (api == HM_QUEUE_GET) {
        if (result <= 0) return 0; /* 0 = WM_QUIT, -1 = error */
    } else return 0;
    return msg_hwnd && msg_hwnd == game_hwnd && current_thread && current_thread == game_thread;
}
#endif
