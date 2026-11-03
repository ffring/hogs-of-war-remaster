#ifndef HOGS_MOUSE_POLL_LOGIC_H
#define HOGS_MOUSE_POLL_LOGIC_H

#include <stdint.h>
#include "mouse_logic.h"

enum {
    HM_POLL_NONE = 0,
    HM_POLL_BASELINE = 1,
    HM_POLL_MOTION = 2,
    HM_POLL_WARP_REJECTED = 3,
    HM_POLL_SAMPLE_FAILED = 4
};

typedef struct {
    HMState mouse;
    int32_t x, y;
    uint8_t baseline_valid;
    uint8_t sample_valid;
} HMPollState;

static inline void hm_poll_reset(HMPollState *p) {
    p->x = p->y = 0;
    p->baseline_valid = 0;
    p->sample_valid = 0;
    hm_clear(&p->mouse);
}

/* Consume one real cursor/button sample. Coordinates are screen pixels. A
   large jump updates the baseline but rejects that frame's movement. */
static inline int hm_poll_update(HMPollState *p, int sample_ok, int32_t x, int32_t y,
                                 uint8_t buttons, uint32_t now_ms, int armed,
                                 int require_right_button, int max_cursor_delta) {
    if (!sample_ok) {
        hm_poll_reset(p);
        return HM_POLL_SAMPLE_FAILED;
    }

    buttons &= (HM_MOUSE_LEFT | HM_MOUSE_RIGHT);
    if (!p->baseline_valid) {
        p->x = x;
        p->y = y;
        p->baseline_valid = 1;
        p->sample_valid = 1;
        hm_clear(&p->mouse);
        if (armed) hm_buttons(&p->mouse, buttons);
        return HM_POLL_BASELINE;
    }

    int32_t dx = x - p->x;
    int32_t dy = y - p->y;
    p->x = x;
    p->y = y;
    p->sample_valid = 1;

    if (!armed) {
        hm_clear(&p->mouse);
        return HM_POLL_BASELINE;
    }

    hm_buttons(&p->mouse, buttons);
    if (dx > max_cursor_delta || dx < -max_cursor_delta ||
        dy > max_cursor_delta || dy < -max_cursor_delta) {
        p->mouse.dx = p->mouse.dy = 0;
        return HM_POLL_WARP_REJECTED;
    }
    if ((dx || dy) && (!require_right_button || (p->mouse.held & HM_MOUSE_RIGHT))) {
        hm_motion(&p->mouse, dx, dy, now_ms);
        return HM_POLL_MOTION;
    }
    return HM_POLL_NONE;
}

/* Single-source arbitration. Fresh Raw Input wins; fresh legacy events are
   next; otherwise the callback-polled physical state is the fallback. */
static inline int hm_poll_choose_source(int raw_registered, int raw_seen,
                                        uint32_t raw_ms, int legacy_active,
                                        uint32_t legacy_ms, int poll_valid,
                                        uint32_t now_ms) {
    if (hm_raw_recent(raw_registered, raw_seen, raw_ms, now_ms)) return HM_SOURCE_RAW;
    if (legacy_active && (uint32_t)(now_ms - legacy_ms) <= 250) return HM_SOURCE_LEGACY;
    if (poll_valid) return HM_SOURCE_POLL;
    if (raw_seen) return HM_SOURCE_RAW;
    return legacy_active ? HM_SOURCE_LEGACY : HM_SOURCE_NONE;
}

static inline void hm_poll_discard_if_unselected(HMPollState *p, int selected_source) {
    if (selected_source != HM_SOURCE_POLL) p->mouse.dx = p->mouse.dy = 0;
}

#endif
