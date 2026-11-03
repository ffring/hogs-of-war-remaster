#ifndef HOGS_NATIVE_INPUT_POLICY_H
#define HOGS_NATIVE_INPUT_POLICY_H
#include <stdint.h>

enum { HN_NONE = 0, HN_BODY = 1, HN_AIM = 2 };

/* These are input-axis values, not angles or writes into a pig/camera. The
   original human input record synchronizes them before simulation consumes it. */
static inline int hn_axis(int64_t pixels, int gain) {
    if (gain < 1) gain = 1;
    if (gain > 8) gain = 8;
    int64_t value = (int64_t)pixels * gain;
    return value < -120 ? -120 : value > 120 ? 120 : (int)value;
}

typedef struct {
    uint32_t user, at, serial;
    int route, x, y;
    int ready;
} HNPacket;

static inline void hn_clear(HNPacket *p) {
    p->user = p->at = 0;
    p->route = p->x = p->y = p->ready = 0;
}

static inline void hn_stage(HNPacket *p, uint32_t user, uint32_t now,
                            int route, int dx, int dy, int gain, int invert_y) {
    p->serial++;
    p->user = user; p->at = now; p->route = route;
    p->x = route ? hn_axis(dx, gain) : 0;
    /* In walking mode the Y joystick axis is forward/backward movement.
       Mouse Y must never become walking input. Free vertical view is separate. */
    p->y = route == HN_AIM ? hn_axis(invert_y ? -(int64_t)dy : (int64_t)dy, gain) : 0;
    p->ready = route != HN_NONE && (p->x || p->y);
}

/* Consume exactly once, including rejected frames. Do not overwrite joystick
   input. Preserve every action bit while converting keyboard walk directions
   only when a body-turn axis must coexist with forward/backward walking. */
static inline int hn_take(HNPacket *p, uint32_t user, uint32_t now,
                          uint32_t original_axes, uint32_t mask,
                          uint32_t *axes_out, uint32_t *mask_out) {
    HNPacket pending = *p;
    p->ready = 0;
    if (!pending.ready || pending.user != user ||
        (uint32_t)(now - pending.at) > 100 || original_axes) return 0;
    int x = pending.x, y = pending.y;
    if (pending.route == HN_BODY) {
        unsigned lr = mask & 12, ud = mask & 3;
        if (lr) x = lr == 4 ? -64 : lr == 8 ? 64 : 0;
        /* Executing the original dispatch proves negative Y means Up:
           its two sign conversions lead to native +64 for human/queue.
           +/-126 fits int8 safely; type 2 has proportional walking. */
        y = ud == 1 ? -126 : ud == 2 ? 126 : 0;
        mask &= ~15u;
    } else if (mask & 15) {
        /* Explicit keyboard aim wins over concurrent mouse aim. */
        return 0;
    }
    *axes_out = (uint32_t)(uint8_t)(int8_t)x |
                ((uint32_t)(uint8_t)(int8_t)y << 8);
    *mask_out = mask;
    return 1;
}
#endif
