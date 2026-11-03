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

/* Free-aim mode proved too easy to leave active accidentally. Keep reading
   the old INI key for compatibility, but always require the RMB gesture. */
static inline int hm_normalize_require_rmb(int configured) {
    (void)configured;
    return 1;
}

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

/* Convert one selected input-frame delta into at most one key pulse per axis.
   Only a signed sub-threshold remainder survives. A reversal discards the
   fraction accumulated in the old direction before considering this frame. */
static inline int hm_aim_axis_pulse(int32_t frame_delta, int32_t *remainder,
                                    int sensitivity) {
    if (sensitivity < 1) sensitivity = 1;
    if (frame_delta > 0 && *remainder < 0) *remainder = 0;
    else if (frame_delta < 0 && *remainder > 0) *remainder = 0;

    int64_t total = (int64_t)frame_delta + *remainder;
    if (total >= sensitivity) {
        *remainder = (int32_t)(total % sensitivity);
        return 1;
    }
    if (total <= -sensitivity) {
        *remainder = (int32_t)(total % sensitivity);
        return -1;
    }
    *remainder = (int32_t)total;
    return 0;
}

static inline uint16_t hm_read_aim(HMState *s, uint32_t now_ms, int sensitivity,
                                   int invert_y, int require_right_button,
                                   int right_button_held) {
    uint16_t out = 0;
    if (s->last_motion_ms && (uint32_t)(now_ms - s->last_motion_ms) > 100)
        s->dx = s->dy = s->remainder_dx = s->remainder_dy = 0;
    if (require_right_button && !right_button_held) {
        s->dx = s->dy = s->remainder_dx = s->remainder_dy = 0;
        return 0;
    }
    /* L1 is the game's hold-to-aim/camera modifier, so it stays down for
       every valid read in the active gesture, including a stationary frame. */
    out |= HM_L1;
    int32_t frame_dx = s->dx, frame_dy = s->dy;
    s->dx = s->dy = 0; /* Never carry whole directional steps across reads. */
    int x = hm_aim_axis_pulse(frame_dx, &s->remainder_dx, sensitivity);
    int y = hm_aim_axis_pulse(frame_dy, &s->remainder_dy, sensitivity);
    if (x > 0) out |= HM_RIGHT;
    else if (x < 0) out |= HM_LEFT;
    if (y > 0) out |= invert_y ? HM_UP : HM_DOWN;
    else if (y < 0) out |= invert_y ? HM_DOWN : HM_UP;
    return out;
}

#endif
