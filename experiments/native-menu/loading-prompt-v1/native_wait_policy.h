#ifndef HOGS_NATIVE_WAIT_POLICY_H
#define HOGS_NATIVE_WAIT_POLICY_H

/* Pure policy for an already-active, original-game conditional wait screen.
 * It does not activate or force that screen. */
typedef struct HogsWaitInputPolicy {
    unsigned active;
    unsigned armed;
} HogsWaitInputPolicy;

static inline void hogs_wait_policy_enter(HogsWaitInputPolicy *p) {
    if (!p) return;
    p->active = 1;
    p->armed = 0;
}

static inline void hogs_wait_policy_leave(HogsWaitInputPolicy *p) {
    if (!p) return;
    p->active = 0;
    p->armed = 0;
}

/* Returns whether the original wait routine may receive this input result.
 * Unknown policy states fail closed. Outside the conditional wait, pass through. */
static inline int hogs_wait_policy_filter(HogsWaitInputPolicy *p, int native_pressed) {
    if (!p || (native_pressed != 0 && native_pressed != 1)) return 0;
    if (p->active > 1 || p->armed > 1) return 0;
    if (!p->active) {
        if (p->armed) return 0;
        return native_pressed;
    }
    if (!p->armed) {
        if (!native_pressed) p->armed = 1;
        return 0;
    }
    return native_pressed;
}

#endif
