#ifndef HOGS_MOUSE_BUTTON_STATE_H
#define HOGS_MOUSE_BUTTON_STATE_H

#include <stdint.h>
#include "mouse_logic.h"

typedef struct {
    uint8_t known;                 /* buttons governed by actual input events */
    uint8_t held;                  /* event state, or poll fallback for unknown buttons */
    uint8_t pressed;               /* latched LMB edge, consumed by next battle read */
    uint8_t require_event_release; /* previously event-held across focus/UI reset */
} HMButtonState;

static inline void hm_button_state_clear(HMButtonState *s) {
    s->known = s->held = s->pressed = s->require_event_release = 0;
}

static inline void hm_button_event(HMButtonState *s, uint8_t button, int down) {
    if (button != HM_MOUSE_LEFT && button != HM_MOUSE_RIGHT) return;
    s->known |= button;
    if (down) {
        if (button == HM_MOUSE_LEFT && !(s->held & button)) s->pressed |= button;
        s->held |= button;
    } else {
        s->held &= (uint8_t)~button;
        s->require_event_release &= (uint8_t)~button;
    }
}

/* Physical polling is a fallback only for a button for which no event has
   established authority. It must not erase a Raw/legacy held event. */
static inline void hm_button_poll(HMButtonState *s, uint8_t physical) {
    physical &= HM_MOUSE_LEFT | HM_MOUSE_RIGHT;
    for (uint8_t bit = HM_MOUSE_LEFT; bit <= HM_MOUSE_RIGHT; bit <<= 1) {
        if ((s->known | s->require_event_release) & bit) continue;
        int was_down = (s->held & bit) != 0;
        int is_down = (physical & bit) != 0;
        if (bit == HM_MOUSE_LEFT && is_down && !was_down) s->pressed |= bit;
        if (is_down) s->held |= bit;
        else s->held &= (uint8_t)~bit;
    }
}

/* On focus/UI/Escape reset, event-held buttons need their actual UP event
   before rearming; async polling cannot contradict a received DOWN. */
static inline void hm_button_disarm(HMButtonState *s) {
    s->require_event_release |= s->known & s->held;
    s->known = 0;
    s->held = 0;
    s->pressed = 0;
}

static inline int hm_button_state_released(const HMButtonState *s) {
    return s->held == 0 && s->require_event_release == 0;
}

static inline uint16_t hm_button_state_take_fire(HMButtonState *s) {
    uint16_t result = ((s->held | s->pressed) & HM_MOUSE_LEFT) ? HM_CROSS : 0;
    s->pressed = 0;
    return result;
}

static inline uint16_t hm_read_aim(HMState *s, uint32_t now_ms, int sensitivity,
                                   int invert_y, int require_right_button,
                                   int right_button_held) {
    uint16_t out = 0;
    if (s->last_motion_ms && (uint32_t)(now_ms - s->last_motion_ms) > 100)
        s->dx = s->dy = 0;
    if (require_right_button && !right_button_held) {
        s->dx = s->dy = 0;
        return 0;
    }
    if (s->dx >= sensitivity) { out |= HM_RIGHT; s->dx -= sensitivity; }
    else if (s->dx <= -sensitivity) { out |= HM_LEFT; s->dx += sensitivity; }
    if (s->dy >= sensitivity) { out |= invert_y ? HM_UP : HM_DOWN; s->dy -= sensitivity; }
    else if (s->dy <= -sensitivity) { out |= invert_y ? HM_DOWN : HM_UP; s->dy += sensitivity; }
    if (out & (HM_UP | HM_DOWN | HM_LEFT | HM_RIGHT)) out |= HM_L1;
    return out;
}

#endif
