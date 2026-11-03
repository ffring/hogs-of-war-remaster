#ifndef HOGSMOUSE_AIM_SCALE_H
#define HOGSMOUSE_AIM_SCALE_H

#include <stdint.h>

enum {
    HM_AIM_SCALE_DEFAULT_PERCENT = 300,
    HM_AIM_SCALE_MAX_PERCENT = 1000,
    HM_AIM_SCALE_MAX_FRAME_DELTA = 4096
};

typedef struct {
    int32_t fraction_x100_x;
    int32_t fraction_x100_y;
    int32_t percent;
} HMAimScale;

static inline int32_t hm_aim_scale_normalize_percent(int32_t percent) {
    if (percent <= 0) return HM_AIM_SCALE_DEFAULT_PERCENT;
    if (percent > HM_AIM_SCALE_MAX_PERCENT) return HM_AIM_SCALE_MAX_PERCENT;
    return percent;
}

static inline void hm_aim_scale_init(HMAimScale *scale, int32_t percent) {
    if (!scale) return;
    scale->fraction_x100_x = 0;
    scale->fraction_x100_y = 0;
    scale->percent = hm_aim_scale_normalize_percent(percent);
}

/* Clear fractional history on focus loss, UI transitions, turn changes, or
   any other lifecycle reset. Keep the selected sensitivity. */
static inline void hm_aim_scale_clear(HMAimScale *scale) {
    if (!scale) return;
    scale->fraction_x100_x = 0;
    scale->fraction_x100_y = 0;
}

static inline int32_t hm_aim_scale_axis(int32_t input, int32_t *fraction_x100,
                                        int32_t percent) {
    if (!fraction_x100) return 0;

    /* A reversal discards only the old fractional sliver, never whole input. */
    if ((input > 0 && *fraction_x100 < 0) ||
        (input < 0 && *fraction_x100 > 0))
        *fraction_x100 = 0;

    /* int32 input times the bounded percent fits comfortably in int64. */
    const int64_t total = (int64_t)input * (int64_t)percent + *fraction_x100;
    const int64_t whole = total / 100;
    const int64_t fraction = total % 100;
    if (whole > HM_AIM_SCALE_MAX_FRAME_DELTA) {
        *fraction_x100 = 0; /* Saturation must never queue future motion. */
        return HM_AIM_SCALE_MAX_FRAME_DELTA;
    }
    if (whole < -HM_AIM_SCALE_MAX_FRAME_DELTA) {
        *fraction_x100 = 0;
        return -HM_AIM_SCALE_MAX_FRAME_DELTA;
    }
    *fraction_x100 = (int32_t)fraction;
    return (int32_t)whole;
}

/* Scale one input frame. Only sub-pixel hundredths survive to the next frame;
   whole movement is returned immediately and capped at +/-4096 pixels. */
static inline int hm_aim_scale_frame(HMAimScale *scale, int32_t dx, int32_t dy,
                                     int32_t *scaled_dx, int32_t *scaled_dy) {
    if (!scale || !scaled_dx || !scaled_dy) return 0;
    scale->percent = hm_aim_scale_normalize_percent(scale->percent);
    int32_t x = hm_aim_scale_axis(dx, &scale->fraction_x100_x, scale->percent);
    int32_t y = hm_aim_scale_axis(dy, &scale->fraction_x100_y, scale->percent);
    *scaled_dx = x;
    *scaled_dy = y;
    return 1;
}

#endif
