#ifndef HOGSMOUSE_LOGIC_H
#define HOGSMOUSE_LOGIC_H

#include <stdint.h>

enum {
    HM_UP = 0x0001, HM_DOWN = 0x0002, HM_LEFT = 0x0004, HM_RIGHT = 0x0008,
    HM_CROSS = 0x0020, HM_L1 = 0x0100,
    HM_MOUSE_LEFT = 0x01, HM_MOUSE_RIGHT = 0x02
};

enum { HM_SOURCE_NONE = 0, HM_SOURCE_RAW = 1, HM_SOURCE_LEGACY = 2, HM_SOURCE_POLL = 3 };
enum { HM_WINDOW_KEEP = 0, HM_WINDOW_REBIND = 1, HM_WINDOW_FAIL = 2 };

/* Pure decision core for the HWND monitor; uintptr_t keeps the helper usable
   in the native (non-Windows) logic tests. */
static inline int hm_window_action(uintptr_t cached, uintptr_t candidate,
                                   int old_valid, int old_is_ours, int old_is_saved,
                                   int old_is_known_upper) {
    if (cached == candidate) {
        if (!cached) return HM_WINDOW_KEEP;
        if (!old_valid) return HM_WINDOW_REBIND;
        if (old_is_ours) return HM_WINDOW_KEEP;
        if (old_is_saved) return HM_WINDOW_REBIND;
        if (old_is_known_upper) return HM_WINDOW_KEEP;
        return HM_WINDOW_FAIL;
    }
    if (cached && old_valid && !old_is_ours && !old_is_saved) return HM_WINDOW_FAIL;
    return HM_WINDOW_REBIND;
}

/* Prefer recent Raw Input; fall back only after it has been absent for 250 ms.
   Unsigned subtraction makes the age comparison safe across tick wraparound. */
static inline int hm_raw_recent(int registered, int seen, uint32_t last_raw_ms,
                                uint32_t now_ms) {
    return registered && seen && (uint32_t)(now_ms - last_raw_ms) <= 250;
}

static inline int hm_choose_source(int registered, int seen, uint32_t last_raw_ms,
                                   uint32_t now_ms, int legacy_active) {
    if (registered && seen && (!legacy_active || hm_raw_recent(registered, seen, last_raw_ms, now_ms)))
        return HM_SOURCE_RAW;
    return legacy_active ? HM_SOURCE_LEGACY : HM_SOURCE_NONE;
}

typedef struct {
    int32_t dx, dy;
    int32_t remainder_dx, remainder_dy;
    uint32_t last_motion_ms;
    uint8_t held, pressed;
} HMState;

static inline int32_t hm_clamp_delta(int64_t n) {
    return n < -256 ? -256 : (n > 256 ? 256 : n);
}

static inline void hm_clear(HMState *s) {
    s->dx = s->dy = s->remainder_dx = s->remainder_dy = 0;
    s->held = 0;
    s->pressed = 0;
    s->last_motion_ms = 0;
}

static inline void hm_buttons(HMState *s, uint8_t held) {
    held &= (HM_MOUSE_LEFT | HM_MOUSE_RIGHT);
    if ((held & HM_MOUSE_LEFT) && !(s->held & HM_MOUSE_LEFT)) s->pressed |= HM_MOUSE_LEFT;
    s->held = held;
}

static inline void hm_button_down(HMState *s, uint8_t button) {
    if (button == HM_MOUSE_LEFT && !(s->held & button)) s->pressed |= button;
    s->held |= button & (HM_MOUSE_LEFT | HM_MOUSE_RIGHT);
}

static inline void hm_button_up(HMState *s, uint8_t button) {
    s->held &= (uint8_t)~(button & (HM_MOUSE_LEFT | HM_MOUSE_RIGHT));
}

static inline void hm_motion(HMState *s, int32_t dx, int32_t dy, uint32_t now_ms) {
    s->dx = hm_clamp_delta((int64_t)s->dx + dx);
    s->dy = hm_clamp_delta((int64_t)s->dy + dy);
    s->last_motion_ms = now_ms;
}

/* Validate the entire requested mapping before touching the DirectInput image. */
static inline int hm_apply_bindings(uint8_t *dik, const uint16_t bindings[16], uint16_t mask) {
    if (!dik || !bindings) return 0;
    for (int i = 0; i < 16; ++i)
        if ((mask & (1u << i)) && (!bindings[i] || bindings[i] >= 256)) return 0;
    for (int i = 0; i < 16; ++i)
        if (mask & (1u << i)) dik[bindings[i]] = 0x80;
    return mask != 0;
}

#endif
